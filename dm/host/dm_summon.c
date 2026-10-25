// 召唤控制台（界面见 dm/ios/Sources/dm_summon_panel.m）：
//   召唤：在玩家身边刷首领，再交给原版「束缚恶魔」的转化函数变成玩家的随从（跟随、打怪、不打玩家），死了不掉落；一键清除只清我们刷的。
//   通用修改：升级 / 升满、金币加满、加属性点 / 技能点、补满、无敌、无限法力耐力。全部是服务端写属性，同 diag.c 的测试开关 DM_TEST_STAT / DM_TEST_GOLD。
// 线程：界面线程只往原子量里投请求，刷怪、转化、写属性都在服务端拍里做（同首领连战：原版这些函数只许服务端线程调）。
// 依据：
//   刷怪 0x7ce630(游戏, 房间, 1, 类号, x, y, 1, 0, 0x10)、找空地 0x7160d0，同首领连战（dm_bossrush.c）；
//   转化 0x7aa190(游戏, 主人, 目标, 技能号, …)：原版束缚恶魔服务端那一下（调用点 0x7024c4，技能 382），参数见 to_minion；
//   清除 0x6c0c30(游戏, 主人, 单位 id, 1)：原版解除随从；先给单位 +0x124 置 bit 17（原版给召唤物的「不掉落」）——同秘境清怪（dm_rift.c）；
//   属性：写 0x5bc1f0(单位, 属性, 值, 0)、读 0x5b9290；4 属性点、5 技能点、6/7 生命、8/9 法力、10/11 耐力（这三对 ×256）、12 等级、13 经验、14 身上金币。
// Summon console (UI in dm/ios/Sources/dm_summon_panel.m):
//   Summon: spawn a boss next to the player, then hand it to the original game's Bind Demon conversion so it becomes the player's minion (follows, fights monsters, does not hit the player); it drops nothing on death; clear-all only clears what we spawned.
//   General cheats: level up / level 99, max gold, add stat / skill points, refill, god mode, infinite mana and stamina. All are server-side stat writes, same as diag.c's test switches DM_TEST_STAT / DM_TEST_GOLD.
// Threads: the UI thread only posts requests into atomics; spawning, conversion and stat writes all happen in the server tick (same as the boss rush: these original functions may only be called from the server thread).
// Basis:
//   spawn 0x7ce630(game, room, 1, class id, x, y, 1, 0, 0x10), find open ground 0x7160d0, same as the boss rush (dm_bossrush.c);
//   conversion 0x7aa190(game, owner, target, skill id, ...): the original Bind Demon server step (call site 0x7024c4, skill 382), parameters see to_minion;
//   clear 0x6c0c30(game, owner, unit id, 1): the original dismiss-minion; first set bit 17 of unit +0x124 (the original "no drop" for summons) -- same as the rift monster sweep (dm_rift.c);
//   stats: set 0x5bc1f0(unit, stat, value, 0), get 0x5b9290; 4 stat points, 5 skill points, 6/7 life, 8/9 mana, 10/11 stamina (these three pairs ×256), 12 level, 13 experience, 14 gold on person.
#include "dm_summon.h"
#include "dm_bossrush.h"
#include "dm_unit.h"
#include "hle.h"
#include "dm.h"
#include "dm_i18n.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

const char *dm_save_commitdir(void);   // hle_fs.c

static const struct { int cls; const char *key; } k_boss[DM_SUM_NBOSS] = {
    { 156, "dmBossAndariel" }, { 211, "dmBossDuriel" }, { 242, "dmBossMephisto" }, { 243, "dmBossDiablo" }, { 544, "dmBossBaal" } };   // monstats 行号同首领连战；名字是串表键（dm/i18n/ui_text.tsv） | monstats rows same as the boss rush; names are string table keys (dm/i18n/ui_text.tsv)
