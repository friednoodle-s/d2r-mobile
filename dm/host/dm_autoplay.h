#pragma once
#include <stdint.h>

// 自动清图（dm_autoplay.c）：面板「自动」页调前几个（任意线程），服务端每拍调 dm_autoplay_tick
// Auto map clear (dm_autoplay.c): the panel "Auto" page calls the first ones (any thread); the server calls dm_autoplay_tick every tick
void dm_auto_set(int on);
int dm_auto_on(void);
int dm_auto_skill_count(void);                 // 可选技能条数（0 = 当前右键技能） | number of selectable skills (0 = current right-hand skill)
const char *dm_auto_skill_key(int i);          // 技能名的串表键 | string table key of the skill name
void dm_auto_set_skill(int aoe, int i);        // aoe = 0 主攻（0..）/ 1 范围（-1 = 不用） | aoe = 0 main (0..) / 1 area (-1 = none)
int dm_auto_skill(int aoe);
void dm_auto_set_pick(int mask);               // 1 暗金 / 套装、2 稀有、4 符文 | 1 unique / set, 2 rare, 4 runes
int dm_auto_pick(void);
void dm_auto_status(int *rooms, int *total, int *kills, int *state);
void dm_autoplay_tick(uint64_t game, uint64_t player, uint8_t *main_base);
