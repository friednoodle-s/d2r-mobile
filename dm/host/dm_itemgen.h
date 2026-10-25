#pragma once
// 刷装备：生成指定暗金 / 套装物品 / 成品符文之语，去掉力量与敏捷需求，捡进背包。实现与依据见 dm_itemgen.c
// Item spawner: create a chosen unique / set item / finished runeword, remove strength and dexterity requirements, pick it into the inventory. For implementation and rationale see dm_itemgen.c
#include <stdint.h>

enum { DM_ITEM_UNIQUE, DM_ITEM_SET, DM_ITEM_RUNEWORD, DM_ITEM_NKIND };
enum { DM_RW_PICKED, DM_RW_SHIELD, DM_RW_ONEHAND };   // 符文之语行的底材：清单挑的 / 君主盾 / 幻化之刃 | runeword row base: the list's pick / Monarch / Phase Blade

int dm_itemgen_count(int kind);                 // 清单条数 | number of list entries
const char *dm_itemgen_name(int kind, int i);   // 显示名（按当前语言中文 / 英文） | display name (Chinese / English by the current language)
const char *dm_itemgen_sub(int kind, int i);    // 副标题：套装名 / 底材码 / 需求等级（可空串） | subtitle: set name / base code / required level (may be empty)
int dm_itemgen_variant(int kind, int i);        // 符文之语行的底材变体 DM_RW_*（其他种类为 0） | base variant DM_RW_* of a runeword row (0 for other kinds)
int dm_itemgen_match(int kind, int i, const char *q);   // 搜索：中英文名（套装另比套装名）含 q；q 空串全中 | search: the Chinese or English name (sets also the set name) contains q; an empty q matches all
void dm_itemgen_request(int kind, int i);       // 界面线程调：投请求，服务端下一拍生成 | called from the UI thread: posts a request, created on the next server tick
void dm_itemgen_set_nolevel(int on);            // 装备免等级需求（运行时表清零，全部物品生效，不进存档） | no level requirement (runtime tables zeroed, applies to every item, never saved)
int dm_itemgen_nolevel(void);

void dm_itemgen_tick(uint64_t game, uint64_t player, uint8_t *main_base);   // 服务端每拍调（dm_summon_tick 里） | called every server tick (from dm_summon_tick)