#define MAX_KEEP 20                     // 同时在场的召唤首领上限：再多画面和性能都扛不住 | cap on summoned bosses alive at once: more than this is too much for the screen and performance
// 可调参数：存档目录旁的 dm_summon.txt（iOS 在「文件」App 的 D2R Mobile 文件夹里，同 sw_custom.txt），一行一个「键=整数」，# 起是注释。
// 每次召唤前重读，改完文件下一次召唤就生效，不用重新构建；没有文件就按缺省写一份出来给玩家改
// Tunables: dm_summon.txt next to the save directory (on iOS in the D2R Mobile folder of the Files app, same as sw_custom.txt), one "key=integer" per line, # starts a comment.
// Re-read before every summon, so an edited file takes effect on the next summon without a rebuild; if there is no file, one with the defaults is written out for the player to edit
static struct { int bind, restore, hp_pct, level, init, skill, multi; } C = { 1, 1, 100, 20, 1, 382, 1 };
static const struct { const char *key; int *v, lo, hi; const char *note; } k_cfg[] = {
    { "bind", &C.bind, 0, 1, "1 = 刷出后转成随从；0 = 不转（敌对，测试用）| 1 = bind as minion after spawning; 0 = leave hostile (testing)" },
    { "restore", &C.restore, 0, 1, "1 = 转化后把等级和生命上限改回刷出时的值 | 1 = restore spawn-time level and max life after binding" },
    { "hp_pct", &C.hp_pct, 10, 10000, "改回时的生命上限百分比（100 = 原样，300 = 三倍）| max life percent when restoring (100 = as spawned, 300 = triple)" },
    { "bind_level", &C.level, 1, 99, "转化用的技能等级（影响随从加成与数量上限）| skill level used for binding (affects minion bonuses and the count cap)" },
    { "bind_init", &C.init, 0, 1, "1 = 转化走原版施法的初始化（修饰、压等级）；0 = 跳过 | 1 = run the original cast init (mods, level cut); 0 = skip" },
    { "skill", &C.skill, 0, 1000, "转化用的技能号（382 = 束缚恶魔）| skill id used for binding (382 = Bind Demon)" },
    { "multi", &C.multi, 0, 1, "1 = 允许同时多只（借骷髅复生的数量算式）；0 = 原版一次只留一只 | 1 = allow several at once (borrows Raise Skeleton's count formula); 0 = original, one at a time" },
};
static void cfg_load(void) {
    const char *c = dm_save_commitdir(); if (!c) return; char p[1100]; snprintf(p, sizeof p, "%s/../dm_summon.txt", c);
    FILE *f = fopen(p, "r");
    if (!f) { if (!(f = fopen(p, "w"))) return;
        fprintf(f, "# 召唤控制台参数：改完保存，下一次召唤生效 | summon console tunables: save, takes effect on the next summon\n");
        for (size_t i = 0; i < sizeof k_cfg / sizeof *k_cfg; i++) fprintf(f, "# %s\n%s=%d\n", k_cfg[i].note, k_cfg[i].key, *k_cfg[i].v);
        fclose(f); LOG("召唤：写出缺省参数文件 %s", p); return; }
    char ln[256]; while (fgets(ln, sizeof ln, f)) { if (ln[0] == '#') continue; char k[32]; int v;
        if (sscanf(ln, " %31[a-z_] = %d", k, &v) != 2) continue;
        for (size_t i = 0; i < sizeof k_cfg / sizeof *k_cfg; i++) if (!strcmp(k, k_cfg[i].key)) *k_cfg[i].v = v < k_cfg[i].lo ? k_cfg[i].lo : v > k_cfg[i].hi ? k_cfg[i].hi : v; }
    fclose(f); LOG("召唤：参数 bind=%d restore=%d hp_pct=%d bind_level=%d bind_init=%d skill=%d multi=%d", C.bind, C.restore, C.hp_pct, C.level, C.init, C.skill, C.multi);
}

static _Atomic int g_req_boss = -1, g_req_n, g_req_clear, g_req_op, g_req_arg, g_god, g_infmp, g_alive, g_buffs;
static uint8_t *B;
static uint32_t g_ids[MAX_KEEP]; static int g_hp[MAX_KEEP], g_nids;   // 我们刷出来的单位 id 与上一拍的生命（×256；服务端线程独占） | unit ids we spawned and their life on the previous tick (×256; owned by the server thread)
static uint64_t g_G;                                  // 换局（游戏对象变了）就清名单 | a new run (the game object changed) clears the list

