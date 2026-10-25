// 自动清图（面板「自动」页）：在当前地图里自己找怪、打怪、捡东西，整张图的房间都去过且附近没有活的敌对怪就停。服务端线程每拍调（dm_summon_tick）。
// 做法参照 Koolo / kolbot 的清图循环（调研：C:\Users\miiro\agy_tasks\20261026_bot_research\report.md），原版函数据静态逆向（C:\Users\miiro\agy_tasks\20261026_bot_re\report.md）：
//   · 选怪：服务端怪物表（游戏 +0x2630，128 桶，链 +0x158）里活着（+0x126 位 0 未置、模式不是 0 / 12）、可选取（+0x124 位 2 置、位 16 未置）、敌对（阵营 0，读法同 dm_rift.c unit_align）、
//     monstats 可杀且不是 NPC / 交互 / 城镇人物（行 +0x3d：位 7 置、低 3 位全空）、不是我们召唤的首领；30 格内，金怪 / 冠军 / Boss 优先，其次按距离。
//   · 打：对单位施放 0x6e7dc0(游戏, 玩家, 1, 怪 id, 技能节点, 1, 0)（收包 0x0d 的内部入口；最后一个 0 = 远了由原版自己走近）；
//     目标周围 6 格内有 3 只以上且配了范围技能时，改对目标脚下施放 0x6e77f0(游戏, 玩家, x, y, 1, 节点)。两者都要求节点是左 / 右手节点，调用期间把技能链 +0x10 临时指向它（同 dm_freetp.c）。
//   · 走：原版定点传送 dm_teleport（dm_unit.h，0x7cc530，目标房间先用 0x52dcc0 载入、0x7160d0 找空地），同 dm_rift.c 的测试清图；目标 15 格外先跳到它旁边。
//   · 探索：关卡房间链（关卡 +0x10，下一间 +0x48，几何 +0x60..+0x6c 乘 5）每次去最近的没去过的房间中心。
//   · 捡：地上物品表（游戏 +0x2e30，模式 3）按面板开关挑（暗金 / 套装、稀有、符文），先跳到旁边再发原版拾取包 0x16（同 dm_itemgen.c pick_up），每件只试一次。
//   · 停：图清完、换了地图（进出口 / 回城）、死了、没开无敌而生命低于 25%、关掉开关。
// Auto map clear (panel "Auto" page): finds, fights and loots in the current map by itself, and stops once every room of the map has been visited and no living hostile monster is near. Called every server tick (dm_summon_tick).
// Modelled on the Koolo / kolbot clear loops; original functions from static reverse engineering (reports above): monster filter, cast on unit 0x6e7dc0 / on a point 0x6e77f0 with the right-hand node temporarily
// pointed at the skill (as dm_freetp.c), movement by the original fixed-point teleport dm_teleport (as dm_rift.c's test clear), exploration by nearest unvisited room centre, looting by the original pick-up packet 0x16.
#include "dm_autoplay.h"
#include "dm_summon.h"
#include "dm_bossrush.h"
#include "dm_unit.h"
#include "hle.h"
#include "dm.h"
#include "dm_i18n.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *B;
static uint64_t rd(uint64_t a) { return a ? *(volatile uint64_t *)(uintptr_t)a : 0; }
static uint32_t rd32(uint64_t a) { return a ? *(volatile uint32_t *)(uintptr_t)a : 0; }
static int gs(uint64_t u, int st) { return ((int (*)(uint64_t, int, int))(B + 0x5b9290))(u, st, 0); }
static uint64_t unit_room(uint64_t u) { uint64_t p = rd(u + 0x38); return !p ? 0 : rd32(u) == 4 || rd32(u) == 2 ? rd(p) : rd(p + 0x20); }
static int unit_xy(uint64_t u, int *x, int *y) {   // 玩家 / 怪：动态路径 +2 / +6；物品 / 物件：静态路径 +0x10 / +0x14 | players / monsters: dynamic path +2 / +6; items / objects: static path +0x10 / +0x14
    uint64_t p = rd(u + 0x38); if (!p) return 0; uint32_t t = rd32(u);
    if (t == 2 || t == 4) { *x = *(int32_t *)(uintptr_t)(p + 0x10); *y = *(int32_t *)(uintptr_t)(p + 0x14); } else { *x = *(uint16_t *)(uintptr_t)(p + 2); *y = *(uint16_t *)(uintptr_t)(p + 6); }
    return 1;
}
static int unit_lv(uint64_t u) {   // 物品的房间在静态路径 +0；其余走 dm_unit.h | an item's room is at static path +0; everything else goes through dm_unit.h
    if (rd32(u) != 4) return dm_unit_level(u);
    uint64_t r1 = unit_room(u), r2 = rd(r1 + 0x18), lv = rd(r2 + 0x90); return lv ? *(int32_t *)(uintptr_t)(lv + 0x1f8) : 0;
}
static int unit_align(uint64_t u) {   // 同 dm_rift.c：属性表状态 0x69 的 0xac；没有 = 0 敌对 | as dm_rift.c: stat 0xac of state 0x69; none = 0 hostile
    uint64_t sl = rd(u + 0x88); if (!sl || !(rd32(sl + 0x1c) & 0x80000000u)) return 0;
    for (int pass = 0; pass < 2; pass++) for (uint64_t n = rd(sl + (pass ? 0xd8 : 0xd0)), k = 0; n && k < 512; n = rd(n + 0x68), k++)
        if (rd32(n + 0x20) == 0x69) return ((int (*)(int, uint64_t, int, int))(B + 0x5bc810))(*(uint8_t *)(uintptr_t)(u + 0x1bd), n, 0xac, 0);
    return 0;
}
static uint64_t free_spot(uint64_t G, uint64_t room, int32_t pos[2]) { uint64_t out = 0; ((void (*)(uint64_t, uint64_t, int32_t *, int, int, uint64_t *, int, int))(B + 0x7160d0))(G, room, pos, 3, 0x400, &out, 4, 100); return out; }
static uint64_t room_load(uint64_t G, uint64_t lv, int px, int py) {   // 同 dm_rift.c：坐标所在的关卡房间，没载入就用原版房间初始化载入 | as dm_rift.c: the level room holding the point, loaded with the original room init if needed
    int n = 0; for (uint64_t r = dm_level_rooms(lv); r && n < 2000; r = rd(r + 0x48), n++) {
        int x = *(int32_t *)(uintptr_t)(r + 0x60) * 5, y = *(int32_t *)(uintptr_t)(r + 0x64) * 5, w = *(int32_t *)(uintptr_t)(r + 0x68) * 5, h = *(int32_t *)(uintptr_t)(r + 0x6c) * 5;
        if (px >= x && px < x + w && py >= y && py < y + h) { if (!rd(r + 0x58)) ((void (*)(int, uint64_t))(B + 0x52dcc0))(*(uint8_t *)(uintptr_t)(G + 0x106), r); return rd(r + 0x58); } }
    return 0;
}
static int jump_to(uint64_t G, uint64_t pl, int x, int y) {   // 定点传送到 (x, y) 附近的空地；返回 1 = 成 | fixed-point teleport to open ground near (x, y); 1 = done
    uint64_t lv = dm_unit_level_ptr(pl), rs = room_load(G, lv, x, y); int32_t pos[2] = { x, y }; uint64_t rr = rs ? free_spot(G, rs, pos) : 0;
    return rr && dm_teleport(B, G, pl, rr, pos[0], pos[1]);
}
static int in_town(int lv) { return lv == 1 || lv == 40 || lv == 75 || lv == 103 || lv == 109; }

