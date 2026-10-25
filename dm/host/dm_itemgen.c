// 刷装备（界面在 dm/ios/Sources/dm_summon_panel.m「装备」页）：生成指定暗金 / 套装物品 / 成品符文之语。
// 清单：dm_items_tab.h（原版 uniqueitems / setitems / runes 表与中英文名，生成器见文件头；符文之语另带挑好的底材码与孔数）。
// 依据（静态逆向，见交接文档 handover\reports）：
//   通用生成 0x656780(游戏, 参数块 0xd8, 0)，参数照 dm_rift.c give_item2：+0 玩家、+0x10 游戏、+0x18 物品等级、+0x20 统一物品类号（[数据表 + 0x258] 的 {码, 类号} 索引）、
//   +0x24 模式 3（地上）、+0x28 / +0x2c 落点、+0x30 房间、+0x38 = 1、+0x3a = [G+0x128]、+0x40 品质（7 暗金、5 套装、2 普通）、+0x50 指定暗金 / 套装行号 + 1；
//   暗金另有本局已出位图 G+0x3638（第 row >> 5 个 u32 的 row & 31 位），重复生成前清这一位。生成后核对 物品数据 +0 品质、+0x34 行号。
//   鉴定：0x78acf0(游戏, 玩家, 物品)（同 diag.c 免费鉴定，自己广播更新）。设孔：0x5754a0(物品, 孔数)，属性 194 读回。
//   去力量 / 敏捷需求：物品自己的属性 91 item_req_percent 有效值 −100（存档 8 位、Save Add 100，能存 −100），写进物品词缀表（见 no_req）。
//   等级需求不写进物品：属性 92 存档只有 7 位无符号，负数写盘读回会变成一个大正数，存档不安全（见交接文档）。
//   改为「装备免等级需求」开关：开着时把运行时数据表里的需求等级清零——暗金行 +0x3a、套装行 +0x36（i16）、物品表行 +0x14f（u8，底材与符文）；
//   等级函数 0x571580 合并的正是这几项（另有魔法词缀，这里不管），客户端、服务端读同一张表。只改内存不进存档，关掉原样写回，重开游戏也恢复原值。
//   代价：开着时所有同类物品都免等级，不只是刷出来的。
//   行号核对：清单行号来自原版文本表（暗金 / 套装的 *ID、符文之语的数据行），生成前先核对运行时表同一行的物品码（+0x28）/ 名字键（+0），
//   对不上就按物品码 + 物品等级（暗金 +0x38、套装 +0x34）或名字键在运行时表里找，找不到不生成。
//   捡进背包：照 diag.c DM_TEST_GIVE_INV 交 0x16 包 {物品 id, 地上 x, y, 1} 给 0x6e5b40(游戏, 玩家, 包, 17)，期间 G+0x19050 = 1。背包放不下就留在脚下。
//   成品符文之语：底材（普通品质、指定孔数）和各颗符文都捡进背包后，交给 dm_runeword.c 的一键合成（dm_runeword_auto，物品栏开着时自动镶，成不成词由原版判）。
// Item spawner (UI on the "Items" page of dm/ios/Sources/dm_summon_panel.m): creates a chosen unique / set item / finished runeword.
// Lists: dm_items_tab.h (the original uniqueitems / setitems / runes tables with Chinese and English names, generator noted in its header; runewords also carry the chosen base code and socket count).
// Basis (static reverse engineering, see the handover document handover\reports):
//   generic creation 0x656780(game, 0xd8 parameter block, 0), parameters as in dm_rift.c give_item2: +0 player, +0x10 game, +0x18 item level, +0x20 unified item class (the {code, class} index at [data table + 0x258]),
//   +0x24 mode 3 (ground), +0x28 / +0x2c drop point, +0x30 room, +0x38 = 1, +0x3a = [G+0x128], +0x40 quality (7 unique, 5 set, 2 normal), +0x50 chosen unique / set row + 1;
//   uniques also have the already-dropped-this-run bitmap at G+0x3638 (bit row & 31 of u32 number row >> 5), cleared before creating a repeat. After creation, item data +0 quality and +0x34 row are checked.
//   Identify: 0x78acf0(game, player, item) (same as diag.c free identify, broadcasts the update itself). Sockets: 0x5754a0(item, count), read back via stat 194.
//   Strength / dexterity requirements removed: effective value −100 of the item's own stat 91 item_req_percent (saved in 8 bits with Save Add 100, so −100 is storable), written into the item affix list (see no_req).
//   The level requirement is not written into the item: stat 92 is saved as 7 unsigned bits, so a negative value would read back as a large positive one; not save-safe (see the handover document).
//   Instead a "no level requirement" switch: while on, the required level in the runtime data tables is zeroed - unique row +0x3a, set row +0x36 (i16), item table row +0x14f (u8, bases and runes);
//   these are exactly what the level function 0x571580 merges (magic affixes aside, not handled here), and client and server read the same table. Memory only, never saved; turning it off writes the originals back, and a restart restores them too.
//   Cost: while on, every item of those kinds has no level requirement, not just spawned ones.
//   Row check: list rows come from the original text tables (*ID for unique / set, data row for runewords); before creating, the runtime table's same row is checked by item code (+0x28) / name key (+0),
//   and on a mismatch the runtime table is searched by item code + item level (unique +0x38, set +0x34) or by name key; nothing is created if not found.
//   Pick-up into the inventory: as diag.c DM_TEST_GIVE_INV, a 0x16 packet {item id, ground x, y, 1} handed to 0x6e5b40(game, player, packet, 17) with G+0x19050 = 1. If it does not fit, it stays at the player's feet.
//   Finished runeword: once the base (normal quality, given socket count) and each rune are in the inventory, it is handed to dm_runeword.c's one-tap crafting (dm_runeword_auto: sockets automatically while the inventory is open; whether the word forms is up to the original).
#include "dm_itemgen.h"
#include "dm_items_tab.h"
#include "dm_bossrush.h"
#include "dm_i18n.h"
#include "hle.h"
#include "dm.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void dm_runeword_auto(uint32_t base_id, int rec);   // dm_runeword.c
int dm_runeword_inv_open(void);                     // dm_runeword.c
int dm_runeword_busy(void);                         // dm_runeword.c
typedef struct lo_piece lo_piece_t;                 // 一键配装的一件（定义在下面「一键配装」一节） | one loadout piece (defined in the loadout section below)
typedef struct { uint16_t stat, layer; int32_t value; } lo_stat_t;
struct lo_aff;
static uint32_t g_rwpend, g_rwpend_at;            // 刷装备页等成词的符文之语底材 id 与开始等的拍（rw_post） | base id of an item-page runeword awaiting its word, and the tick waiting began (rw_post)
static const struct lo_aff *g_aff;                  // make_ready2 捡起前要写的词缀（只在服务端线程用） | affixes make_ready2 writes before pick-up (server thread only)
static int g_aff_bad;                               // 上面那组写失败 | the group above failed
static int apply_list(uint64_t it, int rw, const struct lo_aff *a);
static uint64_t affix_list(uint64_t it, int rw);

static uint8_t *B;
static _Atomic int g_req = -1;   // 种类 << 16 | 序号；-1 = 没有 | kind << 16 | index; -1 = none
static _Atomic int g_nolv = 1;   // 装备免等级需求（界面记住玩家的选择，启动时设回来） | no level requirement (the UI remembers the player's choice and sets it back at start)
void dm_itemgen_set_nolevel(int on) { atomic_store(&g_nolv, on != 0); }
int dm_itemgen_nolevel(void) { return atomic_load(&g_nolv); }

