/* --------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <p-sam@d3vs.net>, <natinusala@gmail.com>, <m4x@m4xw.net>
 * wrote this file. As long as you retain this notice you can do whatever you
 * want with this stuff. If you meet any of us some day, and you think this
 * stuff is worth it, you can buy us a beer in return.  - The sys-clk authors
 * --------------------------------------------------------------------------
 */


#pragma once

#include <stdint.h>
#include "board.h"

#define SYSCLK_FREQ_LIST_MAX 32

typedef struct
{
    uint64_t applicationId;
    SysClkProfile profile;
    uint32_t freqs[SysClkModule_EnumMax];
    uint32_t realFreqs[SysClkModule_EnumMax];
    uint32_t overrideFreqs[SysClkModule_EnumMax];
    uint32_t temps[SysClkThermalSensor_EnumMax];
    int32_t power[SysClkPowerSensor_EnumMax];
    uint32_t partLoad[SysClkPartLoad_EnumMax];
    uint32_t voltages[SysClkVoltage_EnumMax];
    u16 speedos[HorizonOCSpeedo_EnumMax];
    u16 iddq[HorizonOCSpeedo_EnumMax];
    u8 maxDisplayFreq;
    u8 fps;
    u8 dramID;
    bool isDram8GB;
    // u32 marikoGpuVoltArray[SYSCLK_FREQ_LIST_MAX];
    // u32 eristaGpuVoltArray[SYSCLK_FREQ_LIST_MAX];
} SysClkContext;

typedef struct
{
    union {
        uint32_t mhz[+SysClkProfile_EnumMax * +SysClkModule_EnumMax];
        uint32_t mhzMap[+SysClkProfile_EnumMax][+SysClkModule_EnumMax];
    };
} SysClkTitleProfileList;

#define GLOBAL_PROFILE_ID 0xA111111111111111