#ifndef __KEYMAP_H__
#define __KEYMAP_H__

#include <inttypes.h>

uint32_t keySymToMask(uint32_t keySym);
uint16_t keySymToScanCode(uint32_t sym);

#endif