// 符文之语清单：每条一行（底材是清单按「精英、需求等级最高」挑的，武器常挑到双手的），另加两种变体行：
//   盾：能上盾、挑的底材又不是盾的（如精神挑了剑、巨龙挑了铠甲），底材用君主盾 uit（4 孔、全职业）；
//   单手：能上剑 / 近战 / 任意武器、没有排除类型的，底材用幻化之刃 7cr（单手剑、6 孔），好配盾（如正义之手、战争召唤）
// Runeword list: one row each (the list picked the base as "elite, highest required level", which for weapons is often two-handed), plus two kinds of variant rows:
//   shield: allowed on shields while the chosen base is not a shield (e.g. Spirit picked a sword, Dragon an armor), on a Monarch uit (4 sockets, all classes);
//   one-handed: allowed on swords / melee / any weapon with no excluded types, on a Phase Blade 7cr (one-handed sword, 6 sockets), to pair with a shield (e.g. Hand of Justice, Call to Arms)
static const char *const k_var_base[] = { NULL, "uit ", "7cr " };
static struct { int16_t rw; uint8_t var; } g_rwl[3 * DM_RUNEWORDS_COUNT]; static int g_nrwl;
static void rwl_build(void) {
    if (g_nrwl) return; int n = 0;
    for (int i = 0; i < DM_RUNEWORDS_COUNT; i++) { const dm_runeword_t *w = &dm_runewords[i]; int shld = 0, wpn = 0, ex = 0;
        g_rwl[n].rw = (int16_t)i; g_rwl[n++].var = DM_RW_PICKED;
        for (int k = 0; k < 6; k++) if (w->itype[k]) { shld |= !strcmp(w->itype[k], "shld"); wpn |= !strcmp(w->itype[k], "weap") || !strcmp(w->itype[k], "mele") || !strcmp(w->itype[k], "swor"); }
        for (int k = 0; k < 3; k++) ex |= w->etype[k] && *w->etype[k];
        if (shld && w->nsock >= 1 && w->nsock <= 4 && memcmp(w->base, k_var_base[DM_RW_SHIELD], 4)) { g_rwl[n].rw = (int16_t)i; g_rwl[n++].var = DM_RW_SHIELD; }
        if (wpn && !ex && w->nsock >= 1 && w->nsock <= 6 && memcmp(w->base, k_var_base[DM_RW_ONEHAND], 4)) { g_rwl[n].rw = (int16_t)i; g_rwl[n++].var = DM_RW_ONEHAND; } }
    g_nrwl = n;
}
static const dm_runeword_t *rwl(int i, int *var) { rwl_build(); *var = g_rwl[i].var; return &dm_runewords[g_rwl[i].rw]; }
static const char *rw_base(const dm_runeword_t *w, int var) { return var ? k_var_base[var] : w->base; }
int dm_itemgen_count(int kind) { if (kind == DM_ITEM_RUNEWORD) rwl_build(); return kind == DM_ITEM_UNIQUE ? DM_UNIQUE_ITEMS_COUNT : kind == DM_ITEM_SET ? DM_SET_ITEMS_COUNT : kind == DM_ITEM_RUNEWORD ? g_nrwl : 0; }
int dm_itemgen_variant(int kind, int i) { int v = DM_RW_PICKED; if (kind == DM_ITEM_RUNEWORD && i >= 0 && i < dm_itemgen_count(kind)) rwl(i, &v); return v; }
const char *dm_itemgen_name(int kind, int i) {
    if (i < 0 || i >= dm_itemgen_count(kind)) return ""; int zh = dm_lang_zh(), s;
    if (kind == DM_ITEM_UNIQUE) return zh ? dm_unique_items[i].name_zh : dm_unique_items[i].name_en;
    if (kind == DM_ITEM_SET) return zh ? dm_set_items[i].name_zh : dm_set_items[i].name_en;
    const dm_runeword_t *w = rwl(i, &s); return zh ? w->name_zh : w->name_en;
}
const char *dm_itemgen_sub(int kind, int i) {
    static char s[96]; int sh; if (i < 0 || i >= dm_itemgen_count(kind)) return "";
    if (kind == DM_ITEM_UNIQUE) snprintf(s, sizeof s, "%.4s · Lv %d · #%d", dm_unique_items[i].code, dm_unique_items[i].lvl_req, dm_unique_items[i].row);   // 行号区分同名的（彩虹魔石有 8 条） | the row tells same-named entries apart (Rainbow Facet has 8)
    else if (kind == DM_ITEM_SET && dm_lang_zh()) snprintf(s, sizeof s, "%s · Lv %d", dm_set_items[i].set_zh, dm_set_items[i].lvl_req);
    else if (kind == DM_ITEM_SET) snprintf(s, sizeof s, "%.4s · Lv %d", dm_set_items[i].code, dm_set_items[i].lvl_req);   // 套装名只有中文 | the set name is Chinese only
    else { const dm_runeword_t *w = rwl(i, &sh); snprintf(s, sizeof s, "%.4s · %d", rw_base(w, sh), w->nsock); }
    return s;
}
int dm_itemgen_match(int kind, int i, const char *q) {   // 中英文名都比，英文不分大小写 | matches both Chinese and English names, English case-insensitively
    if (i < 0 || i >= dm_itemgen_count(kind)) return 0; if (!q || !*q) return 1;
    if (kind == DM_ITEM_RUNEWORD) { int s; i = (int)(rwl(i, &s) - dm_runewords); }
    const char *zh = kind == DM_ITEM_UNIQUE ? dm_unique_items[i].name_zh : kind == DM_ITEM_SET ? dm_set_items[i].name_zh : dm_runewords[i].name_zh;
    const char *en = kind == DM_ITEM_UNIQUE ? dm_unique_items[i].name_en : kind == DM_ITEM_SET ? dm_set_items[i].name_en : dm_runewords[i].name_en;
    return (zh && strstr(zh, q)) || (en && strcasestr(en, q)) || (kind == DM_ITEM_SET && dm_set_items[i].set_zh && strstr(dm_set_items[i].set_zh, q));
}
void dm_itemgen_request(int kind, int i) { if (i >= 0 && i < dm_itemgen_count(kind)) atomic_store(&g_req, kind << 16 | i); }

static uint64_t rd(uint64_t a) { return a ? *(volatile uint64_t *)(uintptr_t)a : 0; }
static uint64_t tables(uint64_t G) { uint8_t v = *(uint8_t *)(uintptr_t)(G + 0x106); return v >= 1 && v <= 3 ? rd((uint64_t)(uintptr_t)(B + 0x2d64a18 + v * 16)) : 0; }
static int class_of(uint64_t G, const char *code4) {   // 物品码 → 统一物品类号（[数据表 + 0x258] = {条目, 条数}，每条 {u32 码, u32 类号}，同 give_item2） | item code → unified item class
    uint64_t T = tables(G), ix = T ? rd(T + 0x258) : 0, ar = ix ? rd(ix) : 0, cnt = ix ? rd(ix + 8) : 0; uint32_t code; memcpy(&code, code4, 4);
    for (uint64_t k = 0; ar && k < cnt && k < 8192; k++) if (*(uint32_t *)(uintptr_t)(ar + k * 8) == code) return *(int32_t *)(uintptr_t)(ar + k * 8 + 4);
    return -1;
}
// 清单行 → 运行时表行（见文件头「行号核对」）：返回运行时行号或 -1 | list row → runtime table row (see "Row check" in the file header): returns the runtime row or -1
static int rt_row(uint64_t G, int kind, int i) {
    uint64_t T = tables(G); if (!T) return -1;
    if (kind == DM_ITEM_RUNEWORD) {
        int sh; const dm_runeword_t *w = rwl(i, &sh); const char *key = w->key; uint64_t rr = rd(T + 0x1880); uint32_t rn = *(uint32_t *)(uintptr_t)(T + 0x1888); int r = w->row;
        if (rr && r < (int)rn && !strncmp((const char *)(uintptr_t)(rr + (uint64_t)r * 0xe8), key, 64)) return r;
        for (uint32_t k = 0; rr && k < rn && k < 4096; k++) if (!strncmp((const char *)(uintptr_t)(rr + (uint64_t)k * 0xe8), key, 64)) { LOG("刷装备：符文之语 %s 清单行 %d → 运行时行 %u", key, r, k); return (int)k; }
        LOG("【红】刷装备：运行时符文之语表里没有 %s", key); return -1;
    }
    int u = kind == DM_ITEM_UNIQUE, r = u ? dm_unique_items[i].row : dm_set_items[i].row, lvl = u ? dm_unique_items[i].lvl : dm_set_items[i].lvl;
    const char *code = u ? dm_unique_items[i].code : dm_set_items[i].code;
    uint64_t rows = rd(T + (u ? 0x13e8 : 0x13c8)), n = rd(T + (u ? 0x13f0 : 0x13d0)), sz = u ? 0x15c : 0x1cc, lo = u ? 0x38 : 0x34; if (n > 4096) n = 4096;
    if (rows && (uint64_t)r < n && !memcmp((const void *)(uintptr_t)(rows + r * sz + 0x28), code, 4)) return r;
    for (uint64_t k = 0; rows && k < n; k++) { uint64_t e = rows + k * sz;
        if (!memcmp((const void *)(uintptr_t)(e + 0x28), code, 4) && *(int16_t *)(uintptr_t)(e + lo) == lvl) { LOG("刷装备：%s %.4s 清单行 %d → 运行时行 %llu", u ? "暗金" : "套装", code, r, (unsigned long long)k); return (int)k; } }
    LOG("【红】刷装备：运行时%s表里找不到 %.4s（清单行 %d，表 %llu 行）", u ? "暗金" : "套装", code, r, (unsigned long long)n); return -1;
}

