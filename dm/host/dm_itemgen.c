// 刷装备（界面在 dm/ios/Sources/dm_summon_panel.m「装备」页）：生成指定暗金 / 套装物品 / 成品符文之语。
// 清单：dm_items_tab.h（原版 uniqueitems / setitems / runes 表与中英文名，生成器见文件头；符文之语另带挑好的底材码与孔数）。
// 依据（静态逆向，见交接文档 handover\reports）：
//   通用生成 0x656780(游戏, 参数块 0xd8, 0)，参数照 dm_rift.c give_item2：+0 玩家、+0x10 游戏、+0x18 物品等级、+0x20 统一物品类号（[数据表 + 0x258] 的 {码, 类号} 索引）、
//   +0x24 模式 3（地上）、+0x28 / +0x2c 落点、+0x30 房间、+0x38 = 1、+0x3a = [G+0x128]、+0x40 品质（7 暗金、5 套装、2 普通）、+0x50 指定暗金 / 套装行号 + 1；
//   暗金另有本局已出位图 G+0x3638（第 row >> 5 个 u32 的 row & 31 位），重复生成前清这一位。生成后核对 物品数据 +0 品质、+0x34 行号。
//   鉴定：0x78acf0(游戏, 玩家, 物品)（同 diag.c 免费鉴定，自己广播更新）。设孔：0x5754a0(物品, 孔数)，属性 194 读回。
//   去力量 / 敏捷需求：物品自己的属性 91 item_req_percent 有效值 −100（存档 8 位、Save Add 100，能存 −100）。
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
//   Strength / dexterity requirements removed: effective value −100 of the item's own stat 91 item_req_percent (saved in 8 bits with Save Add 100, so −100 is storable).
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

// 装备免等级需求（见文件头）：按当前游戏版本那张表，第一次开时存原值再清零，关时写回；表指针变了（重新载表）就当新表 | no level requirement (see the file header)
static struct { uint64_t T; int on; uint32_t nu, ns, ni; int16_t *u, *s; uint8_t *it; } g_lv[4];
static void lv_apply(uint64_t G) {
    uint8_t v = *(uint8_t *)(uintptr_t)(G + 0x106); uint64_t T = tables(G); if (!T) return;
    int on = atomic_load(&g_nolv); __typeof__(&g_lv[0]) L = &g_lv[v];
    if (L->T != T) { free(L->u); free(L->s); free(L->it); memset(L, 0, sizeof *L); L->T = T; }
    if (on == L->on) return;
    uint64_t ur = rd(T + 0x13e8), sr = rd(T + 0x13c8), ir = rd(T + 0x15c0);
    if (!L->u) {   // 存原值 | keep the originals
        L->nu = (uint32_t)rd(T + 0x13f0); L->ns = (uint32_t)rd(T + 0x13d0); L->ni = *(uint32_t *)(uintptr_t)(T + 0x15c8);
        if (!ur || !sr || !ir || L->nu > 4096 || L->ns > 4096 || L->ni > 8192) { LOG("【红】刷装备：免等级 表不对（暗金 %u、套装 %u、物品 %u 行）", L->nu, L->ns, L->ni); L->on = on; return; }
        L->u = malloc(2 * L->nu + 2); L->s = malloc(2 * L->ns + 2); L->it = malloc(L->ni + 1); if (!L->u || !L->s || !L->it) { L->on = on; return; }
        for (uint32_t k = 0; k < L->nu; k++) L->u[k] = *(int16_t *)(uintptr_t)(ur + k * 0x15c + 0x3a);
        for (uint32_t k = 0; k < L->ns; k++) L->s[k] = *(int16_t *)(uintptr_t)(sr + k * 0x1cc + 0x36);
        for (uint32_t k = 0; k < L->ni; k++) L->it[k] = *(uint8_t *)(uintptr_t)(ir + k * 0x1c0 + 0x14f);
    }
    if (!L->u || !L->s || !L->it) { L->on = on; return; }   // 没存到原值就什么都不改 | without the originals nothing is changed
    for (uint32_t k = 0; k < L->nu; k++) *(int16_t *)(uintptr_t)(ur + k * 0x15c + 0x3a) = on ? 0 : L->u[k];
    for (uint32_t k = 0; k < L->ns; k++) *(int16_t *)(uintptr_t)(sr + k * 0x1cc + 0x36) = on ? 0 : L->s[k];
    for (uint32_t k = 0; k < L->ni; k++) *(uint8_t *)(uintptr_t)(ir + k * 0x1c0 + 0x14f) = on ? 0 : L->it[k];
    L->on = on; LOG("刷装备：装备免等级需求 %s（版本 %u：暗金 %u、套装 %u、物品 %u 行）", on ? "开" : "关，原值写回", v, L->nu, L->ns, L->ni);
}