void dm_summon_request(int boss, int count) { if (boss < 0 || boss >= DM_SUM_NBOSS) return; if (count < 1) count = 1; if (count > 10) count = 10; atomic_store(&g_req_n, count); atomic_store(&g_req_boss, boss); }
void dm_summon_clear(void) { atomic_store(&g_req_clear, 1); }
void dm_cheat_request(int op, int arg) { atomic_store(&g_req_arg, arg); atomic_store(&g_req_op, op); }
void dm_cheat_set_god(int on) { atomic_store(&g_god, !!on); }
void dm_cheat_set_inf_mana(int on) { atomic_store(&g_infmp, !!on); }
int dm_cheat_god(void) { return atomic_load(&g_god); }
int dm_cheat_inf_mana(void) { return atomic_load(&g_infmp); }
int dm_summon_alive(void) { return atomic_load(&g_alive); }
// 属性开关：属性号与目标值都来自原版 itemstatcost.txt（Saved 列为空 = 不进存档）：80 掉宝率、79 金币掉落、85 经验加成（%）、
// 96 / 93 / 105 / 99 跑速 / 攻速 / 施法速度 / 打击恢复、25 伤害加成 % / 136 压碎性打击 / 141 致命攻击、39 / 41 / 43 / 45 火 / 电 / 冰 / 毒抗、127 全技能
// Stat toggles: stat ids and targets come from the original itemstatcost.txt (empty Saved column = not saved): 80 magic find, 79 gold find, 85 experience bonus (%),
// 96 / 93 / 105 / 99 run / attack / cast speed / hit recovery, 25 damage % / 136 crushing blow / 141 deadly strike, 39 / 41 / 43 / 45 fire / lightning / cold / poison resist, 127 all skills
static const struct { const char *key; int n; int16_t st[4], v[4]; } k_buff[DM_NBUFF] = {
    { "dmBuffMf", 1, { 80 }, { 500 } },
    { "dmBuffGf", 1, { 79 }, { 500 } },
    { "dmBuffExp", 1, { 85 }, { 100 } },
    { "dmBuffSpeed", 4, { 96, 93, 105, 99 }, { 100, 100, 100, 100 } },
    { "dmBuffDamage", 3, { 25, 136, 141 }, { 2000, 100, 100 } },
    { "dmBuffResist", 4, { 39, 41, 43, 45 }, { 100, 100, 100, 100 } },
    { "dmBuffSkills", 1, { 127 }, { 5 } },
};
void dm_cheat_set_buff(int b, int on) { if (b < 0 || b >= DM_NBUFF) return; if (on) atomic_fetch_or(&g_buffs, 1 << b); else atomic_fetch_and(&g_buffs, ~(1 << b)); }
int dm_cheat_buff(int b) { return b >= 0 && b < DM_NBUFF && (atomic_load(&g_buffs) >> b & 1); }
const char *dm_cheat_buff_key(int b) { return b >= 0 && b < DM_NBUFF ? k_buff[b].key : ""; }
const char *dm_summon_key(int boss) { return boss >= 0 && boss < DM_SUM_NBOSS ? k_boss[boss].key : ""; }

static uint64_t rd(uint64_t a) { return a ? *(volatile uint64_t *)(uintptr_t)a : 0; }
static uint64_t unit_room(uint64_t u) { uint64_t p = rd(u + 0x38); return p ? rd(p + 0x20) : 0; }   // 玩家 / 怪物：路径 +0x20 是房间 | players / monsters: path +0x20 is the room
static int unit_xy(uint64_t u, int *x, int *y) { uint64_t p = rd(u + 0x38); if (!p) return 0; *x = *(uint16_t *)(uintptr_t)(p + 2); *y = *(uint16_t *)(uintptr_t)(p + 6); return 1; }
static uint64_t find_unit(uint64_t G, uint32_t id) { for (int b = 0; b < 128; b++) for (uint64_t u = rd(G + 0x2230 + 0x400 + 8 * b); u; u = rd(u + 0x158)) if (*(uint32_t *)(uintptr_t)(u + 8) == id) return u; return 0; }
static int gs(uint64_t u, int st) { return ((int (*)(uint64_t, int, int))(B + 0x5b9290))(u, st, 0); }
static void ss(uint64_t u, int st, int v) { ((void (*)(uint64_t, int, int, int))(B + 0x5bc1f0))(u, st, v, 0); }
// 属性的基础项（同 dm_rift.c gsb：ss 写的是基础数组，读合计再写回会把加成乘两遍） | base entry of a stat (same as dm_rift.c gsb: ss writes the base array, so reading the total and writing it back multiplies bonuses twice)
static int gsb(uint64_t u, int st) { uint64_t sl = rd(u + 0x88); const uint32_t *a = sl ? (const uint32_t *)(uintptr_t)rd(sl + 0x30) : NULL; uint64_t n = sl ? rd(sl + 0x38) : 0;
    if (!a || n > 4096) return 0; for (uint64_t i = 0; i < n; i++) if (a[i * 2] == (uint32_t)st << 16) return (int)a[i * 2 + 1]; return 0; }
static uint64_t free_spot(uint64_t G, uint64_t room, int32_t pos[2]) { uint64_t out = 0; ((void (*)(uint64_t, uint64_t, int32_t *, int, int, uint64_t *, int, int))(B + 0x7160d0))(G, room, pos, 3, 0x400, &out, 4, 100); return out; }
static int alive(uint64_t u) { uint32_t m = *(uint32_t *)(uintptr_t)(u + 0xc); return m != 0 && m != 12; }   // 模式 0 正在死、12 已死 | mode 0 dying, 12 dead
static int in_town(int lv) { return lv == 1 || lv == 40 || lv == 75 || lv == 103 || lv == 109; }

