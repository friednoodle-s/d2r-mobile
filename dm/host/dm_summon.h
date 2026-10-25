#pragma once
// 召唤控制台：在玩家身边刷友方首领当随从，外加通用修改（升级、金币、属性点、无敌…）。实现与依据见 dm_summon.c
// Summon console: spawn friendly bosses next to the player as minions, plus general cheats (level up, gold, stat points, god mode...). For implementation and rationale see dm_summon.c
#include <stdint.h>

enum { DM_SUM_ANDARIEL, DM_SUM_DURIEL, DM_SUM_MEPHISTO, DM_SUM_DIABLO, DM_SUM_BAAL, DM_SUM_NBOSS };
enum { DM_CHEAT_LEVEL_UP = 1, DM_CHEAT_LEVEL_99, DM_CHEAT_GOLD_MAX, DM_CHEAT_STATPTS, DM_CHEAT_SKILLPTS, DM_CHEAT_REFILL, DM_CHEAT_WAYPOINTS, DM_CHEAT_DIFFICULTY };

// 界面线程调：只投请求，服务端下一拍执行 | called from the UI thread: only posts a request, executed on the next server tick
void dm_summon_request(int boss, int count);   // boss = DM_SUM_*，count 1–10
void dm_summon_clear(void);                    // 一键清除本局召唤出来的全部首领 | clear every boss summoned in this run
void dm_cheat_request(int op, int arg);        // op = DM_CHEAT_*；arg：属性点 / 技能点的点数 | arg: number of stat / skill points
void dm_cheat_set_god(int on);                 // 无敌（生命每拍回满） | god mode (life refilled every tick)
void dm_cheat_set_inf_mana(int on);            // 无限法力和耐力 | infinite mana and stamina
int dm_cheat_god(void);
int dm_cheat_inf_mana(void);
// 属性开关：开着时每隔一会儿把角色的这些属性写到目标值，关掉写回 0（这些属性 itemstatcost 不存档，读档自然没了）
// Stat toggles: while on, the character's stats below are written to the target values every so often; off writes them back to 0 (itemstatcost does not save these stats, so they are gone after a reload anyway)
enum { DM_BUFF_MF, DM_BUFF_GF, DM_BUFF_EXP, DM_BUFF_SPEED, DM_BUFF_DAMAGE, DM_BUFF_RESIST, DM_BUFF_SKILLS, DM_NBUFF };
void dm_cheat_set_buff(int buff, int on);
int dm_cheat_buff(int buff);
const char *dm_cheat_buff_key(int buff);       // 开关名字的串表键 | string table key of the toggle name
int dm_summon_alive(void);                     // 界面显示：现存的召唤首领数（上一拍数的） | for the UI: number of summoned bosses alive (counted on the previous tick)
void dm_summon_set_bind_mode(int mode);        // 绑定类型：0 骷髅复生、1 束缚恶魔、2 女武神、3 不转化（排查随从猝死用） | bind type: 0 Raise Skeleton, 1 Bind Demon, 2 Valkyrie, 3 do not bind (for chasing sudden minion deaths)
int dm_summon_bind_mode(void);
int dm_summon_owns(uint32_t id);               // 这个怪物单位是我们召唤出来的（刷怪积分牌不计、不提示；服务端线程） | this monster unit was summoned by us (not counted or announced by the kill board; server thread)
const char *dm_summon_key(int boss);           // 首领名字的串表键（界面用 dm_ui 取） | string table key of the boss name (the UI looks it up with dm_ui)

void dm_summon_tick(uint64_t game, uint64_t player, uint8_t *main_base);   // 服务端每拍调（diag.c hud_tick） | called every server tick (diag.c hud_tick)
