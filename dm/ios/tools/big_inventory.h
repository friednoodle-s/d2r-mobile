/* Shared build switch: regenerate overlays AND rebuild the host after changing it. */
#pragma once
#define BIG_INV 0

#if BIG_INV != 0 && BIG_INV != 1
#error BIG_INV must be 0 or 1
#endif