// 装备免等级需求（见文件头）：三个版本的表（[main+0x2d64a18 + 版本×16]，版本 1–3）都改，第一次开时存原值再清零，关时写回；表指针变了（重新载表）就当新表。
// 不只改游戏版本 G+0x106 那张：等级函数 0x571580 按物品自己的版本 +0x1bd 取表（0x5717e8–0x571800）。build 12.1 真机只改了前者，开关开着拉扎鲁斯的螺旋杖仍要 18 级；
// 两个版本不一致是推断，没有日志证实，所以生成后把物品版本、游戏版本和 0x571580 读回的需求等级记进日志
// No level requirement (see the file header): the tables of all three versions ([main+0x2d64a18 + version×16], versions 1-3) are changed; originals are kept on the first enable, then zeroed, and written back on disable; a changed table pointer (reloaded tables) counts as a new table.
// Not only the game version's table at G+0x106: the level function 0x571580 picks the table by the item's own version +0x1bd (0x5717e8-0x571800). Build 12.1 changed only the former, and on the device Lazarus Spire still required level 18 with the switch on;
// a mismatch of the two versions is an inference not confirmed by a log, so after creation the item version, game version and the level read back from 0x571580 are logged
// 0x571580 的全部来源（读码）：魔法 / 稀有词缀行（[数据表 + 0x1608]，条数 +0x1610，每条 0x8c；物品数据 +0x46 / +0x48 / +0x4e 等存词缀号）取 +0x65 需求等级，
// 职业专属的取 +0x68（+0x67 = 职业）；物品上的技能词缀（属性 97 / 107，0x5bdf30 按技能号找）取技能表行 +0x214（build 14.1 真机：拉扎鲁斯的螺旋杖带连锁闪电，开着开关仍要 18 级）；
// 再与暗金 +0x3a、套装 +0x36、物品表 +0x14f、孔内各件取最大。所以这几处一起清零。技能表 +0x214 同时是技能树的学习等级：开着时学技能也不受等级限制，关掉写回
// All sources of 0x571580 (from reading the code): magic / rare affix rows ([data table + 0x1608], count +0x1610, 0x8c each; item data +0x46 / +0x48 / +0x4e etc. hold the affix ids) give +0x65 required level,
// class-specific ones +0x68 (+0x67 = class); skill affixes on the item (stats 97 / 107, found by skill id via 0x5bdf30) give skill row +0x214 (build 14.1 device: Lazarus Spire carries Chain Lightning and still needed level 18 with the switch on);
// then the max with unique +0x3a, set +0x36, item table +0x14f and each socketed item. So all of these are zeroed together. Skill row +0x214 is also the skill tree's learning level: while on, skills can be learned at any level too; off writes them back
static struct { uint64_t T; int on; uint32_t nu, ns, ni, na, nk; int16_t *u, *s, *sk; uint8_t *it, *af; } g_lv[4];
static void lv_apply1(int v);
static void lv_apply(void) { for (int v = 1; v <= 3; v++) lv_apply1(v); }
static void lv_apply1(int v) {
    uint64_t T = rd((uint64_t)(uintptr_t)(B + 0x2d64a18 + v * 16)); if (!T) return;
    for (int w = 1; w < v; w++) if (g_lv[w].T == T) return;   // 两个版本共用一张表：只改一次 | two versions sharing one table: change it once
    int on = atomic_load(&g_nolv); __typeof__(&g_lv[0]) L = &g_lv[v];
    if (L->T != T) { free(L->u); free(L->s); free(L->it); free(L->af); free(L->sk); memset(L, 0, sizeof *L); L->T = T; }
    if (on == L->on) return;
    uint64_t ur = rd(T + 0x13e8), sr = rd(T + 0x13c8), ir = rd(T + 0x15c0), ar = rd(T + 0x1608), kr = rd(T + 0x11e8);
    if (!L->u) {   // 存原值 | keep the originals
        L->nu = (uint32_t)rd(T + 0x13f0); L->ns = (uint32_t)rd(T + 0x13d0); L->ni = *(uint32_t *)(uintptr_t)(T + 0x15c8);
        if (!ur || !sr || !ir || L->nu > 4096 || L->ns > 4096 || L->ni > 8192) { LOG("【红】刷装备：免等级 表不对（暗金 %u、套装 %u、物品 %u 行）", L->nu, L->ns, L->ni); L->on = on; return; }
        L->u = malloc(2 * L->nu + 2); L->s = malloc(2 * L->ns + 2); L->it = malloc(L->ni + 1); if (!L->u || !L->s || !L->it) { L->on = on; return; }
        for (uint32_t k = 0; k < L->nu; k++) L->u[k] = *(int16_t *)(uintptr_t)(ur + k * 0x15c + 0x3a);
        for (uint32_t k = 0; k < L->ns; k++) L->s[k] = *(int16_t *)(uintptr_t)(sr + k * 0x1cc + 0x36);
        for (uint32_t k = 0; k < L->ni; k++) L->it[k] = *(uint8_t *)(uintptr_t)(ir + k * 0x1c0 + 0x14f);
        L->na = (uint32_t)rd(T + 0x1610); L->nk = (uint32_t)rd(T + 0x11f0);
        if (ar && kr && L->na <= 8192 && L->nk <= 4096 && (L->af = malloc(2 * L->na + 2)) && (L->sk = malloc(2 * L->nk + 2))) {
            for (uint32_t k = 0; k < L->na; k++) { L->af[2 * k] = *(uint8_t *)(uintptr_t)(ar + k * 0x8c + 0x65); L->af[2 * k + 1] = *(uint8_t *)(uintptr_t)(ar + k * 0x8c + 0x68); }
            for (uint32_t k = 0; k < L->nk; k++) L->sk[k] = *(int16_t *)(uintptr_t)(kr + k * 0x2ec + 0x214);
        } else { LOG("【红】刷装备：免等级 词缀表 / 技能表不对（词缀 %u、技能 %u 行），这两类不改", L->na, L->nk); free(L->af); free(L->sk); L->af = NULL; L->sk = NULL; L->na = L->nk = 0; }
    }
    if (!L->u || !L->s || !L->it) { L->on = on; return; }   // 没存到原值就什么都不改 | without the originals nothing is changed
    for (uint32_t k = 0; k < L->nu; k++) *(int16_t *)(uintptr_t)(ur + k * 0x15c + 0x3a) = on ? 0 : L->u[k];
    for (uint32_t k = 0; k < L->ns; k++) *(int16_t *)(uintptr_t)(sr + k * 0x1cc + 0x36) = on ? 0 : L->s[k];
    for (uint32_t k = 0; k < L->ni; k++) *(uint8_t *)(uintptr_t)(ir + k * 0x1c0 + 0x14f) = on ? 0 : L->it[k];
    for (uint32_t k = 0; L->af && k < L->na; k++) { *(uint8_t *)(uintptr_t)(ar + k * 0x8c + 0x65) = on ? 0 : L->af[2 * k]; *(uint8_t *)(uintptr_t)(ar + k * 0x8c + 0x68) = on ? 0 : L->af[2 * k + 1]; }
    for (uint32_t k = 0; L->sk && k < L->nk; k++) *(int16_t *)(uintptr_t)(kr + k * 0x2ec + 0x214) = on ? 0 : L->sk[k];
    L->on = on; LOG("刷装备：装备免等级需求 %s（版本 %d：暗金 %u、套装 %u、物品 %u、词缀 %u、技能 %u 行）", on ? "开" : "关，原值写回", v, L->nu, L->ns, L->ni, L->na, L->nk);
}

static int gs(uint64_t u, int st) { return ((int (*)(uint64_t, int, int))(B + 0x5b9290))(u, st, 0); }

// 在玩家脚下生成一件：返回物品单位（地上，模式 3）或 0 | create one item at the player's feet: returns the item unit (on the ground, mode 3) or 0
static uint64_t make(uint64_t G, uint64_t player, const char *code4, int q, int row, int ilvl, int eth) {
    int cls = class_of(G, code4); if (cls < 0) { LOG("刷装备：物品码 %.4s 查不到类号", code4); return 0; }
    int32_t p0[2] = { 0, 0 }, p1[2] = { 0, 0 }; ((void (*)(uint64_t, int32_t *))(B + 0x5c84f0))(player, p0); uint64_t pth = rd(player + 0x38), room0 = pth ? rd(pth + 0x20) : 0;
    uint64_t room = room0 ? ((uint64_t (*)(uint64_t, uint64_t, int32_t *, int32_t *, int))(B + 0x6529e0))(G, room0, p0, p1, 1) : 0; if (!room) { LOG("刷装备：脚下找不到落点"); return 0; }
    uint8_t prm[0xd8]; memset(prm, 0, sizeof prm);
    *(uint64_t *)(prm + 0) = player; *(uint64_t *)(prm + 0x10) = G; *(int32_t *)(prm + 0x18) = ilvl < 99 ? 99 : ilvl; *(int32_t *)(prm + 0x20) = cls; *(int32_t *)(prm + 0x24) = 3;
    *(int32_t *)(prm + 0x28) = p1[0]; *(int32_t *)(prm + 0x2c) = p1[1]; *(uint64_t *)(prm + 0x30) = room; *(uint16_t *)(prm + 0x38) = 1; *(uint16_t *)(prm + 0x3a) = *(uint16_t *)(uintptr_t)(G + 0x128); *(int32_t *)(prm + 0x40) = q;
    if (row >= 0) { int32_t r1 = row + 1; memcpy(prm + 0x50, &r1, 4); }
    *(uint32_t *)(prm + 0xc0) = (eth ? 4 : 2) | 8;   // 生成控制：bit1 不出无形 / bit2 强制无形（佣兵件）、bit3 不随机打孔（孔数由 make_ready 指定） | creation control: bit1 no ethereal / bit2 force ethereal (mercenary pieces), bit3 no random sockets (make_ready sets the count)
    if (row >= 0 && q == 7) { uint32_t *w = (uint32_t *)(uintptr_t)(G + 0x3638 + 4 * ((uint32_t)row >> 5)); *w &= ~(1u << (row & 31)); }
    uint64_t it = ((uint64_t (*)(uint64_t, void *, int))(B + 0x656780))(G, prm, 0); uint64_t d = it ? rd(it + 0x10) : 0;
    LOG("刷装备：生成 %.4s 品质 %d 行 %d → %s（读回品质 %d、行 %d）", code4, q, row, it ? "成" : "【红】败", d ? *(int32_t *)(uintptr_t)d : -1, d ? *(int32_t *)(uintptr_t)(d + 0x34) : -1);
    return it;
}
// 去力量 / 敏捷需求：属性 91 的有效值调到 −100，按「有效 − 现值」差补，免得和物品自带的减需叠错。
// 写进物品自己的词缀表（0x5bcf20(物品, 0, 0x40)，没有就建）：存档写的是这张表；0x5bc1f0 改的基础表不进存档（stat_re 报告第 2 节），build 13 及以前读档会丢
// Remove strength / dexterity requirements: bring stat 91's effective value to -100 by the difference "effective - current", so it does not stack wrongly with the item's own reduction.
// Written into the item's own affix list (0x5bcf20(item, 0, 0x40), created if absent): the save writes this list; the base list that 0x5bc1f0 changes is not saved (stat_re report section 2), so build 13 and earlier lose it on reload
// 已成词的物品写进原版成词时建好的那张表（state 0xab，同样存档），不给普通底材另建物品词缀表 | a formed runeword is written into the list the original built when the word formed (state 0xab, also saved), instead of creating an item affix list on a normal base
static void no_req(uint64_t it) {
    uint64_t d0 = rd(it + 0x10); int rw = d0 && (*(uint32_t *)(uintptr_t)(d0 + 0x18) & 0x04000000);
    uint64_t M = affix_list(it, rw); uint8_t v = *(uint8_t *)(uintptr_t)(it + 0x1bd);
    if (!M) { LOG("【红】刷装备：找不到词缀表，力量 / 敏捷需求没去"); return; }
    int cur = ((int (*)(uint8_t, uint64_t, int, int))(B + 0x5bc690))(v, M, 91, 0), total = gs(it, 91), want = cur + (-100 - total);
    if (want < -100) want = -100;
    if (want > 155) want = 155;
    if (want != cur) ((int (*)(uint8_t, uint64_t, int, int, int))(B + 0x5bbe20))(v, M, 91, want, 0);
}
// 捡进背包：返回 1 = 进了（模式 0），0 = 还在地上 | pick up into the inventory: 1 = in (mode 0), 0 = still on the ground
static int pick_up(uint64_t G, uint64_t player, uint64_t it) {
    uint64_t ip = rd(it + 0x38); if (!ip) return 0; uint8_t pk[17] = { 0x16 }; uint32_t one = 1;
    memcpy(pk + 1, (void *)(uintptr_t)(it + 8), 4); memcpy(pk + 5, (void *)(uintptr_t)(ip + 0x10), 4); memcpy(pk + 9, (void *)(uintptr_t)(ip + 0x14), 4); memcpy(pk + 13, &one, 4);
    uint32_t *busy = (uint32_t *)(uintptr_t)(G + 0x19050), old = *busy; *busy = 1;
    ((void (*)(uint64_t, uint64_t, uint8_t *, uint32_t))(B + 0x6e5b40))(G, player, pk, 17); *busy = old;
    return *(uint32_t *)(uintptr_t)(it + 0xc) == 0;
}
// 物品等级至少 99，也不低于目标行的物品等级（毁灭、地狱火炬是 110；原版指定生成要求物品等级 ≥ 行等级） | item level at least 99 and not below the target row's level (Annihilus and Hellfire Torch are 110; forced creation requires item level >= row level)
// 整件重发给客户端：清掉「已鉴定」位再走一次原版鉴定 0x78acf0，它会置回这一位并自己广播物品更新（0x9c / 0x9d，见 diag.c 免费鉴定）
// Resend the whole item to the client: clear the identified bit and run the original identify 0x78acf0 again; it sets the bit back and broadcasts the item update itself (0x9c / 0x9d, see free identify in diag.c)
static void resend(uint64_t G, uint64_t player, uint64_t it) {
    uint64_t d = rd(it + 0x10); if (!d) return; *(uint32_t *)(uintptr_t)(d + 0x18) &= ~0x10u;
    int r = ((int (*)(uint64_t, uint64_t, uint64_t))(B + 0x78acf0))(G, player, it);
    LOG("刷装备：重发物品 id %u → %d（已鉴定位 %s）", *(uint32_t *)(uintptr_t)(it + 8), r, *(uint32_t *)(uintptr_t)(d + 0x18) & 0x10 ? "已置回" : "【红】没置回");
}
static uint64_t make_ready2(uint64_t G, uint64_t player, const char *code4, int q, int row, int ilvl, int sockets, int eth, int *in_bag) {
    uint64_t it = make(G, player, code4, q, row, ilvl, eth); if (!it) return 0;
    if (sockets > 0) { ((void (*)(uint64_t, int))(B + 0x5754a0))(it, sockets); LOG("刷装备：%.4s 设 %d 孔 → 读回 %d", code4, sockets, gs(it, 194)); }
    ((int (*)(uint64_t, uint64_t, uint64_t))(B + 0x78acf0))(G, player, it);
    // 符文之语底材（普通品质、带孔）镶嵌前不写词缀、不去需求：普通底材本来没有物品词缀表，提前写会新建一张，真机上镶到成词的最后一颗时被原版退回光标
    // （build 14.1 日志：正义之手前 3 颗镶进、第 4 颗留在光标）。成词以后再写（lo_sock 末尾 / rw_post），并重发整件
    // Runeword bases (normal quality, socketed) get no affixes and no requirement removal before socketing: a normal base has no item affix list, writing early creates one, and on the device the last rune that would form the word was returned to the cursor
    // (build 14.1 log: Hand of Justice took 3 runes, the 4th stayed on the cursor). They are written after the word forms (end of lo_sock / rw_post), and the whole item is resent
    int rw_base_item = q == 2 && sockets > 0;
    if (g_aff && rw_base_item) g_aff = NULL;
    if (g_aff) { const struct lo_aff *a = g_aff; g_aff = NULL; if (!apply_list(it, 0, a)) g_aff_bad = 1; }   // 一键配装：捡起前写词缀，捡起时整件发给客户端（在去需求之前：定制件要先清表） | loadout: affixes written before pick-up, so the whole item goes to the client on pick-up (before the requirement removal: custom pieces wipe the list first)
    if (q != 2) no_req(it);   // 符文本身没有力量 / 敏捷需求，不写 | runes have no strength / dexterity requirement, left alone
    LOG("刷装备：%.4s 物品版本 %u（游戏 %u），需求等级 %d，属性 91 有效值 %d", code4, *(uint8_t *)(uintptr_t)(it + 0x1bd), *(uint8_t *)(uintptr_t)(G + 0x106),
        ((int (*)(uint64_t, uint64_t))(B + 0x571580))(it, player), gs(it, 91));
    *in_bag = pick_up(G, player, it);
    return it;
}
static uint64_t make_ready(uint64_t G, uint64_t player, const char *code4, int q, int row, int ilvl, int sockets, int *in_bag) { return make_ready2(G, player, code4, q, row, ilvl, sockets, 0, in_bag); }