// 把刚刷出的怪 u 变成 player 的随从：原版束缚恶魔的服务端转化 0x7aa190(游戏, 主人, 目标, 技能号, 技能等级, 关联编号, 初始化)，返回 bit 0 = 原版接受了。
//   技能等级：BoundStat 等算式的等级（原版取施法者技能等级，夹在 [0, *[数据表 + 0x1500]]）；关联编号：原版每次取 G+0x16c 加 1（到 INT_MAX 回到 1）写回再传；
//   初始化 = 1：按新束缚的目标走初始化块（怪物修饰、等级压到主人等级），原版施法那一路传 1（0x7ab940），读档重建那一路传 0（0x7024c4）。
//   技能记录 +0x112 是宠物类型（越界回落 21 = 束缚恶魔），由 0x6c1b00 挂进玩家宠物表
// Turn the freshly spawned monster u into player's minion: the original Bind Demon server conversion 0x7aa190(game, owner, target, skill id, skill level, link id, initialize); bit 0 of the return = accepted.
//   Skill level: the level used by formulas such as BoundStat (the original takes the caster's skill level, clamped to [0, *[data table + 0x1500]]); link id: each time the original takes G+0x16c plus 1 (wrapping to 1 at INT_MAX), writes it back and passes it;
//   initialize = 1: run the init block for a freshly bound target (monster mods, level brought down to the owner's level); the original cast path passes 1 (0x7ab940), the load-rebuild path passes 0 (0x7024c4).
//   Skill record +0x112 is the pet type (out of range falls back to 21 = bound demon); 0x6c1b00 hooks it into the player's pet table
// 宠物表：[[玩家+0x10]+0x98] 起每种宠物类型一个 0x20 字节槽：+0 链头（节点 +4 单位 id、+0x18 下一个）、+8 数量、+0xc 上限（每次束缚由技能记录 +0x114 的算式重算，超了原版裁掉最旧的）
// Pet table: from [[player+0x10]+0x98], one 0x20-byte slot per pet type: +0 chain head (node +4 unit id, +0x18 next), +8 count, +0xc cap (recomputed on every bind from the formula at skill record +0x114; above it the original trims the oldest)
static uint64_t skill_rec(uint64_t G, int skill, uint64_t *table) {   // 技能表记录（步长 0x2ec，同 dm_actions.c） | skill table record (stride 0x2ec, same as dm_actions.c)
    uint8_t ver = *(uint8_t *)(uintptr_t)(G + 0x106); if (ver < 1 || ver > 3 || skill < 0) return 0;
    uint64_t T = rd((uint64_t)(uintptr_t)(B + 0x2d64a18 + ver * 16)); if (!T || rd(T + 0x11f0) <= (uint64_t)skill) return 0;
    if (table) *table = T; return rd(T + 0x11e8) + (uint64_t)skill * 0x2ec;
}
static uint64_t pet_slot(uint64_t G, uint64_t player) {
    uint64_t T = 0, rec = skill_rec(G, C.skill, &T); if (!rec) return 0;
    uint32_t nt = *(uint32_t *)(uintptr_t)(T + 0x1300), ty = *(uint8_t *)(uintptr_t)(rec + 0x112);
    if (ty >= nt) ty = 21;   // 同 0x7aa5fc：越界回落束缚恶魔 | same as 0x7aa5fc: out of range falls back to bound demon
    uint64_t pd = rd(player + 0x10), lst = pd ? rd(pd + 0x98) : 0, arr = lst ? rd(lst) : 0; return arr ? arr + (uint64_t)ty * 0x20 : 0;
}
static int is_pet(uint64_t slot, uint32_t id) { int k = 0; for (uint64_t nd = rd(slot); nd && k < 256; nd = rd(nd + 0x18), k++) if (*(uint32_t *)(uintptr_t)(nd + 4) == id) return 1; return 0; }
static int to_minion(uint64_t G, uint64_t player, uint64_t u, int present) {   // present：这只之前已在场的我们的随从数 | present: how many of our minions were already present before this one
    int32_t *seq = (int32_t *)(uintptr_t)(G + 0x16c); int link = *seq == 0x7fffffff ? 1 : *seq + 1; *seq = link;
    uint32_t id = *(uint32_t *)(uintptr_t)(u + 8); int lv0 = gs(u, 12), mx0 = gsb(u, 7), full0 = gs(u, 7);
    // 数量：束缚恶魔的 petmax 是常数 1（skills.txt 第 384 行），束缚第二只时原版把第一只裁掉（0x6c39e0 → 0x6c0c30）。上限是束缚时按技能记录 +0x114 的算式现算的，
    // 所以这一次调用前把它临时换成骷髅复生（70）的算式「(lvl < 4) ? lvl : (2 + lvl / 3)」，技能等级给到够装下已在场的数目 + 1，调完立刻换回（真术士自己的束缚照旧一只）
    // Count: Bind Demon's petmax is the constant 1 (skills.txt line 384), so binding a second one makes the original trim the first (0x6c39e0 → 0x6c0c30). The cap is computed at bind time from the formula at skill record +0x114,
    // so for this one call it is temporarily swapped for Raise Skeleton's (70) formula "(lvl < 4) ? lvl : (2 + lvl / 3)", with a skill level high enough to hold those already present + 1, and swapped back right after (a real Warlock's own bind stays at one)
    uint64_t rec = skill_rec(G, C.skill, NULL), rsk = skill_rec(G, 70, NULL); int lvl = C.level, keep = 0;
    if (C.multi && rec && rsk) { int want = present + 1; int need = want < 4 ? want : 3 * (want - 2); if (need > lvl) lvl = need;
        keep = *(int32_t *)(uintptr_t)(rec + 0x114); *(int32_t *)(uintptr_t)(rec + 0x114) = *(int32_t *)(uintptr_t)(rsk + 0x114); }
    int r = ((int (*)(uint64_t, uint64_t, uint64_t, int, int, int, int))(B + 0x7aa190))(G, player, u, C.skill, lvl, link, C.init) & 1;
    if (C.multi && rec && rsk) *(int32_t *)(uintptr_t)(rec + 0x114) = keep;
    // 原版只在返回值上说「接受了」，挂没挂进宠物表不查（上限 0 时会直接清掉新目标）：按 id 回查宠物表才算数
    // The original's return value only says "accepted"; it does not check whether the pet table entry took (with a cap of 0 it clears the new target outright): it only counts after looking the id up in the pet table
    uint64_t slot = pet_slot(G, player); int in = r && slot && is_pet(slot, id);
    if (in && find_unit(G, id) == u) {   // 原版施法成功后的两处收尾（0x7ab948–0x7ab970）照做 | the two follow-ups after a successful original cast (0x7ab948–0x7ab970), done the same
        *(uint32_t *)(uintptr_t)(u + 0x124) |= 0x800; uint64_t md = rd(u + 0x10); if (md) *(uint16_t *)(uintptr_t)(md + 0x30) |= 8; }
    LOG("召唤：束缚 id %u → 返回 %d、宠物表%s；该类型 数量 %d 上限 %d", id, r, in ? "有" : "没有", slot ? *(int32_t *)(uintptr_t)(slot + 8) : -1, slot ? *(int32_t *)(uintptr_t)(slot + 0xc) : -1);
    // 束缚会按技能算式重算生命上限、把等级压到主人等级（0x7aa684 初始化块、0x79c130）：首领打一只怪就死（真机实测）。束缚完把等级、生命上限的基础项改回刷出时的值，再回满
    // Binding recomputes maximum life from the skill formulas and brings the level down to the owner's (init block at 0x7aa684, 0x79c130): the boss died after fighting one monster (measured on device). After binding, set the level and the base of maximum life back to their spawn values, then refill
    if (in && C.restore && find_unit(G, id) == u) {
        int lv1 = gs(u, 12), mx1 = gsb(u, 7), full1 = gs(u, 7);
        if (lv1 < lv0) ss(u, 12, lv0);
        long long want = (long long)mx0 * C.hp_pct / 100; if (want > 0x7fffff00) want = 0x7fffff00;   // 上限 0x7fffff00：再大溢出成负数，读出来成 1 点（同秘境） | cap 0x7fffff00: beyond it overflows negative and reads back as 1 point (same as the rift)
        if (want > 0 && gsb(u, 7) != (int)want) ss(u, 7, (int)want);
        ss(u, 6, gs(u, 7));
        LOG("召唤：id %u 刷出时 等级 %d 生命上限 %d（基础 %d）→ 束缚后 等级 %d 上限 %d（基础 %d）→ 改回后 等级 %d 上限 %d 当前 %d", id, lv0, full0 >> 8, mx0 >> 8, lv1, full1 >> 8, mx1 >> 8, gs(u, 12), gs(u, 7) >> 8, gs(u, 6) >> 8);
    }
    return in;
}

