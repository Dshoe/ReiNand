/*
*   emunand.h
*       by Reisyukaku
*   Copyright (c) 2015 All Rights Reserved
*/

#ifndef EMU_INC
#define EMU_INC

#include "types.h"

#define NCSD_MAGIC (0x4453434E)

void getEmunandSect(u32 *off, u32 *head);
void getSDMMC(void *pos, Size size, u32 *off);
void getEmuRW(void *pos, Size size, u32 *readOff, u32 *writeOff);
void getMPU(void *pos, Size size, u32 *off);
void getEmuCode(void *pos, Size size, u32 *off);

#endif