static void spawn(uint64_t G, uint64_t player, int kind, int i) {
    char msg[200]; const char *nm = dm_itemgen_name(kind, i); int in = 0;
    if (kind == DM_ITEM_UNIQUE || kind == DM_ITEM_SET) {
        const char *code = kind == DM_ITEM_UNIQUE ? dm_unique_items[i].code : dm_set_items[i].code; int row = rt_row(G, kind, i), q = kind == DM_ITEM_UNIQUE ? 7 : 5;
        if (row < 0) { snprintf(msg, sizeof msg, dm_str(B, "dmItemFail"), nm); dm_game_notice(B, msg); return; }
        int lvl = kind == DM_ITEM_UNIQUE ? dm_unique_items[i].lvl : dm_set_items[i].lvl;
        uint64_t it = make_ready(G, player, code, q, row, lvl, 0, &in); uint64_t d = it ? rd(it + 0x10) : 0;
        if (!it || !d || *(int32_t *)(uintptr_t)d != q || *(int32_t *)(uintptr_t)(d + 0x34) != row) { snprintf(msg, sizeof msg, dm_str(B, "dmItemFail"), nm); dm_game_notice(B, msg); return; }
        snprintf(msg, sizeof msg, dm_str(B, in ? "dmItemDone" : "dmItemGround"), nm); dm_game_notice(B, msg); return;
    }
    int sh; const dm_runeword_t *w = rwl(i, &sh); int n = w->nsock, rec = rt_row(G, kind, i); const char *bc = rw_base(w, sh);
    if (n < 1 || !memcmp(bc, "    ", 4) || rec < 0) { snprintf(msg, sizeof msg, dm_str(B, "dmItemFail"), nm); dm_game_notice(B, msg); return; }
    uint64_t base = make_ready(G, player, bc, 2, -1, 0, n, &in);
    if (!base || gs(base, 194) != n || !in) { snprintf(msg, sizeof msg, dm_str(B, base && !in ? "dmItemNoRoom" : "dmItemFail"), nm); dm_game_notice(B, msg); return; }
    for (int k = 0; k < n; k++) { int rin = 0; uint64_t r = make_ready(G, player, w->runes[k], 2, -1, 0, 0, &rin);
        if (!r || !rin) { snprintf(msg, sizeof msg, dm_str(B, r ? "dmItemNoRoom" : "dmItemFail"), nm); dm_game_notice(B, msg); return; } }
    dm_runeword_auto(*(uint32_t *)(uintptr_t)(base + 8), rec);
    g_rwpend = *(uint32_t *)(uintptr_t)(base + 8); g_rwpend_at = *(uint32_t *)(uintptr_t)(G + 0x170);
    snprintf(msg, sizeof msg, dm_str(B, "dmItemRwOpen"), nm); dm_game_notice(B, msg);
}