static void summon(uint64_t G, uint64_t player, int boss, int n) {
    int lv = dm_unit_level(player); uint64_t room = unit_room(player); int px, py;
    if (!lv || !room || !unit_xy(player, &px, &py)) return;
    if (in_town(lv)) { dm_game_notice(B, dm_str(B, "dmSumTown")); return; }
    cfg_load();
    static const int8_t ring[10][2] = { {3,0}, {-3,0}, {0,3}, {0,-3}, {3,3}, {-3,-3}, {3,-3}, {-3,3}, {5,0}, {-5,0} };
    int ok = 0, conv = 0;
    for (int i = 0; i < n && g_nids < MAX_KEEP; i++) {
        int32_t pos[2] = { px + ring[i][0], py + ring[i][1] }; uint64_t rr = free_spot(G, room, pos);
        uint64_t u = rr ? ((uint64_t (*)(uint64_t, uint64_t, int, int, int, int, int, int, uint64_t))(B + 0x7ce630))(G, rr, 1, k_boss[boss].cls, pos[0], pos[1], 1, 0, 0x10) : 0;
        if (!u) continue; ok++;
        *(uint32_t *)(uintptr_t)(u + 0x124) |= 0x20000;   // 不掉落：死了不出宝，也防清除时爆一地 | no drop: nothing on death, and no loot shower when cleared
        int c = C.bind ? to_minion(G, player, u, atomic_load(&g_alive) + conv) : 0; conv += c || !C.bind;
        g_hp[g_nids] = gs(u, 6); g_ids[g_nids++] = *(uint32_t *)(uintptr_t)(u + 8);
        LOG("召唤：%s（类号 %d）id %u 在 %d,%d（玩家 %d,%d）→ 转随从%s", k_boss[boss].key, k_boss[boss].cls, *(uint32_t *)(uintptr_t)(u + 8), pos[0], pos[1], px, py, c ? "成" : "败");
    }
    char msg[160]; snprintf(msg, sizeof msg, dm_str(B, "dmSumDone"), dm_str(B, k_boss[boss].key), ok); dm_game_notice(B, ok ? msg : dm_str(B, "dmSumNoRoom"));
    if (conv < ok) dm_game_notice(B, dm_str(B, "dmSumPartial"));
}