// 可选技能：0 = 当前右键技能；其余 {串表键, 技能号, 所属职业}。别的职业的技能按附加技能的办法挂上（不进存档），本职业的要自己学过
// Selectable skills: 0 = the current right-hand skill; the rest {string key, skill id, owning class}. Other classes' skills are attached the extra-skill way (not saved); own-class ones must already be learned
static const struct { const char *key; int16_t sk; int8_t cls; } k_as[] = {
    { "dmAsRight", 0, -1 }, { "dmAsHammer", 112, 3 }, { "dmAsFoh", 121, 3 }, { "dmAsZeal", 106, 3 }, { "dmAsSmite", 97, 3 }, { "dmAsHolyBolt", 101, 3 },
    { "dmAsFrozenOrb", 64, 1 }, { "dmAsBlizzard", 59, 1 }, { "dmAsMeteor", 56, 1 }, { "dmAsChainLight", 53, 1 }, { "dmAsNova", 48, 1 },
    { "dmAsBoneSpear", 84, 2 }, { "dmAsCorpseExp", 74, 2 }, { "dmAsWhirlwind", 151, 4 }, { "dmAsBerserk", 152, 4 },
    { "dmAsMultiShot", 22, 0 }, { "dmAsStrafe", 24, 0 }, { "dmAsLightFury", 35, 0 }, { "dmAsChargedStrike", 34, 0 },
    { "dmAsTornado", 245, 5 }, { "dmAsHurricane", 250, 5 }, { "dmAsFissure", 234, 5 }, { "dmAsLightSentry", 271, 6 }, { "dmAsPhoenix", 280, 6 },
};
enum { NAS = (int)(sizeof k_as / sizeof k_as[0]), AS_LEVEL = 20 };
int dm_auto_skill_count(void) { return NAS; }
const char *dm_auto_skill_key(int i) { return i >= 0 && i < NAS ? k_as[i].key : ""; }