// ---------- 一键配装：生成一整套、镶好符文、穿上（含副手武器组），换下来的放进背包 ----------
// 依据 C:\Users\miiro\agy_tasks\20261025_d2r_equip_re\report.md（静态逆向，未真机）：
//   收包分发 0x6e5b40 同步执行，返回后服务端位置已变；0x19 格子 → 光标、0x18 光标 → 格子（17 字节 {op, id, x, y, 页}），
//   0x1a 光标 → 空装备位、0x1d 光标与装备位互换（9 / 13 字节，装备位只收 1–10），0x28 镶嵌（21 字节，背包里的底材：{op, 符文, 底材, 0, 页, x | y << 16}），
//   0x50 切换武器组（30 字节，带两组武器 id 与左右技能快照，组号须与当前一致）；副手组只能先切过去再按 4 / 5 穿。
//   找空格：原版 0x55f620(背包, 物品, 格区, &x, &y, 页)，个人背包格区 = [main+0x1dae684 + 职业 × 8 + 4]。
//   光标上有东西而物品面板没开时客户端会自己发 0x17 丢地上，所以要求物品栏开着，且每步「拿起 → 放下」在同一拍里做完。
// One-click loadout: create a whole set, socket the runes, equip it (including the secondary weapon set), put what comes off into the inventory.
// Basis C:\Users\miiro\agy_tasks\20261025_d2r_equip_re\report.md (static reverse engineering, not device-tested):
//   the receive dispatcher 0x6e5b40 runs synchronously and the server position has changed when it returns; 0x19 cell -> cursor, 0x18 cursor -> cell (17 bytes {op, id, x, y, page}),
//   0x1a cursor -> empty equipment slot, 0x1d swap cursor with an equipment slot (9 / 13 bytes, slots 1-10 only), 0x28 socketing (21 bytes, base in the inventory: {op, rune, base, 0, page, x | y << 16}),
//   0x50 weapon set switch (30 bytes, with both sets' weapon ids and the left / right skill snapshot; the set number must match the current one); the secondary set can only be equipped by switching to it and using 4 / 5.
//   Free cell: the original 0x55f620(inventory, item, grid record, &x, &y, page), personal inventory grid record = [main+0x1dae684 + class × 8 + 4].
//   With something on the cursor and no item panel open, the client sends 0x17 by itself and drops it, so the inventory must be open, and each "pick up -> put down" finishes within one tick.
// 词缀（满值 / 定制）依据 C:\Users\miiro\agy_tasks\20261025_d2r_stat_re\report.md：存档写的是物品自己的词缀表，不是 0x5bc1f0 改的基础表——
//   暗金 / 魔法 / 稀有 = 0x5bcf20(物品, 0, 0x40)（表 +0x20 须为 0；没有就 0x56be80(0, 物品, 0, 0x40) 建），符文之语 = 0x5bcf20(物品, 0xab, 0)；
//   改值 0x5bbe20(物品版本 +0x1bd, 表, 属性, 值, layer)（同键覆盖，值 0 = 删掉那条），读 0x5bc690(版本, 表, 属性, layer)；
//   表 +0x30 条目数组（每条 8 字节：u16 layer、u16 属性、i32 值）、+0x38 条数。layer：职业技能 = 职业号，单技能 / 光环 = 技能号，技能树 = 职业 × 8 + 树，
//   触发施放 = 技能 << 6 | 等级，充能 = 技能 << 6 | 等级、值 = 最大 << 8 | 当前。手工品质 8 没完全证实，生成后读回品质，不对就当稀有用（词缀一样，只是不显示橙色）。
// Affixes (max rolls / custom) per C:\Users\miiro\agy_tasks\20261025_d2r_stat_re\report.md: the save writes the item's own affix list, not the base list that 0x5bc1f0 changes -
//   unique / magic / rare = 0x5bcf20(item, 0, 0x40) (list +0x20 must be 0; create with 0x56be80(0, item, 0, 0x40) if absent), runeword = 0x5bcf20(item, 0xab, 0);
//   set a value with 0x5bbe20(item version +0x1bd, list, stat, value, layer) (same key overwrites, value 0 deletes the entry), read with 0x5bc690(version, list, stat, layer);
//   list +0x30 entry array (8 bytes each: u16 layer, u16 stat, i32 value), +0x38 count. layer: class skills = class, single skill / aura = skill, skill tab = class × 8 + tab,
//   chance to cast = skill << 6 | level, charges = skill << 6 | level with value = max << 8 | current. Crafted quality 8 is not fully confirmed; the quality is read back after creation, and if it differs the item is used as a rare (same affixes, just not orange).
enum { LK_UNIQUE, LK_RUNEWORD, LK_CUSTOM };
struct lo_aff { const char *what; const lo_stat_t *st; int n, clear; };
struct lo_piece {
    const char *what; uint8_t slot, set2, merc, kind, q, eth, nsock;   // nsock：暗金另打的孔（岁月之冠 2 孔） | nsock: sockets added to a unique (Crown of Ages: 2)
    int16_t uid; const char *rwkey; const char *base;
    const char *fill[6];             // 镶嵌物：符文码，或 "#行号" = 暗金珠宝；符文之语留空按表 | socket fillers: rune code, or "#row" = unique jewel; empty for runewords (taken from the table)
    const lo_stat_t *st; uint8_t nst;    // 词缀：暗金 / 定制写进物品词缀表，符文之语写进成词那张表 | affixes: unique / custom into the item affix list, runeword into the runeword list
    const lo_stat_t *bst; uint8_t nbst;  // 符文之语底材自带（超强、法杖技能、盾抗），写进底材词缀表 | runeword base extras (superior, staff skills, shield resists), into the base affix list
    const lo_stat_t *fst; uint8_t nfst;  // 镶嵌的暗金珠宝要写的词缀 | affixes for the socketed unique jewel
    uint8_t clear;                       // 先清掉随机词缀（定制件、新生火焰裂隙） | wipe the random affixes first (custom pieces, Renewed Flame Rift)
};
#include "dm_loadout_tab.h"   // k_lo[]：这一套的件（dm/ios/tools/gen_loadout.py 生成） | k_lo[]: the pieces of this set (generated by dm/ios/tools/gen_loadout.py)
#define NLO (int)(sizeof k_lo / sizeof k_lo[0])
static _Atomic int g_lo_req, g_lo_busy;   // g_lo_busy：1 = 配装占着光标，2 = 交给一键合成等成词（一键合成可动光标） | 1 = the loadout owns the cursor, 2 = waiting on one-tap crafting (which may use the cursor)
void dm_itemgen_loadout(void) { atomic_store(&g_lo_req, 1); }
int dm_itemgen_busy(void) { return atomic_load(&g_lo_busy) == 1; }   // dm_runeword.c 的一键合成在配装期间不动光标 | dm_runeword.c's one-tap crafting keeps off the cursor during a loadout
enum { LO_IDLE, LO_MAKE, LO_SOCK, LO_SWITCH, LO_EQUIP, LO_BACK, LO_RWJOB };   // LO_RWJOB：符文之语交给一键合成镶（见 lo_rwjob） | LO_RWJOB: a runeword is socketed by one-tap crafting (see lo_rwjob)
// 任务绑定这一局与这个角色（G、玩家 id），换局 / 换角色作废；resume = 背包满停下时的阶段，腾出空位再点接着做
// The job is bound to this game and character (G, player id) and dropped on a change; resume = the phase it stopped in on a full inventory, continued on the next tap
static struct { int st, i, k, nr, closed, nomerc, resume, backtry, cursor_warned; uint64_t G; uint32_t pid, base, rune[6]; } J;

static uint64_t affix_list(uint64_t it, int rw) {
    uint64_t M = ((uint64_t (*)(uint64_t, int, uint32_t))(B + 0x5bcf20))(it, rw ? 0xab : 0, rw ? 0 : 0x40);
    if (!M && !rw) M = ((uint64_t (*)(uint64_t, uint64_t, int, uint32_t))(B + 0x56be80))(0, it, 0, 0x40);
    return M && *(uint32_t *)(uintptr_t)(M + 0x20) == (uint32_t)(rw ? 0xab : 0) ? M : 0;
}
// 写一组词缀：先按需清空；职业技能 83 / 技能树 188 写入前删掉同属性别的 layer（地狱火炬随机职业）；写完逐条读回。返回 0 = 有问题
// Write a group of affixes: wipe first if asked; for class skills 83 / skill tab 188, other layers of the same stat are deleted first (Hellfire Torch's random class); read back each one. Returns 0 on any problem
static int apply_list(uint64_t it, int rw, const struct lo_aff *a) {
    uint64_t M = affix_list(it, rw); uint8_t v = *(uint8_t *)(uintptr_t)(it + 0x1bd); if (!M) { LOG("【红】一键配装：「%s」找不到词缀表", a->what); return 0; }
    int (*set)(uint8_t, uint64_t, int, int, int) = (int (*)(uint8_t, uint64_t, int, int, int))(B + 0x5bbe20);
    int (*get)(uint8_t, uint64_t, int, int) = (int (*)(uint8_t, uint64_t, int, int))(B + 0x5bc690);
    int cleared = 0, bad = 0;
    for (int g = 0; a->clear && g < 64 && rd(M + 0x38); g++) {   // 每次删当前第 0 条（删除会挪动数组，不按旧下标走） | delete entry 0 each time (deletion shifts the array)
        uint64_t n = rd(M + 0x38), e = rd(M + 0x30); if (!e) break;
        uint16_t layer = *(uint16_t *)(uintptr_t)e, stat = *(uint16_t *)(uintptr_t)(e + 2); set(v, M, stat, 0, layer);
        if (rd(M + 0x38) >= n) { LOG("【红】一键配装：「%s」删不掉属性 %u/%u", a->what, stat, layer); bad++; break; } cleared++; }
    if (a->clear && rd(M + 0x38)) { LOG("【红】一键配装：「%s」词缀表没清空（剩 %llu 条）", a->what, (unsigned long long)rd(M + 0x38)); bad++; }
    for (int k = 0; k < a->n; k++) { const lo_stat_t *s = &a->st[k];
        if (s->stat == 83 || s->stat == 188) for (int g = 0; g < 16; g++) {   // 同属性别的 layer | other layers of the same stat
            uint64_t n = rd(M + 0x38), e = rd(M + 0x30), hit = 0; for (uint64_t j = 0; e && j < n && j < 64; j++) { uint16_t l = *(uint16_t *)(uintptr_t)(e + 8 * j), st = *(uint16_t *)(uintptr_t)(e + 8 * j + 2); if (st == s->stat && l != s->layer) { set(v, M, st, 0, l); hit = 1; break; } }
            if (!hit) break; }
        set(v, M, s->stat, s->value, s->layer); if (get(v, M, s->stat, s->layer) != s->value) { LOG("【红】一键配装：「%s」属性 %u/%u 读回 %d（要 %d）", a->what, s->stat, s->layer, get(v, M, s->stat, s->layer), s->value); bad++; } }
    LOG("一键配装：「%s」%s词缀表：清掉 %d 条，写 %d 条，问题 %d 处", a->what, rw ? "符文之语" : "物品", cleared, a->n, bad);
    return !bad;
}