// 清除：原版解除随从那一下 0x6c0c30(游戏, 主人, 单位 id, 1)（解除技能入口 0x7bc3d0 就这么调）：摘出宠物表并交给原版消失 / 死亡处理。
// 先确认还在我们名单里、还挂在这个玩家的宠物表上；没挂上的（束缚没成）用杀怪 0x7d82b0(游戏, 单位, 0, 1) 收掉。都先置不掉落
// Clear: the original dismiss-minion call 0x6c0c30(game, owner, unit id, 1) (the dismiss skill entry 0x7bc3d0 calls it this way): takes it off the pet table and hands it to the original vanish / death handling.
// First confirm it is still on our list and still on this player's pet table; those not on it (bind failed) are taken down with the kill 0x7d82b0(game, unit, 0, 1). No-drop is set first in both cases
static void clear_all(uint64_t G, uint64_t player) {
    uint64_t slot = pet_slot(G, player); int n = 0;
    for (int i = 0; i < g_nids; i++) { uint32_t id = g_ids[i]; uint64_t u = find_unit(G, id); if (!u || !alive(u)) continue;
        *(uint32_t *)(uintptr_t)(u + 0x124) |= 0x20000;
        if (slot && is_pet(slot, id)) ((void (*)(uint64_t, uint64_t, uint32_t, int))(B + 0x6c0c30))(G, player, id, 1);
        else ((void (*)(uint64_t, uint64_t, uint64_t, int))(B + 0x7d82b0))(G, u, 0, 1);
        n++; }
    g_nids = 0; LOG("召唤：一键清除 %d 只", n); dm_game_notice(B, dm_str(B, n ? "dmSumCleared" : "dmSumNone"));
}

// 开全部小站：玩家数据块 P = [玩家+0x10]，三个难度的小站位图在 [P+0x58 + 8 × 难度]（16 字节：第 0 个 16 位字是版本头 0x102，其后小站 k 在字 1 + k / 16 的 1 << (k % 16)；0x6af190 激活小站就这么置位）。
// 有效小站编号 0–38（原版 levels.txt 的 Waypoint 列，共 39 个）。头不是 0x102 的不碰。改完把当前难度那份整份发给客户端 0x63e5c0([P+0xf0], 位图)（原版激活后同样调），再请一次原版存盘
// Unlock all waypoints: player data block P = [player+0x10]; the waypoint bitmaps of the three difficulties are at [P+0x58 + 8 × difficulty] (16 bytes: 16-bit word 0 is the version header 0x102, then waypoint k is bit 1 << (k % 16) of word 1 + k / 16; this is how 0x6af190 sets it when a waypoint is activated).
// Valid waypoint indices are 0–38 (the Waypoint column of the original levels.txt, 39 in all). Bitmaps whose header is not 0x102 are left alone. Afterwards the current difficulty's bitmap is sent whole to the client 0x63e5c0([P+0xf0], bitmap) (the original calls it after activation too), and one original save is requested
int dm_request_save(void);
static void unlock_waypoints(uint64_t player) {
    uint64_t P = rd(player + 0x10); if (!P) return; int n = 0;
    for (int d = 0; d < 3; d++) { uint16_t *wp = (uint16_t *)(uintptr_t)rd(P + 0x58 + 8 * d);
        if (!wp || wp[0] != 0x102) { LOG("通用修改：难度 %d 的小站位图%s，不改", d, wp ? "头不是 0x102" : "没有"); continue; }
        for (int k = 0; k < 39; k++) wp[1 + k / 16] |= (uint16_t)(1u << (k % 16)); n++; }
    uint8_t cur = *(uint8_t *)(uintptr_t)(g_G + 0x104); uint64_t client = rd(P + 0xf0), wpc = cur < 3 ? rd(P + 0x58 + 8 * cur) : 0;
    if (client && wpc) ((void (*)(uint64_t, uint64_t))(B + 0x63e5c0))(client, wpc);
    dm_request_save(); LOG("通用修改：开全部小站，%d 个难度", n); dm_game_notice(B, dm_str(B, "dmCheatWpDone"));
}
// 解锁全部难度：最高可进难度记在 [P+0xf0] +0x0e 这个 16 位字的第 8–12 位（progress）；原版打完巴尔 0x758330 把它抬到 max(旧值, 5 × 难度 + 5)，
// 资料片角色 > 4 可进噩梦、> 9 可进地狱（经典角色阈值 > 3 / > 7）。抬到 10，别的位不动；不改 G+0x104（那是本局的难度规则），也不碰任务位。
// 要等原版存盘、退回选角界面才看得到（当前这一局不变）
// Unlock all difficulties: the highest enterable difficulty is bits 8–12 (progress) of the 16-bit word at [P+0xf0] +0x0e; on killing Baal the original 0x758330 raises it to max(old, 5 × difficulty + 5);
// expansion characters can enter Nightmare at > 4 and Hell at > 9 (classic thresholds > 3 / > 7). It is raised to 10, other bits untouched; G+0x104 (this run's difficulty rule) and the quest bits are not touched.
// It only shows after the original saves and the player returns to character select (the current run does not change)
static void unlock_difficulty(uint64_t player) {
    uint64_t P = rd(player + 0x10), client = P ? rd(P + 0xf0) : 0; if (!client) return;
    uint16_t *fl = (uint16_t *)(uintptr_t)(client + 0x0e); unsigned old = (*fl >> 8) & 31u;
    if (old < 10) *fl = (uint16_t)((*fl & ~0x1f00u) | (10u << 8));
    dm_request_save(); LOG("通用修改：难度进度 %u → %u", old, (*fl >> 8) & 31u); dm_game_notice(B, dm_str(B, "dmCheatDiffDone"));
}

