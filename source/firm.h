/*
*   firm.h
*       by Reisyukaku
*   Copyright (c) 2015 All Rights Reserved
*/
#ifndef FIRM_INC
#define FIRM_INC

#include "types.h"

typedef struct patchData {
    u32 offset[2];
    union {
        u8 type0[8];
        u16 type1;
    } patch;
    u32 type;
} patchData;

void loadSplash(void);
void loadFirm(void);
void loadSys(void);
void loadEmu(void);
void patchFirm(void);
void patchTwlAgbFirm(void);
void launchFirm(void);

#endif