static _Atomic int g_on, g_main, g_aoe = -1, g_pick = 7, g_rooms, g_rooms_n, g_kills, g_state;
void dm_auto_set(int on) { atomic_store(&g_on, on ? 1 : 0); }
int dm_auto_on(void) { return atomic_load(&g_on); }
void dm_auto_set_skill(int aoe, int i) { if (i < (aoe ? -1 : 0) || i >= NAS) return; atomic_store(aoe ? &g_aoe : &g_main, i); }
int dm_auto_skill(int aoe) { return atomic_load(aoe ? &g_aoe : &g_main); }
void dm_auto_set_pick(int mask) { atomic_store(&g_pick, mask); }
int dm_auto_pick(void) { return atomic_load(&g_pick); }
void dm_auto_status(int *rooms, int *total, int *kills, int *state) { *rooms = atomic_load(&g_rooms); *total = atomic_load(&g_rooms_n); *kills = atomic_load(&g_kills); *state = atomic_load(&g_state); }

// 技能节点：0 号 = 右手节点；技能链里有就用；别的职业的没有就挂上（0x7bf010）并给等级（属性 97，同 dm_summon.c 附加技能）
// Skill node: entry 0 = the right-hand node; use the one in the chain if present; another class's skill that is missing gets attached (0x7bf010) with a level (stat 97, as the extra skills in dm_summon.c)
static uint64_t skill_node(uint64_t pl, int i) {
    uint64_t sl = rd(pl + 0x100); if (!sl) return 0; if (i <= 0 || i >= NAS) return rd(sl + 0x10);
    int sk = k_as[i].sk; for (uint64_t n = rd(sl), k = 0; n && k < 512; n = rd(n + 8), k++) { uint64_t rec = rd(n); if (rec && *(int16_t *)(uintptr_t)rec == sk) return n; }
    if ((int)rd32(pl + 4) == k_as[i].cls) return 0;   // 本职业没学：不白送 | own class, not learned: not given away
    uint64_t nd = ((uint64_t (*)(uint64_t, int))(B + 0x7bf010))(pl, sk); if (nd) ((void (*)(uint64_t, int, int, int))(B + 0x5bc1f0))(pl, 97, AS_LEVEL, sk);
    LOG("自动清图：挂上技能 %d → %s", sk, nd ? "成" : "【红】败"); return nd;
}
static int cast_unit(uint64_t G, uint64_t pl, uint64_t node, uint32_t id) {
    uint64_t sl = rd(pl + 0x100); if (!sl || !node) return -1; uint64_t old = rd(sl + 0x10); *(uint64_t *)(uintptr_t)(sl + 0x10) = node;
    int r = ((int (*)(uint64_t, uint64_t, uint32_t, uint32_t, uint64_t, int, int))(B + 0x6e7dc0))(G, pl, 1, id, node, 1, 0); *(uint64_t *)(uintptr_t)(sl + 0x10) = old; return r;
}
static int cast_at(uint64_t G, uint64_t pl, uint64_t node, int x, int y) {
    uint64_t sl = rd(pl + 0x100); if (!sl || !node) return -1; uint64_t old = rd(sl + 0x10); *(uint64_t *)(uintptr_t)(sl + 0x10) = node;
    int r = ((int (*)(uint64_t, uint64_t, int, int, int, uint64_t))(B + 0x6e77f0))(G, pl, x, y, 1, node); *(uint64_t *)(uintptr_t)(sl + 0x10) = old; return r;
}
static int monster_ok(uint64_t G, uint64_t u, int lv) {
    if (rd32(u) != 1 || (*(uint8_t *)(uintptr_t)(u + 0x126) & 1)) return 0;
    uint32_t m = rd32(u + 0xc), f = rd32(u + 0x124); if (m == 0 || m == 12 || !(f & 4) || (f & 0x10000)) return 0;
    uint8_t v = *(uint8_t *)(uintptr_t)(u + 0x1bd); if (v < 1 || v > 3) return 0; uint64_t T = rd((uint64_t)(uintptr_t)(B + 0x2d64a18 + v * 16)), rows = T ? rd(T + 0xf90) : 0;
    uint32_t cls = rd32(u + 4); if (!rows || cls >= rd(T + 0xf98)) return 0; uint8_t mf = *(uint8_t *)(uintptr_t)(rows + (uint64_t)cls * 0x1fc + 0x3d);
    if (!(mf & 0x80) || (mf & 7)) return 0;   // 不可杀 / NPC / 交互 / 城镇人物 | not killable / NPC / interact / town folk
    if (unit_align(u) != 0 || dm_summon_owns(rd32(u + 8)) || dm_unit_level(u) != lv) return 0;
    (void)G; return 1;
}
static int rank_of(uint64_t u) { uint64_t md = rd(u + 0x10); uint8_t r = md ? *(uint8_t *)(uintptr_t)(md + 0x1a) : 0; return (r & 0x0e) ? 1 : 0; }   // 超级独特 / 冠军 / 金怪 | super unique / champion / unique