static void w32(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }
static void w16(uint8_t *p, uint16_t v) { memcpy(p, &v, 2); }
static int dispatch(uint64_t G, uint64_t pl, const uint8_t *p, uint32_t n) {   // 照原版收包点：分发期间 G+0x19050 = 1；返回 0 = 处理器收下 | as the original receive point: G+0x19050 = 1 while dispatching; 0 = accepted by the handler
    uint32_t *busy = (uint32_t *)(uintptr_t)(G + 0x19050), old = *busy; *busy = 1;
    int rc = ((int (*)(uint64_t, uint64_t, const uint8_t *, uint32_t))(B + 0x6e5b40))(G, pl, p, n); *busy = old; return rc;
}
static int pk_grid(uint64_t G, uint64_t pl, int op, uint32_t id, int x, int y, int page) { uint8_t p[17] = { (uint8_t)op }; w32(p + 1, id); w32(p + 5, (uint32_t)x); w32(p + 9, (uint32_t)y); w32(p + 13, (uint32_t)page); return dispatch(G, pl, p, 17); }
static uint64_t inv_first(uint64_t u) { uint64_t c = rd(u + 0x90); return c && *(uint32_t *)(uintptr_t)c == 0x01020304 ? rd(c + 0x10) : 0; }
static uint64_t own_item(uint64_t pl, uint32_t id) {   // 玩家自己挂着的物品里按 id 找（背包、身上、光标） | find by id among the player's own items (inventory, equipped, cursor)
    int n = 0; for (uint64_t it = inv_first(pl); it && n < 1024; n++) { uint64_t d = rd(it + 0x10); if (!d) break; if (*(uint32_t *)(uintptr_t)(it + 8) == id) return it; it = rd(d + 0xb0); }
    return 0;
}
static uint64_t equipped(uint64_t u, int slot) {
    int n = 0; for (uint64_t it = inv_first(u); it && n < 1024; n++) { uint64_t d = rd(it + 0x10); if (!d) break; if (*(uint32_t *)(uintptr_t)(it + 0xc) == 1 && *(uint8_t *)(uintptr_t)(d + 0x54) == slot) return it; it = rd(d + 0xb0); }
    return 0;
}
static uint64_t cursor_item(uint64_t pl) { uint64_t inv = rd(pl + 0x90); return inv ? rd(inv + 0x40) : 0; }   // 玩家光标上的物品（merc_re 3.2：背包 +0x40） | item on the player's cursor (merc_re 3.2: inventory +0x40)
static int mode_of(uint64_t it) { return it ? (int)*(uint32_t *)(uintptr_t)(it + 0xc) : -1; }
static int cell_of(uint64_t it, int *x, int *y) {   // 背包页 0 的格子坐标（路径 +0x10 / +0x14，同 dm_runeword.c grid_xy） | grid cell on inventory page 0
    uint64_t d = rd(it + 0x10), pp = rd(it + 0x38); if (!d || !pp || mode_of(it) != 0 || *(uint8_t *)(uintptr_t)(d + 0x55) != 0) return 0;
    *x = *(int32_t *)(uintptr_t)(pp + 0x10); *y = *(int32_t *)(uintptr_t)(pp + 0x14); return 1;
}
static int find_free(uint64_t pl, uint64_t it, uint32_t *x, uint32_t *y) {
    uint32_t cls = *(uint32_t *)(uintptr_t)(pl + 4); if (cls > 7) return 0; int grid = *(int32_t *)(uintptr_t)(B + 0x1dae684 + cls * 8 + 4);
    return ((int (*)(uint64_t, uint64_t, int, uint32_t *, uint32_t *, uint8_t))(B + 0x55f620))(rd(pl + 0x90), it, grid, x, y, 0) & 1;
}
// 光标上有东西就放进背包空格：返回 1 = 光标已空 | put whatever is on the cursor into a free inventory cell: 1 = cursor empty
// 光标上的东西丢到脚下：0x17 {物品 id}（客户端没有物品面板时自己丢东西用的就是它，见 dm_runeword.c item_panel_open）| drop the cursor item at the player's feet: 0x17 {item id}
static int drop_cursor(uint64_t G, uint64_t pl, uint64_t it) {
    uint32_t id = *(uint32_t *)(uintptr_t)(it + 8); uint8_t p[5] = { 0x17 }; w32(p + 1, id); dispatch(G, pl, p, 5);
    int ok = !cursor_item(pl); LOG("一键配装：光标上的 id %u 丢到脚下 %s", id, ok ? "成" : "【红】败"); return ok;
}
static int clear_cursor(uint64_t G, uint64_t pl) {
    uint64_t it = cursor_item(pl); uint32_t x = 0, y = 0; if (!it) return 1;
    if (find_free(pl, it, &x, &y)) pk_grid(G, pl, 0x18, *(uint32_t *)(uintptr_t)(it + 8), (int)x, (int)y, 0);
    else return drop_cursor(G, pl, it);   // 背包放不下（多半是换下来的旧装备）：丢到脚下，不卡住配装 | no room in the bag (usually a replaced piece): drop it, do not stall the loadout
    if (!cursor_item(pl)) { LOG("一键配装：光标上的 id %u 放回背包 %u,%u", *(uint32_t *)(uintptr_t)(it + 8), x, y); return 1; }
    return 0;
}
static int cur_set(uint64_t pl) { uint64_t pd = rd(pl + 0x10), sc = pd ? rd(pd + 0xf0) : 0; return sc ? *(uint8_t *)(uintptr_t)(sc + 0x5a0) & 1 : -1; }
static int switch_set(uint64_t G, uint64_t pl) {   // 0x50：按服务端此刻的两组武器与左右技能拼（equip_re 1.3） | 0x50 built from the server's current weapons of both sets and left / right skills
    uint8_t p[30] = { 0x50 }; int slots[4] = { 5, 4, 12, 11 };
    for (int k = 0; k < 4; k++) { uint64_t it = equipped(pl, slots[k]); w32(p + 1 + 4 * k, it ? *(uint32_t *)(uintptr_t)(it + 8) : 0xffffffffu); }
    uint64_t sk = rd(pl + 0x100);
    for (int k = 0; k < 2; k++) { uint64_t nd = sk ? rd(sk + 8 + 8 * k) : 0, rec = nd ? rd(nd) : 0;
        w32(p + 0x11 + 4 * k, nd ? *(uint32_t *)(uintptr_t)(nd + 0x44) : 0xffffffffu); w16(p + 0x19 + 2 * k, rec ? *(uint16_t *)(uintptr_t)rec : 0xffff); }
    int was = cur_set(pl); p[0x1d] = (uint8_t)(was & 1);
    int rc = dispatch(G, pl, p, 30), now = cur_set(pl);
    LOG("一键配装：切换武器组 %d → %d（分发返回 %d）", was, now, rc); return now >= 0 && now != was;
}
static void lo_notice(const char *key, const char *what) { char msg[200]; snprintf(msg, sizeof msg, dm_str(B, key), what ? what : ""); dm_game_notice(B, msg); }
// 停：背包满时记下阶段，腾出空位再点从这里接着做（生成阶段会把这一件重做，背包里已有的半成品不会自动复用）
// Stop: on a full inventory remember the phase, so the next tap continues here (the creation phase redoes this piece; leftovers already in the inventory are not reused)
static void lo_stop(const char *key, const char *what) {
    lo_notice(key, what); J.resume = !strcmp(key, "dmLoNoRoom") && J.st != LO_IDLE && J.st != LO_BACK ? J.st : 0;
    LOG("一键配装：停在第 %d 件「%s」（%s%s）", J.i, what ? what : "", key, J.resume ? "，腾出空位再点接着做" : ""); J.st = J.st == LO_IDLE ? LO_IDLE : LO_BACK;
}