static int gs(uint64_t u, int st) { return ((int (*)(uint64_t, int, int))(B + 0x5b9290))(u, st, 0); }
static int gsb(uint64_t u, int st) { return (int)((uint32_t (*)(uint64_t, int, int))(B + 0x5b9110))(u, st, 0); }
static void ss(uint64_t u, int st, int v) { ((void (*)(uint64_t, int, int, int))(B + 0x5bc1f0))(u, st, v, 0); }

// 在玩家脚下生成一件：返回物品单位（地上，模式 3）或 0 | create one item at the player's feet: returns the item unit (on the ground, mode 3) or 0
static uint64_t make(uint64_t G, uint64_t player, const char *code4, int q, int row, int ilvl) {
    int cls = class_of(G, code4); if (cls < 0) { LOG("刷装备：物品码 %.4s 查不到类号", code4); return 0; }
    int32_t p0[2] = { 0, 0 }, p1[2] = { 0, 0 }; ((void (*)(uint64_t, int32_t *))(B + 0x5c84f0))(player, p0); uint64_t pth = rd(player + 0x38), room0 = pth ? rd(pth + 0x20) : 0;
    uint64_t room = room0 ? ((uint64_t (*)(uint64_t, uint64_t, int32_t *, int32_t *, int))(B + 0x6529e0))(G, room0, p0, p1, 1) : 0; if (!room) { LOG("刷装备：脚下找不到落点"); return 0; }
    uint8_t prm[0xd8]; memset(prm, 0, sizeof prm);
    *(uint64_t *)(prm + 0) = player; *(uint64_t *)(prm + 0x10) = G; *(int32_t *)(prm + 0x18) = ilvl < 99 ? 99 : ilvl; *(int32_t *)(prm + 0x20) = cls; *(int32_t *)(prm + 0x24) = 3;
    *(int32_t *)(prm + 0x28) = p1[0]; *(int32_t *)(prm + 0x2c) = p1[1]; *(uint64_t *)(prm + 0x30) = room; *(uint16_t *)(prm + 0x38) = 1; *(uint16_t *)(prm + 0x3a) = *(uint16_t *)(uintptr_t)(G + 0x128); *(int32_t *)(prm + 0x40) = q;
    if (row >= 0) { int32_t r1 = row + 1; memcpy(prm + 0x50, &r1, 4); }
    *(uint32_t *)(prm + 0xc0) = 2 | 8;   // 生成控制：bit1 不出无形、bit3 不随机打孔（孔数由 make_ready 指定） | creation control: bit1 no ethereal, bit3 no random sockets (make_ready sets the count)
    if (row >= 0 && q == 7) { uint32_t *w = (uint32_t *)(uintptr_t)(G + 0x3638 + 4 * ((uint32_t)row >> 5)); *w &= ~(1u << (row & 31)); }
    uint64_t it = ((uint64_t (*)(uint64_t, void *, int))(B + 0x656780))(G, prm, 0); uint64_t d = it ? rd(it + 0x10) : 0;
    LOG("刷装备：生成 %.4s 品质 %d 行 %d → %s（读回品质 %d、行 %d）", code4, q, row, it ? "成" : "【红】败", d ? *(int32_t *)(uintptr_t)d : -1, d ? *(int32_t *)(uintptr_t)(d + 0x34) : -1);
    return it;
}
// 去力量 / 敏捷需求：属性 91 的有效值调到 −100（基础项按「有效 − 基础」差补，免得和物品自带的减需叠错） | remove strength / dexterity requirements: bring stat 91's effective value to −100
static void no_req(uint64_t it) {
    int base = gsb(it, 91), total = gs(it, 91), want = base + (-100 - total);
    if (want < -100) want = -100;
    if (want > 155) want = 155;
    if (want != base) ss(it, 91, want);
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
static uint64_t make_ready(uint64_t G, uint64_t player, const char *code4, int q, int row, int ilvl, int sockets, int *in_bag) {
    uint64_t it = make(G, player, code4, q, row, ilvl); if (!it) return 0;
    if (sockets > 0) { ((void (*)(uint64_t, int))(B + 0x5754a0))(it, sockets); LOG("刷装备：%.4s 设 %d 孔 → 读回 %d", code4, sockets, gs(it, 194)); }
    ((int (*)(uint64_t, uint64_t, uint64_t))(B + 0x78acf0))(G, player, it);
    if (q != 2 || sockets > 0) no_req(it);   // 符文本身没有力量 / 敏捷需求，不写 | runes have no strength / dexterity requirement, left alone
    *in_bag = pick_up(G, player, it);
    return it;
}

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
    snprintf(msg, sizeof msg, dm_str(B, "dmItemRwOpen"), nm); dm_game_notice(B, msg);
}

void dm_itemgen_tick(uint64_t G, uint64_t player, uint8_t *main_base) {
    B = main_base; if (!G || !player) return;
    lv_apply(G);
    int r = atomic_exchange(&g_req, -1); if (r < 0) return;
    int kind = r >> 16, i = r & 0xffff; LOG("刷装备：请求 种类 %d 第 %d 条「%s」", kind, i, dm_itemgen_name(kind, i));
    spawn(G, player, kind, i);
}