enum { MAXR = 1024, MAXSKIP = 256, MAXTRIED = 512 };
static struct { uint64_t G; int lv, nr, done, cur_hits, nskip, ntried, idle, stuck_room, refused; uint32_t cur, skip[MAXSKIP], tried[MAXTRIED]; uint64_t room[MAXR]; uint8_t seen[MAXR]; } S;
static void stop(const char *key) { atomic_store(&g_on, 0); atomic_store(&g_state, 0); LOG("自动清图：停（%s），房间 %d / %d，击杀 %d", key, S.done, S.nr, atomic_load(&g_kills)); dm_game_notice(B, dm_str(B, key)); memset(&S, 0, sizeof S); }
static int skipped(uint32_t id) { for (int i = 0; i < S.nskip; i++) if (S.skip[i] == id) return 1; return 0; }
static int tried(uint32_t id) { for (int i = 0; i < S.ntried; i++) if (S.tried[i] == id) return 1; return 0; }
static int pick_wanted(uint64_t it, int mask) {
    uint64_t d = rd(it + 0x10); if (!d) return 0; int q = (int)rd32(d);
    if ((mask & 1) && (q == 5 || q == 7)) return 1;
    if ((mask & 2) && q == 6) return 1;
    if (mask & 8) return 1;   // 测试用：什么都捡（面板不给这一位） | for tests: pick up everything (the panel does not offer this bit)
    if (mask & 4) { uint32_t c = ((uint32_t (*)(uint64_t))(B + 0x570260))(it); char s[5] = { (char)c, (char)(c >> 8), (char)(c >> 16), (char)(c >> 24), 0 }; if (s[0] == 'r' && s[1] >= '0' && s[1] <= '9' && s[2] >= '0' && s[2] <= '9') return 1; }
    return 0;
}