static int unique_index(int uid) { for (int k = 0; k < DM_UNIQUE_ITEMS_COUNT; k++) if (dm_unique_items[k].row == uid) return k; return -1; }
// 一件镶嵌物进背包：符文码，或 "#行号" = 暗金珠宝（彩虹魔石），珠宝可带要写的词缀 | one socket filler into the inventory: rune code, or "#row" = unique jewel (Rainbow Facet), optionally with affixes
static uint64_t lo_filler(uint64_t G, uint64_t pl, const char *f, const struct lo_aff *aff, int *in) {
    if (f[0] != '#') return make_ready(G, pl, f, 2, -1, 0, 0, in);
    int i = unique_index(atoi(f + 1)), row = i >= 0 ? rt_row(G, DM_ITEM_UNIQUE, i) : -1; if (row < 0) return 0;
    g_aff = aff && aff->n ? aff : NULL; uint64_t r = make_ready(G, pl, dm_unique_items[i].code, 7, row, dm_unique_items[i].lvl, 0, in); g_aff = NULL; return r;
}
// 生成第 i 件进背包：暗金直接成品；符文之语 = 带孔底材 + 各颗符文（下一步逐颗镶）；定制件清空随机词缀后照写
// Create piece i into the inventory: a unique comes finished; a runeword = socketed base + each rune (socketed one by one next); a custom piece is wiped and written as given
static int lo_make(uint64_t G, uint64_t pl, const lo_piece_t *pc) {
    int in = 0, want = 0; uint64_t it = 0; const dm_runeword_t *w = NULL; const char *fill[6] = { 0 };
    struct lo_aff a = { pc->what, pc->st, pc->nst, pc->clear }, ab = { pc->what, pc->bst, pc->nbst, 0 }, af = { pc->what, pc->fst, pc->nfst, 0 };
    g_aff_bad = 0;
    if (pc->kind == LK_UNIQUE) {
        int i = unique_index(pc->uid), row = i >= 0 ? rt_row(G, DM_ITEM_UNIQUE, i) : -1; if (row < 0) { lo_stop("dmLoFail", pc->what); return 0; }
        g_aff = pc->nst || pc->clear ? &a : NULL;   // 满值在捡起前写进暗金词缀表 | max rolls go into the unique affix list before pick-up
        it = make_ready2(G, pl, pc->base ? pc->base : dm_unique_items[i].code, 7, row, dm_unique_items[i].lvl, pc->nsock, pc->eth, &in); want = 7;
        J.nr = pc->nsock; memcpy(fill, pc->fill, sizeof fill);
    } else if (pc->kind == LK_RUNEWORD) {
        for (int k = 0; k < DM_RUNEWORDS_COUNT; k++) if (!strcmp(dm_runewords[k].key, pc->rwkey)) w = &dm_runewords[k];
        if (!w || w->nsock < 1) { lo_stop("dmLoFail", pc->what); return 0; }
        g_aff = pc->nbst ? &ab : NULL;   // 底材自带的写进底材词缀表 | base extras into the base affix list
        it = make_ready2(G, pl, pc->base ? pc->base : w->base, 2, -1, 0, w->nsock, pc->eth, &in); want = 2;
        J.nr = w->nsock; for (int k = 0; k < w->nsock; k++) fill[k] = w->runes[k];
    } else {
        g_aff = &a; it = make_ready2(G, pl, pc->base, pc->q, -1, 99, 0, pc->eth, &in); want = 0; J.nr = 0;
    }
    g_aff = NULL;
    uint64_t d = it ? rd(it + 0x10) : 0; int got = d ? *(int32_t *)(uintptr_t)d : -1;
    if (!it || (want && got != want) || (!want && got < 4)) { LOG("一键配装：「%s」生成品质 %d（要 %d）", pc->what, got, want ? want : pc->q); lo_stop("dmLoFail", pc->what); return 0; }
    if (!want && got != pc->q) LOG("一键配装：「%s」要品质 %d，生成成了 %d（词缀照写）", pc->what, pc->q, got);
    if (!in) { lo_stop("dmLoNoRoom", pc->what); return 0; }
    if (g_aff_bad) { lo_stop("dmLoFail", pc->what); return 0; }
    if (J.nr && gs(it, 194) != J.nr) { LOG("一键配装：「%s」孔数 %d（要 %d）", pc->what, gs(it, 194), J.nr); lo_stop("dmLoFail", pc->what); return 0; }
    J.base = *(uint32_t *)(uintptr_t)(it + 8);
    for (int k = 0; k < J.nr; k++) { int rin = 0; uint64_t r = fill[k] ? lo_filler(G, pl, fill[k], fill[k][0] == '#' ? &af : NULL, &rin) : 0;
        if (!r || g_aff_bad) { lo_stop("dmLoFail", pc->what); return 0; } if (!rin) { lo_stop("dmLoNoRoom", pc->what); return 0; }
        J.rune[k] = *(uint32_t *)(uintptr_t)(r + 8); }
    return 1;
}
// 镶第 k 颗：0x19 符文到光标 → 0x28 镶进背包里的底材，同一拍；镶完写符文之语满值 | socket rune k: 0x19 rune to the cursor -> 0x28 into the base in the inventory, same tick; runeword max rolls once done
static int lo_sock(uint64_t G, uint64_t pl, const lo_piece_t *pc) {
    uint64_t r = own_item(pl, J.rune[J.k]), b = own_item(pl, J.base); int rx, ry, bx, by;
    if (!r || !b || !cell_of(r, &rx, &ry) || !cell_of(b, &bx, &by)) { LOG("一键配装：第 %d 颗镶嵌物或底材不在背包格里", J.k + 1); lo_stop("dmLoFail", pc->what); return 0; }
    int rc1 = pk_grid(G, pl, 0x19, J.rune[J.k], rx, ry, 0);
    uint8_t p[21] = { 0x28 }; w32(p + 1, J.rune[J.k]); w32(p + 5, J.base); w32(p + 9, 0); w32(p + 13, 0); w32(p + 17, (uint32_t)bx | (uint32_t)by << 16);
    int rc2 = mode_of(r) == 4 ? dispatch(G, pl, p, 21) : -1, m = mode_of(r);
    LOG("一键配装：「%s」第 %d 颗（id %u）→ 拿起 %d、镶 %d，模式 %d", pc->what, J.k + 1, J.rune[J.k], rc1, rc2, m);
    if (m != 6) { lo_stop("dmLoFail", pc->what); return 0; }   // 光标上剩的东西由 lo_tick 放回背包 | anything left on the cursor is put back by lo_tick
    if (++J.k == J.nr) { uint64_t d = rd(b + 0x10); int rw = d && (*(uint32_t *)(uintptr_t)(d + 0x18) & 0x04000000);
        LOG("一键配装：「%s」镶完，成词标志 %s", pc->what, rw ? "有" : pc->kind == LK_RUNEWORD ? "【红】没有" : "（不是符文之语）");
        if (pc->kind == LK_RUNEWORD && !rw) { lo_stop("dmLoFail", pc->what); return 0; }
        if (pc->kind == LK_RUNEWORD) { struct lo_aff ab = { pc->what, pc->bst, pc->nbst, 0 };   // 成词后再写底材自带词缀与去需求（见 make_ready2） | base extras and requirement removal only after the word forms (see make_ready2)
            if (pc->nbst && !apply_list(b, 0, &ab)) { lo_stop("dmLoFail", pc->what); return 0; }
            no_req(b); }
        struct lo_aff a = { pc->what, pc->st, pc->nst, 0 };   // 满值写进成词那张表；穿上时整件重发给客户端 | max rolls into the runeword list; the whole item is resent to the client when equipped
        if (pc->kind == LK_RUNEWORD && pc->nst && !apply_list(b, 1, &a)) { lo_stop("dmLoFail", pc->what); return 0; } }
    return 1;
}
// 穿：0x19 新件到光标 → 空位 0x1a / 有旧件 0x1d 互换 → 0x18 旧件进背包空格，同一拍 | equip: 0x19 new piece to the cursor -> 0x1a into an empty slot / 0x1d swap with the old one -> 0x18 old one into a free cell, same tick
static int lo_equip(uint64_t G, uint64_t pl, const lo_piece_t *pc) {
    uint64_t it = own_item(pl, J.base); int nx, ny; if (!it || !cell_of(it, &nx, &ny)) { lo_stop("dmLoFail", pc->what); return 0; }
    int slot = pc->slot; uint64_t old = equipped(pl, slot); uint32_t ox = 0, oy = 0;
    pk_grid(G, pl, 0x19, J.base, nx, ny, 0);
    if (mode_of(it) != 4) { LOG("一键配装：「%s」拿不起来（模式 %d）", pc->what, mode_of(it)); lo_stop("dmLoFail", pc->what); return 0; }
    (void)ox; (void)oy;   // 换下的旧件不要求背包有空位：丢到脚下 | a replaced piece needs no bag space: it is dropped
    uint32_t oid = old ? *(uint32_t *)(uintptr_t)(old + 8) : 0;
    if (!old) { uint8_t p[9] = { 0x1a }; w32(p + 1, J.base); w32(p + 5, (uint32_t)slot); dispatch(G, pl, p, 9); }
    else { uint8_t p[13] = { 0x1d }; w32(p + 1, J.base); w32(p + 5, oid); w32(p + 9, (uint32_t)slot); dispatch(G, pl, p, 13); if (mode_of(old) == 4) drop_cursor(G, pl, old); }   // 换下的旧件丢到脚下（用户的做法：穿上新的、丢掉旧的） | the replaced piece is dropped (the user's approach: wear the new one, drop the old)
    uint64_t d = rd(it + 0x10); int ok = mode_of(it) == 1 && d && *(uint8_t *)(uintptr_t)(d + 0x54) == slot;
    LOG("一键配装：「%s」穿到装备位 %d %s；换下 id %u → %s", pc->what, slot, ok ? "成" : "【红】败", oid, !old ? "（原来空着）" : mode_of(old) == 3 ? "丢在脚下" : mode_of(old) == 0 ? "进背包" : "【红】没放好");
    if (!ok && mode_of(it) == 4) pk_grid(G, pl, 0x18, J.base, nx, ny, 0);
    if (old && mode_of(old) == 4) { lo_stop("dmLoNoRoom", pc->what); return 0; }   // 旧件还在光标上：由 lo_tick 等有空位再放 | the old piece is still on the cursor: lo_tick puts it away once there is room
    if (!ok) { lo_stop("dmLoFail", pc->what); return 0; }
    return 1;
}
// 佣兵：依据 C:\Users\miiro\agy_tasks\20261025_d2r_merc_re\report.md——0x6c3130(G, 玩家, 7, 0) 取活着的佣兵（再核单位类型 1、+0x126 bit0、模式不为 0 / 12），
// 0x51 {op, 光标物品, 佣兵 id, 佣兵该位旧件或 0xffffffff, 装备位}（17 字节）一包互换，旧件回玩家光标；佣兵装备位 头 1、身体 3、武器 4、盾 5，按佣兵自己的属性与等级查需求。
// Mercenary: per C:\Users\miiro\agy_tasks\20261025_d2r_merc_re\report.md - 0x6c3130(G, player, 7, 0) gets the living mercenary (then check unit type 1, +0x126 bit0, mode not 0 / 12),
// 0x51 {op, cursor item, mercenary id, its old item in that slot or 0xffffffff, slot} (17 bytes) swaps in one packet, the old item returns to the player's cursor; mercenary slots head 1, body 3, weapon 4, shield 5, requirements checked against the mercenary's own stats and level.
static uint64_t alive_merc(uint64_t G, uint64_t pl) {
    uint64_t m = ((uint64_t (*)(uint64_t, uint64_t, int, int))(B + 0x6c3130))(G, pl, 7, 0); if (!m || *(uint32_t *)(uintptr_t)m != 1) return 0;
    uint32_t md = *(uint32_t *)(uintptr_t)(m + 0xc); return (*(uint8_t *)(uintptr_t)(m + 0x126) & 1) || md == 0 || md == 12 || !rd(m + 0x90) ? 0 : m;
}
static int lo_equip_merc(uint64_t G, uint64_t pl, const lo_piece_t *pc) {
    uint64_t m = alive_merc(G, pl); if (!m) { if (!J.nomerc++) lo_notice("dmLoNoMerc", NULL); LOG("一键配装：没有活着的佣兵，「%s」留在背包", pc->what); return 1; }
    uint64_t it = own_item(pl, J.base); int nx, ny; if (!it || !cell_of(it, &nx, &ny)) { lo_stop("dmLoFail", pc->what); return 0; }
    uint64_t old = equipped(m, pc->slot); uint32_t ox = 0, oy = 0, oid = old ? *(uint32_t *)(uintptr_t)(old + 8) : 0xffffffffu;
    pk_grid(G, pl, 0x19, J.base, nx, ny, 0);
    if (mode_of(it) != 4) { lo_stop("dmLoFail", pc->what); return 0; }
    (void)ox; (void)oy;   // 换下的旧件不要求背包有空位：丢到脚下 | a replaced piece needs no bag space: it is dropped
    uint8_t p[17] = { 0x51 }; w32(p + 1, J.base); w32(p + 5, *(uint32_t *)(uintptr_t)(m + 8)); w32(p + 9, oid); w32(p + 13, pc->slot); int rc = dispatch(G, pl, p, 17);
    if (old && mode_of(old) == 4) drop_cursor(G, pl, old);
    int ok = equipped(m, pc->slot) == it;
    LOG("一键配装：「%s」给佣兵穿到装备位 %d %s（分发 %d）；换下 id %u → %s", pc->what, pc->slot, ok ? "成" : "【红】败", rc, old ? oid : 0, !old ? "（原来空着）" : mode_of(old) == 3 ? "丢在脚下" : mode_of(old) == 0 ? "进背包" : "【红】没放好");
    if (!ok && mode_of(it) == 4) pk_grid(G, pl, 0x18, J.base, nx, ny, 0);
    if (old && mode_of(old) == 4) { lo_stop("dmLoNoRoom", pc->what); return 0; }
    if (!ok) { lo_stop("dmLoFail", pc->what); return 0; }
    return 1;
}
// 符文之语改走 dm_runeword.c 的一键合成（客户端 0x19 / 0x28，真机验证过）：build 14.1 / 15.1 真机上，配装自己在服务端发 0x28，
// 正义之手前 3 颗进孔、会成词的第 4 颗被退回光标；同一件从「装备」页生成、走一键合成能成词（用户实测）。等底材出现成词标志 0x04000000，
// 再写底材自带词缀、去力量 / 敏捷需求、符文之语满值，然后去穿。约 30 秒没成词就停
// Runewords now go through dm_runeword.c's one-tap crafting (client 0x19 / 0x28, verified on device): on device in builds 14.1 / 15.1, with the loadout sending 0x28 itself on the server,
// Hand of Justice took 3 runes and the 4th, which forms the word, was returned to the cursor; the same piece created from the "Items" page and crafted by one-tap crafting formed (user test). Wait for the runeword flag 0x04000000 on the base,
// then write the base extras, remove strength / dexterity requirements and the runeword max rolls, then equip. Stop after about 30 s without a word
static int rw_row_key(uint64_t G, const char *key) {
    uint64_t T = tables(G), rr = T ? rd(T + 0x1880) : 0; uint32_t rn = T ? *(uint32_t *)(uintptr_t)(T + 0x1888) : 0;
    for (uint32_t k = 0; rr && k < rn && k < 4096; k++) if (!strncmp((const char *)(uintptr_t)(rr + (uint64_t)k * 0xe8), key, 64)) return (int)k;
    return -1;
}
static int lo_rwjob(uint64_t G, uint64_t pl, const lo_piece_t *pc) {
    if (!J.k) { int rec = rw_row_key(G, pc->rwkey); if (rec < 0) { LOG("【红】一键配装：运行时符文之语表里没有 %s", pc->rwkey); lo_stop("dmLoFail", pc->what); return 0; }
        dm_runeword_auto(J.base, rec); atomic_store(&g_lo_busy, 2); J.k = 1; J.backtry = 0; LOG("一键配装：「%s」交给一键合成（底材 id %u、表行 %d）", pc->what, J.base, rec); return 0; }
    uint64_t b = own_item(pl, J.base), d = b ? rd(b + 0x10) : 0;
    if (!d || !(*(uint32_t *)(uintptr_t)(d + 0x18) & 0x04000000)) {
        if (++J.backtry > 750 && !dm_runeword_busy()) { LOG("一键配装：「%s」30 秒没成词", pc->what); lo_stop("dmLoFail", pc->what); } return 0; }
    atomic_store(&g_lo_busy, 1); LOG("一键配装：「%s」成词", pc->what);
    struct lo_aff ab = { pc->what, pc->bst, pc->nbst, 0 }, a = { pc->what, pc->st, pc->nst, 0 };
    // 普通品质底材成词后没有物品词缀表（19.1 真机：找不到词缀表），底材附加也写进成词那张表（0xab，同样存档）；写不上只记红字，照样穿上做下一件
    // A normal base has no item affix list after the word forms (19.1 on device: no affix list), so base extras also go into the runeword list (0xab, also saved); a failed write is only logged in red, the piece is still equipped and the loadout continues
    if (pc->nbst && !apply_list(b, 1, &ab)) LOG("【红】一键配装：「%s」底材附加没写全，照样穿上", pc->what);
    no_req(b);
    if (pc->nst && !apply_list(b, 1, &a)) LOG("【红】一键配装：「%s」符文之语满值没写全，照样穿上", pc->what);
    J.backtry = 0; return 1;
}
static void lo_tick(uint64_t G, uint64_t pl) {
    uint32_t pid = *(uint32_t *)(uintptr_t)(pl + 8);
    if (J.G && (J.G != G || J.pid != pid)) { LOG("一键配装：换局或换角色，旧任务作废（停在第 %d 件）", J.i); memset(&J, 0, sizeof J); }
    if (J.st == LO_IDLE) {
        if (!atomic_exchange(&g_lo_req, 0)) { atomic_store(&g_lo_busy, 0); return; }
        if (!dm_runeword_inv_open()) { lo_notice("dmLoOpenInv", NULL); return; }
        if (dm_runeword_busy()) { lo_notice("dmLoBusy", NULL); return; }
        if (J.resume) { J.st = J.resume; J.resume = 0; J.backtry = 0; LOG("一键配装：从第 %d 件接着做", J.i); }
        else { memset(&J, 0, sizeof J); J.G = G; J.pid = pid; J.st = LO_MAKE; LOG("一键配装：开始，共 %d 件，当前武器组 %d", NLO, cur_set(pl)); }
        atomic_store(&g_lo_busy, 1); lo_notice("dmLoStart", NULL);
    }
    if (*(uint32_t *)(uintptr_t)(G + 0x19050)) return;   // 原版正在分发：下一拍再说 | the original is dispatching: next tick
    if (!dm_runeword_inv_open()) { if (!J.closed++) lo_notice("dmLoClosed", NULL); return; }   // 物品栏关了就停着等，开了接着做 | inventory closed: wait, continue once it is open
    J.closed = 0;
    if (J.st != LO_RWJOB && !clear_cursor(G, pl)) { if (!J.cursor_warned++) lo_notice("dmLoCursor", NULL); return; }   // 光标没空之前什么都不做（也不切武器组、不结束） | nothing else until the cursor is empty (no weapon switch, no finishing)
    J.cursor_warned = 0;
    if (J.st != LO_BACK && J.i >= NLO) J.st = LO_BACK;
    const lo_piece_t *pc = J.st == LO_BACK ? NULL : &k_lo[J.i];
    switch (J.st) {
    case LO_MAKE: if (lo_make(G, pl, pc)) { J.k = 0; J.st = !J.nr ? LO_EQUIP : pc->kind == LK_RUNEWORD ? LO_RWJOB : LO_SOCK; } break;
    case LO_RWJOB: if (lo_rwjob(G, pl, pc)) J.st = LO_EQUIP; break;
    case LO_SOCK: if (lo_sock(G, pl, pc) && J.k == J.nr) J.st = LO_EQUIP; break;
    case LO_SWITCH: if (cur_set(pl) == 1 || switch_set(G, pl)) J.st = LO_EQUIP; else lo_stop("dmLoFail", pc->what); break;
    case LO_EQUIP:   // 护身符（装备位 0）留在背包；佣兵件给佣兵；副手组的件先切过去 | charms (slot 0) stay in the inventory; mercenary pieces go to the mercenary; secondary-set pieces switch over first
        if (!pc->slot) { J.i++; J.st = LO_MAKE; break; }
        if (pc->merc) { if (lo_equip_merc(G, pl, pc)) { J.i++; J.st = LO_MAKE; } break; }
        if (pc->set2 && cur_set(pl) != 1) { J.st = LO_SWITCH; break; }
        if (!pc->set2 && cur_set(pl) == 1 && !switch_set(G, pl)) { lo_stop("dmLoFail", pc->what); break; }
        if (lo_equip(G, pl, pc)) { J.i++; J.st = LO_MAKE; } break;
    case LO_BACK:   // 切回主手组：角色正在出招时会被拒，最多等约 3 秒 | switch back to the primary set: refused while the character is mid-skill, wait up to about 3 s
        if (cur_set(pl) == 1 && !switch_set(G, pl) && ++J.backtry < 75) break;
        if (cur_set(pl) == 1) lo_notice("dmLoBackFail", NULL);
        else if (J.i >= NLO) lo_notice("dmLoDone", NULL);
        LOG("一键配装：结束于第 %d / %d 件，武器组 %d%s", J.i, NLO, cur_set(pl), J.resume ? "（可接着做）" : "");
        J.st = LO_IDLE; J.backtry = 0; atomic_store(&g_lo_busy, 0); break;
    }
}