static void cheat(uint64_t player, int op, int arg) {
    int lv = gs(player, 12), to = lv;
    switch (op) {
    case DM_CHEAT_LEVEL_UP: to = lv + 1; break;
    case DM_CHEAT_LEVEL_99: to = 99; break;
    case DM_CHEAT_GOLD_MAX: ss(player, 14, lv * 10000); dm_game_notice(B, dm_str(B, "dmCheatGoldDone")); return;   // 身上金币上限 = 等级 × 1 万 | gold-on-person cap = level × 10,000
    case DM_CHEAT_STATPTS: ss(player, 4, gs(player, 4) + arg); dm_game_notice(B, dm_str(B, "dmCheatStatDone")); return;
    case DM_CHEAT_SKILLPTS: ss(player, 5, gs(player, 5) + arg); dm_game_notice(B, dm_str(B, "dmCheatSkillDone")); return;
    case DM_CHEAT_REFILL: ss(player, 6, gs(player, 7)); ss(player, 8, gs(player, 9)); ss(player, 10, gs(player, 11)); return;
    case DM_CHEAT_WAYPOINTS: unlock_waypoints(player); return;
    case DM_CHEAT_DIFFICULTY: unlock_difficulty(player); return;
    default: return; }
    // 升级走原版加经验 0x7dd9e0(游戏, 玩家, 原等级, 经验增量)：杀怪分经验最终调的就是它，按经验表连升多级并一次结算每级成长（属性点 / 技能点 / 生命法力耐力）与升级特效。
    // 门槛与等级上限都读运行时经验表 [数据表 + 0x1500]：+4 × 职业 = 该职业等级上限，+0x24 × 等级 + 4 × 职业 = 到该级的累计经验；基础等级 / 经验用 0x5b9110 读（同原版累加器）。
    // 不先写等级：先写的话成长差值就丢了。增量不走 0x7dd150（那里夹 0x7fffff、按等级差打折，还会乘经验加成）
    // Level up goes through the original add-experience 0x7dd9e0(game, player, old level, delta): what monster-kill experience ends in; it levels up through the table, possibly several levels, settling each level's growth (stat / skill points, life / mana / stamina) and the level-up effect in one go.
    // Thresholds and the level cap come from the runtime experience table [data table + 0x1500]: +4 × class = that class's level cap, +0x24 × level + 4 × class = cumulative experience for that level; base level / experience are read with 0x5b9110 (same as the original accumulator).
    // The level is not written first: that would lose the growth delta. The delta does not go through 0x7dd150 (which clamps to 0x7fffff, discounts by level difference and multiplies by the experience bonus)
    uint8_t ver = *(uint8_t *)(uintptr_t)(g_G + 0x106); uint64_t T = ver >= 1 && ver <= 3 ? rd((uint64_t)(uintptr_t)(B + 0x2d64a18 + ver * 16)) : 0, E = T ? rd(T + 0x1500) : 0;
    if (!E) { LOG("通用修改：经验表没取到，不升级"); return; }
    uint32_t c = *(uint32_t *)(uintptr_t)(player + 4); if (c >= 8) c = 0;
    uint32_t (*base)(uint64_t, int, int) = (uint32_t (*)(uint64_t, int, int))(B + 0x5b9110);
    int cap = (int)*(uint32_t *)(uintptr_t)(E + 4 * c); lv = (int)base(player, 12, 0);
    if (to > cap) to = cap;
    if (to <= lv) return;
    uint32_t xp = base(player, 13, 0), need = *(uint32_t *)(uintptr_t)(E + 0x24ull * (uint64_t)to + 4 * c);
    if (need > xp) ((void (*)(uint64_t, uint64_t, int, uint32_t))(B + 0x7dd9e0))(g_G, player, lv, need - xp);
    to = (int)base(player, 12, 0);
    LOG("通用修改：等级 %d → %d", lv, to); char msg[96]; snprintf(msg, sizeof msg, dm_str(B, "dmCheatLevelDone"), to); dm_game_notice(B, msg);
}