void dm_autoplay_tick(uint64_t G, uint64_t pl, uint8_t *mb) {
    B = mb; if (!G || !pl) return;
    if (!atomic_load(&g_on)) { if (S.G) memset(&S, 0, sizeof S); atomic_store(&g_state, 0); return; }
    uint32_t t = rd32(G + 0x170); if (t % 5) return;   // 每 5 拍（0.2 秒）想一次 | think every 5 ticks (0.2 s)
    int lv = dm_unit_level(pl), px, py; uint32_t pm = rd32(pl + 0xc);
    if (!lv || !unit_xy(pl, &px, &py)) return;
    if (in_town(lv)) { stop("dmAutoTown"); return; }
    if (pm == 0 || pm == 17) { stop("dmAutoDead"); return; }
    if (!dm_cheat_god() && gs(pl, 6) < gs(pl, 7) / 4) { stop("dmAutoLow"); return; }
    if (S.G != G || !S.lv) {   // 开工：记下这张图的房间 | start: record this map's rooms
        memset(&S, 0, sizeof S); S.G = G; S.lv = lv; atomic_store(&g_kills, 0);
        for (uint64_t r = dm_level_rooms(dm_unit_level_ptr(pl)); r && S.nr < MAXR; r = rd(r + 0x48)) S.room[S.nr++] = r;
        atomic_store(&g_rooms_n, S.nr); atomic_store(&g_rooms, 0); atomic_store(&g_state, 1);
        LOG("自动清图：开始，关卡 %d，房间 %d 间，主攻 %d 范围 %d 拾取 %#x", lv, S.nr, atomic_load(&g_main), atomic_load(&g_aoe), atomic_load(&g_pick));
    }
    if (lv != S.lv) { stop("dmAutoLeft"); return; }
    if (pm == 7 || pm == 8 || pm == 10 || pm == 18) return;   // 正在出招：等它做完 | mid-attack / cast: let it finish
    // 1. 打：上一个目标死了记一只；选新目标 | 1. fight: count the previous target if it died; pick a target
    if (S.cur) { uint64_t u = 0; for (int b = 0; b < 128 && !u; b++) for (uint64_t v = rd(G + 0x2630 + 8 * b); v; v = rd(v + 0x158)) if (rd32(v + 8) == S.cur) { u = v; break; }
        uint32_t m = u ? rd32(u + 0xc) : 0; if (!u || m == 0 || m == 12) { atomic_fetch_add(&g_kills, 1); S.cur = 0; S.cur_hits = 0; } }
    uint64_t best = 0; long bd = 0; int bx = 0, by = 0;
    for (int b = 0; b < 128; b++) for (uint64_t u = rd(G + 0x2630 + 8 * b); u; u = rd(u + 0x158)) { int x, y;
        if (!monster_ok(G, u, lv) || skipped(rd32(u + 8)) || !unit_xy(u, &x, &y)) continue;
        long d = (long)(x - px) * (x - px) + (long)(y - py) * (y - py); if (d > 30 * 30) continue;
        long score = d - (rank_of(u) ? 400 : 0); if (!best || score < bd) { best = u; bd = score; bx = x; by = y; } }
    if (best) {
        uint32_t id = rd32(best + 8); if (id != S.cur) { S.cur = id; S.cur_hits = 0; }
        if (++S.cur_hits > 75) { if (S.nskip < MAXSKIP) S.skip[S.nskip++] = id; LOG("自动清图：怪 id %u（类号 %u）打了 15 秒没死，跳过", id, rd32(best + 4)); S.cur = 0; return; }
        long dd = (long)(bx - px) * (bx - px) + (long)(by - py) * (by - py);
        if (dd > 15 * 15) { jump_to(G, pl, bx + 2, by + 2); return; }
        int ai = atomic_load(&g_aoe), near = 0;
        if (ai >= 0) for (int b = 0; b < 128; b++) for (uint64_t u = rd(G + 0x2630 + 8 * b); u; u = rd(u + 0x158)) { int x, y;
            if (monster_ok(G, u, lv) && unit_xy(u, &x, &y) && (x - bx) * (x - bx) + (y - by) * (y - by) <= 36) near++; }
        // 主攻 / 范围技能连着 3 次被原版拒绝（法力不够、距离、技能不许可…）就改用左手节点（一般是普通攻击）打这一下 | after 3 refusals in a row (mana, range, not allowed...) hit with the left-hand node (usually the plain attack) instead
        int r, aoe = ai >= 0 && near >= 3; uint64_t nd = S.refused >= 3 ? rd(rd(pl + 0x100) + 8) : skill_node(pl, aoe ? ai : atomic_load(&g_main));
        if (S.refused >= 3) { uint64_t sl = rd(pl + 0x100); r = sl && nd ? ((int (*)(uint64_t, uint64_t, uint32_t, uint32_t, uint64_t, int, int))(B + 0x6e7dc0))(G, pl, 1, id, nd, 1, 0) : -1; S.refused = 0; }
        else { r = aoe ? cast_at(G, pl, nd, bx, by) : cast_unit(G, pl, nd, id);
            if (aoe && r != 0) { aoe = 0; nd = skill_node(pl, atomic_load(&g_main)); r = cast_unit(G, pl, nd, id); } }   // 范围技能被拒（多半在冷却）当拍改放主攻 | area skill refused (usually its cooldown): cast the main skill this tick instead
        if (r < 0 && !nd) { stop("dmAutoNoSkill"); return; }
        if (r != 0) S.refused++; else S.refused = 0;
        { static int ln; if (ln++ < 40 || r != 0 && ln % 50 == 0) LOG("自动清图：%s 怪 id %u（距 %ld）→ 返回 %d，玩家模式 %u → %u，法力 %d", aoe ? "范围" : "主攻", id, (long)__builtin_sqrt((double)dd), r, pm, rd32(pl + 0xc), gs(pl, 8) >> 8); }
        S.idle = 0; return;
    }
    // 2. 捡：附近 25 格内要的东西，每件试一次 | 2. loot: wanted items within 25 cells, one try each
    int mask = atomic_load(&g_pick);
    if (mask) { uint64_t bi = 0; long bdi = 0; int ix = 0, iy = 0;
        for (int b = 0; b < 128; b++) for (uint64_t it = rd(G + 0x2e30 + 8 * b); it; it = rd(it + 0x158)) { int x, y;
            if (rd32(it) != 4 || rd32(it + 0xc) != 3 || tried(rd32(it + 8)) || !unit_xy(it, &x, &y) || unit_lv(it) != lv || !pick_wanted(it, mask)) continue;
            long d = (long)(x - px) * (x - px) + (long)(y - py) * (y - py); if (d <= 25 * 25 && (!bi || d < bdi)) { bi = it; bdi = d; ix = x; iy = y; } }
        if (bi) { uint32_t id = rd32(bi + 8);
            if (bdi > 2 * 2) { if (!jump_to(G, pl, ix, iy) && S.ntried < MAXTRIED) S.tried[S.ntried++] = id; return; }
            if (S.ntried < MAXTRIED) S.tried[S.ntried++] = id;
            uint64_t ip = rd(bi + 0x38); uint8_t pk[17] = { 0x16 }; uint32_t one = 1, q = rd32(rd(bi + 0x10)); memcpy(pk + 1, &id, 4); memcpy(pk + 5, (void *)(uintptr_t)(ip + 0x10), 4); memcpy(pk + 9, (void *)(uintptr_t)(ip + 0x14), 4); memcpy(pk + 13, &one, 4);
            uint32_t *busy = (uint32_t *)(uintptr_t)(G + 0x19050), old = *busy; *busy = 1; ((void (*)(uint64_t, uint64_t, uint8_t *, uint32_t))(B + 0x6e5b40))(G, pl, pk, 17); *busy = old;
            // 捡起后不再碰 bi：金币一捡就并进金币数、单位当场释放（云端测试读到乱的品质）；按 id 回地上物品表查还在不在 | after the pick-up bi is not touched again: gold merges into the gold count and the unit is freed at once (the cloud test read garbage quality); look the id up in the ground table instead
            int still = 0; for (int b = 0; b < 128 && !still; b++) for (uint64_t it = rd(G + 0x2e30 + 8 * b); it; it = rd(it + 0x158)) if (rd32(it + 8) == id) { still = rd32(it + 0xc) == 3; break; }
            LOG("自动清图：捡 id %u（品质 %u）→ %s", id, q, still ? "还在地上（背包满 / 够不着）" : "捡到");
            return; } }
    // 3. 探索：去最近的没去过的房间 | 3. explore: go to the nearest unvisited room
    int bestr = -1; long brd = 0, cx = 0, cy = 0;
    for (int i = 0; i < S.nr; i++) { if (S.seen[i]) continue; uint64_t r = S.room[i];
        long x = *(int32_t *)(uintptr_t)(r + 0x60) * 5 + *(int32_t *)(uintptr_t)(r + 0x68) * 5 / 2, y = *(int32_t *)(uintptr_t)(r + 0x64) * 5 + *(int32_t *)(uintptr_t)(r + 0x6c) * 5 / 2;
        long d = (x - px) * (x - px) + (y - py) * (y - py); if (bestr < 0 || d < brd) { bestr = i; brd = d; cx = x; cy = y; } }
    if (bestr < 0) { stop("dmAutoDone"); return; }
    S.seen[bestr] = 1; S.done++; atomic_store(&g_rooms, S.done);
    if (brd > 8 * 8) { int ok = jump_to(G, pl, (int)cx, (int)cy); if (!ok) { static int ln; if (ln++ < 20) LOG("自动清图：第 %d 间房 %ld,%ld 跳不过去，跳过", bestr, cx, cy); } }
}
