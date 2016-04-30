/*
*   firm.c
*       by Reisyukaku
*   Copyright (c) 2015 All Rights Reserved
*/

#include <string.h>
#include "firm.h"
#include "patches.h"
#include "memory.h"
#include "fs.h"
#include "emunand.h"
#include "draw.h"

//Firm vars
const void *firmLocation = (void*)0x24000000;
firmHeader *firm = NULL;
firmSectionHeader *section = NULL;
Size firmSize = 0;

//Emu vars
u32 emuOffset = 0,
    emuHeader = 0,
    emuRead = 0,
    emuWrite = 0,
    sdmmcOffset = 0,
    mpuOffset = 0,
    emuCodeOffset = 0;
    
//Patch vars
u32 sigPatchOffset1 = 0,
    sigPatchOffset2 = 0,
    threadOffset1 = 0,
    threadOffset2 = 0,
    threadCodeOffset = 0,
    exeOffset = 0;

//Load firm into FCRAM
void loadFirm(void){
    //Read FIRM from SD card and write to FCRAM
    fopen("/rei/firmware.bin", "rb");
    firmSize = fsize();
    fread(firmLocation, 1, firmSize);
    fclose();
    decryptFirm(firmLocation, firmSize);
    
    //Initial setup
    firm = firmLocation;
    section = firm->section;
    k9loader(firmLocation + section[2].offset);
    
    //Set MPU for emu/thread code region
    getMPU(firmLocation, firmSize, &mpuOffset);
    memcpy((u8*)mpuOffset, mpu, sizeof(mpu));
    
    //Check for Emunand
    getEmunandSect(&emuOffset, &emuHeader);
    if(emuOffset || emuHeader) loadEmu();
    else loadSys();
}

//Setup for Sysnand
void loadSys(void){
    //Disable firm partition update if a9lh is installed
    if(!PDN_SPI_CNT){
        getExe(firmLocation, firmSize, &exeOffset);
        memcpy((u8*)exeOffset, "kek", 3);
    }
}

//Nand redirection
void loadEmu(void){ 
    //Dont boot emu if AGB game was just played, or if START was held.
    if((HID & 0xFFF) == (1 << 3) || CFG_BOOTENV == 0x7) return;
    
    //Read emunand code from SD
    fopen("/rei/emunand/emunand.bin", "rb");
    Size emuSize = fsize();
    getEmuCode(firmLocation, firmSize, &emuCodeOffset);
    fread(emuCodeOffset, 1, emuSize);
    fclose();
    
    //Setup Emunand code
    uPtr *pos_sdmmc = memsearch(emuCodeOffset, "SDMC", emuSize, 4);
    uPtr *pos_offset = memsearch(emuCodeOffset, "NAND", emuSize, 4);
    uPtr *pos_header = memsearch(emuCodeOffset, "NCSD", emuSize, 4);
	getSDMMC(firmLocation, firmSize, &sdmmcOffset);
    getEmuRW(firmLocation, firmSize, &emuRead, &emuWrite);
    *pos_sdmmc = sdmmcOffset;
    *pos_offset = emuOffset;
    *pos_header = emuHeader;
    
    //Add Emunand hooks
    memcpy((u8*)emuRead, nandRedir, sizeof(nandRedir));
    memcpy((u8*)emuWrite, nandRedir, sizeof(nandRedir));
}

//Patches arm9 things on Sys/Emu
void patchFirm(){ 
    //Disable signature checks
    getSigChecks(firmLocation, firmSize, &sigPatchOffset1, &sigPatchOffset2);
    memcpy((u8*)sigPatchOffset1, sigPatch1, sizeof(sigPatch1));
    memcpy((u8*)sigPatchOffset2, sigPatch2, sizeof(sigPatch2));
    
    //Inject custom loader
    fopen("/rei/loader.cxi", "rb");
    fread(firmLocation + 0x26600, 1, fsize());
    fclose();
}

void patchTwlAgbFirm(){
    u32 firmType,
        console;

    //Detect the console being used
    console = (PDN_MPCORE_CFG == 1) ? 0 : 1;

    //Determine if this is a firmlaunch boot
    if(*(vu8 *)0x23F00005){
        //'0' = NATIVE_FIRM, '1' = TWL_FIRM, '2' = AGB_FIRM
        firmType = *(vu8 *)0x23F00005 - '0';
    } else {
        firmType = 0;
    }
    //On N3DS, decrypt ARM9Bin and patch ARM9 entrypoint to skip arm9loader
    if(console)
    {
        arm9Loader((u8 *)firm + section[3].offset, 0);
        firm->arm9Entry = (u8 *)0x801301C;
    }

    const patchData twlPatches[] = {
        {{0x1650C0, 0x165D64}, {{ 6, 0x00, 0x20, 0x4E, 0xB0, 0x70, 0xBD }}, 0},
        {{0x173A0E, 0x17474A}, { .type1 = 0x2001 }, 1},
        {{0x174802, 0x17553E}, { .type1 = 0x2000 }, 2},
        {{0x174964, 0x1756A0}, { .type1 = 0x2000 }, 2},
        {{0x174D52, 0x175A8E}, { .type1 = 0x2001 }, 2},
        {{0x174D5E, 0x175A9A}, { .type1 = 0x2001 }, 2},
        {{0x174D6A, 0x175AA6}, { .type1 = 0x2001 }, 2},
        {{0x174E56, 0x175B92}, { .type1 = 0x2001 }, 1},
        {{0x174E58, 0x175B94}, { .type1 = 0x4770 }, 1}
    },
    agbPatches[] = {
        {{0x9D2A8, 0x9DF64}, {{ 6, 0x00, 0x20, 0x4E, 0xB0, 0x70, 0xBD }}, 0},
        {{0xD7A12, 0xD8B8A}, { .type1 = 0xEF26 }, 1}
    };

    /* Calculate the amount of patches to apply. Only count the boot screen patch for AGB_FIRM
       if the matching option was enabled (keep it as last) */
    u32 numPatches = firmType == 1 ? (sizeof(twlPatches) / sizeof(patchData)) : (sizeof(agbPatches) / sizeof(patchData));
    const patchData *patches = firmType == 1 ? twlPatches : agbPatches;

    //Patch
    for(u32 i = 0; i < numPatches; i++)
    {
        switch(patches[i].type)
        {
            case 0:
                memcpy((u8 *)firm + patches[i].offset[console], patches[i].patch.type0 + 1, patches[i].patch.type0[0]);
                break;
            case 2:
                *(u16 *)((u8 *)firm + patches[i].offset[console] + 2) = 0;
            case 1:
                *(u16 *)((u8 *)firm + patches[i].offset[console]) = patches[i].patch.type1;
                break;
        }
    }
}

void launchFirm(void){
    //Copy firm partitions to respective memory locations
    memcpy(section[0].address, firmLocation + section[0].offset, section[0].size);
    memcpy(section[1].address, firmLocation + section[1].offset, section[1].size);
    memcpy(section[2].address, firmLocation + section[2].offset, section[2].size);
    
    //Run ARM11 screen stuff
    vu32 *arm11 = (vu32*)0x1FFFFFF8;
    *arm11 = (u32)shutdownLCD;
    while (*arm11);
    
    //Set ARM11 kernel
    *arm11 = (u32)firm->arm11Entry;
    
    //Final jump to arm9 binary
    u32 entry = PDN_MPCORE_CFG != 1 ? 0x801B01C : firm->arm9Entry;
    ((void (*)())entry)();
}