void dm_summon_tick(uint64_t G, uint64_t player, uint8_t *main_base) {
    B = main_base; if (!G || !player) return;
    if (G != g_G) { g_G = G; g_nids = 0; }
    int boss = atomic_exchange(&g_req_boss, -1); if (boss >= 0) summon(G, player, boss, atomic_load(&g_req_n));
    if (atomic_exchange(&g_req_clear, 0)) clear_all(G, player);
    int op = atomic_exchange(&g_req_op, 0); if (op) cheat(player, op, atomic_load(&g_req_arg));
    uint32_t m = *(uint32_t *)(uintptr_t)(player + 0xc);
    if (m != 0 && m != 12 && m != 17) {   // 玩家没在死：无敌 / 无限法力每拍补满 | player not dying: god mode / infinite mana top up every tick
        if (atomic_load(&g_god)) { int mx = gs(player, 7); if (gs(player, 6) < mx) ss(player, 6, mx); }
        if (atomic_load(&g_infmp)) { int mx = gs(player, 9); if (gs(player, 8) < mx) ss(player, 8, mx); mx = gs(player, 11); if (gs(player, 10) < mx) ss(player, 10, mx); }
    }
    // 名单：死了的、走远被卸载的去掉。读数：每 50 拍（2 秒）一行各只的生命；见到死了打一行（上一拍生命、上限、模式），查「随从自己死了」用
    // List: drop the dead and those unloaded far away. Readout: one line with each one's life every 50 ticks (2 s); one line on seeing a death (life on the previous tick, maximum, mode), for tracking down "the minion died by itself"
    // 属性开关：开关变了当拍写；开着的每 125 拍（5 秒）再对一次，防换图 / 读档后被游戏重算掉。关掉的写回 0 一次
    // Stat toggles: written on the tick the switches change; the ones on are re-checked every 125 ticks (5 s) in case a map change / reload recomputed them. Turned-off ones are written back to 0 once
    { static int applied; static uint64_t applied_pl; int want = atomic_load(&g_buffs);
      if (player != applied_pl) { applied_pl = player; applied = 0; }
      if (want != applied || (want && *(uint32_t *)(uintptr_t)(G + 0x170) % 125 == 0)) {
          for (int b = 0; b < DM_NBUFF; b++) { int on = want >> b & 1, was = applied >> b & 1; if (!on && !was) continue;
              for (int k = 0; k < k_buff[b].n; k++) { int v = on ? k_buff[b].v[k] : 0; if (gsb(player, k_buff[b].st[k]) != v) ss(player, k_buff[b].st[k], v); } }
          if (want != applied) LOG("通用修改：属性开关 %#x → %#x", applied, want);
          applied = want; } }
    uint32_t t = *(uint32_t *)(uintptr_t)(G + 0x170); int n = 0, w = 0; char line[600]; int lo = 0; line[0] = 0;
    for (int i = 0; i < g_nids; i++) { uint32_t id = g_ids[i]; uint64_t u = find_unit(G, id);
        if (!u) { LOG("召唤：id %u 不在单位表了（卸载或被回收），上一拍生命 %d", id, g_hp[i] >> 8); continue; }
        if (!alive(u)) { LOG("召唤：id %u 死了：上一拍生命 %d、上限 %d、等级 %d、模式 %u、+0x124 %#x", id, g_hp[i] >> 8, gs(u, 7) >> 8, gs(u, 12), *(uint32_t *)(uintptr_t)(u + 0xc), *(uint32_t *)(uintptr_t)(u + 0x124)); continue; }
        int hp = gs(u, 6); if (t % 50 == 0 && lo < (int)sizeof line - 60) lo += snprintf(line + lo, sizeof line - lo, " [id %u hp %d/%d lv %d]", id, hp >> 8, gs(u, 7) >> 8, gs(u, 12));
        g_hp[w] = hp; g_ids[w++] = id; n++; }
    if (lo) LOG("召唤：在场%s", line);
    g_nids = w; atomic_store(&g_alive, n);
}
