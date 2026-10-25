#pragma once
// 召唤控制台：在玩家身边刷友方首领当随从，外加通用修改（升级、金币、属性点、无敌…）。实现与依据见 dm_summon.c
// Summon console: spawn friendly bosses next to the player as minions, plus general cheats (level up, gold, stat points, god mode...). For implementation and rationale see dm_summon.c
#include <stdint.h>

enum { DM_SUM_ANDARIEL, DM_SUM_DURIEL, DM_SUM_MEPHISTO, DM_SUM_DIABLO, DM_SUM_BAAL, DM_SUM_NBOSS };
enum { DM_CHEAT_LEVEL_UP = 1, DM_CHEAT_LEVEL_99, DM_CHEAT_GOLD_MAX, DM_CHEAT_STATPTS, DM_CHEAT_SKILLPTS, DM_CHEAT_REFILL };

// 界面线程调：只投请求，服务端下一拍执行 | called from the UI thread: only posts a request, executed on the next server tick
void dm_summon_request(int boss, int count);   // boss = DM_SUM_*，count 1–10
void dm_summon_clear(void);                    // 一键清除本局召唤出来的全部首领 | clear every boss summoned in this run
void dm_cheat_request(int op, int arg);        // op = DM_CHEAT_*；arg：属性点 / 技能点的点数 | arg: number of stat / skill points
void dm_cheat_set_god(int on);                 // 无敌（生命每拍回满） | god mode (life refilled every tick)
void dm_cheat_set_inf_mana(int on);            // 无限法力和耐力 | infinite mana and stamina
int dm_cheat_god(void);
int dm_cheat_inf_mana(void);
int dm_summon_alive(void);                     // 界面显示：现存的召唤首领数（上一拍数的） | for the UI: number of summoned bosses alive (counted on the previous tick)
const char *dm_summon_key(int boss);           // 首领名字的串表键（界面用 dm_ui 取） | string table key of the boss name (the UI looks it up with dm_ui)

void dm_summon_tick(uint64_t game, uint64_t player, uint8_t *main_base);   // 服务端每拍调（diag.c hud_tick） | called every server tick (diag.c hud_tick)