// 刷装备页的符文之语：一键合成在客户端跑，服务端不知道何时成词。这里盯着那件底材，见到成词标志 0x04000000 就去力量 / 敏捷需求并重发；2 分钟没成词就不等了
// Runewords from the item page: one-tap crafting runs on the client, so the server does not know when the word forms. Watch that base here; once the word flag 0x04000000 shows, remove strength / dexterity requirements and resend; give up after 2 minutes without a word
static void rw_post(uint64_t G, uint64_t player) {
    if (!g_rwpend) return; uint32_t t = *(uint32_t *)(uintptr_t)(G + 0x170);
    uint64_t it = own_item(player, g_rwpend), d = it ? rd(it + 0x10) : 0;
    if (d && (*(uint32_t *)(uintptr_t)(d + 0x18) & 0x04000000)) { no_req(it); resend(G, player, it); LOG("刷装备：符文之语 id %u 成词后去需求", g_rwpend); g_rwpend = 0; return; }
    if (t - g_rwpend_at > 25 * 120) { LOG("刷装备：符文之语 id %u 2 分钟没成词，不再等", g_rwpend); g_rwpend = 0; }
}
void dm_itemgen_tick(uint64_t G, uint64_t player, uint8_t *main_base) {
    B = main_base; if (!G || !player) return;
    lv_apply();
    rw_post(G, player);
    lo_tick(G, player);
    if (J.st != LO_IDLE) return;   // 配装进行中不接单件请求 | no single-item requests while a loadout runs
    int r = atomic_exchange(&g_req, -1); if (r < 0) return;
    int kind = r >> 16, i = r & 0xffff; LOG("刷装备：请求 种类 %d 第 %d 条「%s」", kind, i, dm_itemgen_name(kind, i));
    spawn(G, player, kind, i);
}
