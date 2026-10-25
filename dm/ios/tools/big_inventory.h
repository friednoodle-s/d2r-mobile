/* Regenerate overlays AND rebuild the host after changing BIG_INV.
 * 0: original 10x4; 1: small 15x6; 2: full-size 13x8. */
#pragma once
#define BIG_INV 2

#if BIG_INV < 0 || BIG_INV > 2
#error BIG_INV must be 0, 1 or 2
#endif

/* Mode 2 custom controls: shared with big_inventory.py and dm_pouch.c. */
#define BIG_INV2_POUCH_X 1100
#define BIG_INV2_POUCH_Y 1340
#define BIG_INV2_STORE_X 84
#define BIG_INV2_STORE_Y 1394
