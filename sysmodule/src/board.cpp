/*
 * Copyright (c) Souldbminer, Lightos_ and Horizon OC Contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 */
 
/* --------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <p-sam@d3vs.net>, <natinusala@gmail.com>, <m4x@m4xw.net>
 * wrote this file. As long as you retain this notice you can do whatever you
 * want with this stuff. If you meet any of us some day, and you think this
 * stuff is worth it, you can buy us a beer in return.  - The sys-clk authors
 * --------------------------------------------------------------------------
 */

// Note: Hoag crashes on display refresh rate init while in sleep mode

#include <nxExt.h>
#include "board.h"
#include "errors.h"
#include "rgltr.h"
#include "file_utils.h"
#include <algorithm> // for std::clamp
#include <math.h>
#include <numeric>
#include <battery.h>
#include <pwm.h>
#include <display_refresh_rate.h>
#include <stdio.h>
#include <cstring>
#include <registers.h>
#include <notification.h>

#define MAX(A, B)   std::max(A, B)
#define MIN(A, B)   std::min(A, B)
#define CEIL(A)     std::ceil(A)
#define FLOOR(A)    std::floor(A)
#define ROUND(A)    std::lround(A)


#define FUSE_CPU_SPEEDO_0_CALIB 0x114
//#define FUSE_CPU_SPEEDO_1_CALIB 0x12C
#define FUSE_CPU_SPEEDO_2_CALIB 0x130

#define FUSE_SOC_SPEEDO_0_CALIB 0x134
//#define FUSE_SOC_SPEEDO_1_CALIB 0x138
//#define FUSE_SOC_SPEEDO_2_CALIB 0x13C

#define FUSE_CPU_IDDQ_CALIB 0x118
#define FUSE_SOC_IDDQ_CALIB 0x140
#define FUSE_GPU_IDDQ_CALIB 0x228

#define HOSSVC_HAS_CLKRST (hosversionAtLeast(8,0,0))
#define HOSSVC_HAS_TC (hosversionAtLeast(5,0,0))
#define NVGPU_GPU_IOCTL_PMU_GET_GPU_LOAD 0x80044715

#define systemtickfrequency 19200000
#define systemtickfrequencyF 19200000.0f
#define CPU_TICK_WAIT (1'000'000'000 / 60)
Result nvCheck = 1;

Thread gpuLThread;
Thread cpuCore0Thread;
Thread cpuCore1Thread;
Thread cpuCore2Thread;
Thread cpuCore3Thread;
Thread miscThread;
double temp = 0;

PwmChannelSession g_ICon;
Result pwmCheck = 1;
Result pwmDutyCycleCheck = 1;
double Rotation_Duty = 0;
u8 fanLevel;

uint32_t GPU_Load_u = 0, fd = 0;
BatteryChargeInfo info;

static SysClkSocType g_socType = SysClkSocType_Erista;
static HorizonOCConsoleType g_consoleType = HorizonOCConsoleType_Iowa;

std::atomic<uint64_t> idletick0{systemtickfrequency};
std::atomic<uint64_t> idletick1{systemtickfrequency};
std::atomic<uint64_t> idletick2{systemtickfrequency};
std::atomic<uint64_t> idletick3{systemtickfrequency};
u32 cpu0, cpu1, cpu2, cpu3, cpuAvg;
u16 cpuSpeedo0, cpuSpeedo2, socSpeedo0; // CPU, GPU, SOC
u16 cpuIDDQ, gpuIDDQ, socIDDQ;
u8 g_dramID = 0;

const char* Board::GetModuleName(SysClkModule module, bool pretty)
{
    ASSERT_ENUM_VALID(SysClkModule, module);
    return sysclkFormatModule(module, pretty);
}

const char* Board::GetProfileName(SysClkProfile profile, bool pretty)
{
    ASSERT_ENUM_VALID(SysClkProfile, profile);
    return sysclkFormatProfile(profile, pretty);
}

const char* Board::GetThermalSensorName(SysClkThermalSensor sensor, bool pretty)
{
    ASSERT_ENUM_VALID(SysClkThermalSensor, sensor);
    return sysclkFormatThermalSensor(sensor, pretty);
}

const char* Board::GetPowerSensorName(SysClkPowerSensor sensor, bool pretty)
{
    ASSERT_ENUM_VALID(SysClkPowerSensor, sensor);
    return sysclkFormatPowerSensor(sensor, pretty);
}

PcvModule Board::GetPcvModule(SysClkModule sysclkModule)
{
    switch(sysclkModule)
    {
        case SysClkModule_CPU:
            return PcvModule_CpuBus;
        case SysClkModule_GPU:
            return PcvModule_GPU;
        case SysClkModule_MEM:
            return PcvModule_EMC;
        default:
            ASSERT_ENUM_VALID(SysClkModule, sysclkModule);
    }

    return (PcvModule)0;
}

PcvModuleId Board::GetPcvModuleId(SysClkModule sysclkModule)
{
    PcvModuleId pcvModuleId;
    Result rc = pcvGetModuleId(&pcvModuleId, GetPcvModule(sysclkModule));
    ASSERT_RESULT_OK(rc, "pcvGetModuleId");

    return pcvModuleId;
}

void CheckCore(void* idletick_ptr) {
    std::atomic<uint64_t>* idletick = (std::atomic<uint64_t>*)idletick_ptr;
    while (true) {
        uint64_t idletick_a;
        uint64_t idletick_b;
        svcGetInfo(&idletick_b, InfoType_IdleTickCount, INVALID_HANDLE, -1);
        svcSleepThread(CPU_TICK_WAIT);
        svcGetInfo(&idletick_a, InfoType_IdleTickCount, INVALID_HANDLE, -1);
        idletick->store(idletick_a - idletick_b, std::memory_order_release);
    }
}

void gpuLoadThread(void*) {
    #define gpu_samples_average 8
    uint32_t gpu_load_array[gpu_samples_average] = {0};
    size_t i = 0;
    if (R_SUCCEEDED(nvCheck)) do {
        u32 temp;
        if (R_SUCCEEDED(nvIoctl(fd, NVGPU_GPU_IOCTL_PMU_GET_GPU_LOAD, &temp))) {
            gpu_load_array[i++ % gpu_samples_average] = temp;
            GPU_Load_u = std::accumulate(&gpu_load_array[0], &gpu_load_array[gpu_samples_average], 0) / gpu_samples_average;
        }
        svcSleepThread(16'666'000); // wait a bit (this is the perfect amount of time to keep the reading accurate)
    } while(true);
}

void miscThreadFunc(void*) {
    for(;;) {
        if (R_SUCCEEDED(pwmCheck)) {
            if (R_SUCCEEDED(pwmChannelSessionGetDutyCycle(&g_ICon, &temp))) {
                temp *= 10;
                temp = trunc(temp);
                temp /= 10;
                Rotation_Duty = 100.0 - temp;
            }
        }
        fanLevel = (u8)Rotation_Duty;
        svcSleepThread(300'000'000);
    }
}


void Board::Initialize()
{
    Result rc = 0;
    if(HOSSVC_HAS_CLKRST)
    {
        rc = clkrstInitialize();
        ASSERT_RESULT_OK(rc, "clkrstInitialize");
    }
    else
    {
        rc = pcvInitialize();
        ASSERT_RESULT_OK(rc, "pcvInitialize");
    }

    rc = apmExtInitialize();
    ASSERT_RESULT_OK(rc, "apmExtInitialize");

    rc = psmInitialize();
    ASSERT_RESULT_OK(rc, "psmInitialize");

    if(HOSSVC_HAS_TC)
    {
        rc = tcInitialize();
        ASSERT_RESULT_OK(rc, "tcInitialize");
    }

    rc = max17050Initialize();
    ASSERT_RESULT_OK(rc, "max17050Initialize");

    rc = tmp451Initialize();
    ASSERT_RESULT_OK(rc, "tmp451Initialize");

    if (R_SUCCEEDED(nvInitialize())) nvCheck = nvOpen(&fd, "/dev/nvhost-ctrl-gpu");

    rc = rgltrInitialize();
    ASSERT_RESULT_OK(rc, "rgltrInitialize");

    // if (R_SUCCEEDED(fanInitialize())) {
    //     if (hosversionAtLeast(7,0,0)) fanCheck = fanOpenController(&fanController, 0x3D000001);
    //     else fanCheck = fanOpenController(&fanController, 1);
    // }

    rc = pmdmntInitialize();
    ASSERT_RESULT_OK(rc, "pmdmntInitialize");

    threadCreate(&gpuLThread, gpuLoadThread, NULL, NULL, 0x1000, 0x3F, -2);
	threadStart(&gpuLThread);

    threadCreate(&cpuCore0Thread, CheckCore, &idletick0, NULL, 0x500, 0x10, 0);
    threadCreate(&cpuCore1Thread, CheckCore, &idletick1, NULL, 0x500, 0x10, 1);
    threadCreate(&cpuCore2Thread, CheckCore, &idletick2, NULL, 0x500, 0x10, 2);
    threadCreate(&cpuCore3Thread, CheckCore, &idletick3, NULL, 0x500, 0x10, 3);
    threadCreate(&miscThread, miscThreadFunc, NULL, NULL, 0x1000, 0x3F, 3);

    threadStart(&cpuCore0Thread);
    threadStart(&cpuCore1Thread);
    threadStart(&cpuCore2Thread);
    threadStart(&cpuCore3Thread);
    threadStart(&miscThread);
    batteryInfoInitialize();

    if (hosversionAtLeast(6,0,0) && R_SUCCEEDED(pwmInitialize())) {
        pwmCheck = pwmOpenSession2(&g_ICon, 0x3D000001);
    }
    if(Board::GetConsoleType() != HorizonOCConsoleType_Hoag) {
        u64 clkVirtAddr, dsiVirtAddr, outsize;
        rc = svcQueryMemoryMapping(&clkVirtAddr, &outsize, 0x60006000, 0x1000);
        ASSERT_RESULT_OK(rc, "svcQueryMemoryMapping (clk)");
        rc = svcQueryMemoryMapping(&dsiVirtAddr, &outsize, 0x54300000, 0x40000);
        ASSERT_RESULT_OK(rc, "svcQueryMemoryMapping (dsi)");

        DisplayRefreshConfig cfg = {.clkVirtAddr = clkVirtAddr, .dsiVirtAddr = dsiVirtAddr};

        DisplayRefresh_Initialize(&cfg);
    }

    FetchHardwareInfos();
}

void Board::fuseReadSpeedos() {

    u64 pid = 0;
    if (R_FAILED(pmdmntGetProcessId(&pid, 0x0100000000000006))) {
        return;
    }

    Handle debug;
    if (R_FAILED(svcDebugActiveProcess(&debug, pid))) {
        return;
    }

    MemoryInfo mem_info = {0};
    u32 pageinfo = 0;
    u64 addr = 0;

    char stack[0x10] = {0};
    const char compare[0x10] = {0};
    char dump[0x400] = {0};

    while (true) {
        if (R_FAILED(svcQueryDebugProcessMemory(&mem_info, &pageinfo, debug, addr)) || mem_info.addr < addr) {
            break;
        }

        if (mem_info.type == MemType_Io && mem_info.size == 0x1000) {
            if (R_FAILED(svcReadDebugProcessMemory(stack, debug, mem_info.addr, sizeof(stack)))) {
                break;
            }

            if (memcmp(stack, compare, sizeof(stack)) == 0) {
                if (R_FAILED(svcReadDebugProcessMemory(dump, debug, mem_info.addr + 0x800, sizeof(dump)))) {
                    break;
                }

                cpuSpeedo0 = *reinterpret_cast<const u16*>(dump + FUSE_CPU_SPEEDO_0_CALIB);
                cpuSpeedo2 = *reinterpret_cast<const u16*>(dump + FUSE_CPU_SPEEDO_2_CALIB);
                socSpeedo0 = *reinterpret_cast<const u16*>(dump + FUSE_SOC_SPEEDO_0_CALIB);
                cpuIDDQ = *reinterpret_cast<const u16*>(dump + FUSE_CPU_IDDQ_CALIB);
                gpuIDDQ = *reinterpret_cast<const u16*>(dump + FUSE_SOC_IDDQ_CALIB);
                socIDDQ = *reinterpret_cast<const u16*>(dump + FUSE_GPU_IDDQ_CALIB);
                
                svcCloseHandle(debug);
                return;
            }
        }

        addr = mem_info.addr + mem_info.size;
    }

    svcCloseHandle(debug);
}

u16 Board::getSpeedo(HorizonOCSpeedo speedoType) {
    switch(speedoType) {
        case HorizonOCSpeedo_CPU:
            return cpuSpeedo0;
        case HorizonOCSpeedo_GPU:
            return cpuSpeedo2;
        case HorizonOCSpeedo_SOC:
            return socSpeedo0;
        default:
            ASSERT_ENUM_VALID(HorizonOCSpeedo, speedoType);
            return 0;
    }
}

u16 Board::getIDDQ(HorizonOCSpeedo speedoType) {
    switch(speedoType) {
        case HorizonOCSpeedo_CPU:
            return cpuIDDQ;
        case HorizonOCSpeedo_GPU:
            return gpuIDDQ;
        case HorizonOCSpeedo_SOC:
            return socIDDQ;
        default:
            ASSERT_ENUM_VALID(HorizonOCSpeedo, speedoType);
            return 0;
    }
}


void Board::Exit()
{
    if(HOSSVC_HAS_CLKRST)
    {
        clkrstExit();
    }
    else
    {
        pcvExit();
    }

    apmExtExit();
    psmExit();

    if(HOSSVC_HAS_TC)
    {
        tcExit();
    }

    max17050Exit();
    tmp451Exit();

    threadClose(&gpuLThread);
    threadClose(&cpuCore0Thread);
    threadClose(&cpuCore1Thread);
    threadClose(&cpuCore2Thread);
    threadClose(&cpuCore3Thread);
    threadClose(&miscThread);

    pwmChannelSessionClose(&g_ICon);
	pwmExit();
    rgltrExit();
    batteryInfoExit();
    pmdmntExit();
    if(Board::GetConsoleType() != HorizonOCConsoleType_Hoag)
        DisplayRefresh_Shutdown();
}

SysClkProfile Board::GetProfile()
{
    std::uint32_t mode = 0;
    Result rc = apmExtGetPerformanceMode(&mode);
    ASSERT_RESULT_OK(rc, "apmExtGetPerformanceMode");

    if(mode)
    {
        return SysClkProfile_Docked;
    }

    PsmChargerType chargerType;

    rc = psmGetChargerType(&chargerType);
    ASSERT_RESULT_OK(rc, "psmGetChargerType");

    if(chargerType == PsmChargerType_EnoughPower)
    {
        return SysClkProfile_HandheldChargingOfficial;
    }
    else if(chargerType == PsmChargerType_LowPower)
    {
        return SysClkProfile_HandheldChargingUSB;
    }

    return SysClkProfile_Handheld;
}

void Board::SetHz(SysClkModule module, std::uint32_t hz)
{
    Result rc = 0;

    if(module == HorizonOCModule_Display && Board::GetConsoleType() != HorizonOCConsoleType_Hoag) {
        DisplayRefresh_SetRate(hz);
        return;
    }
    if(module > SysClkModule_MEM)
        return;
    if(HOSSVC_HAS_CLKRST)
    {
        ClkrstSession session = {0};

        rc = clkrstOpenSession(&session, Board::GetPcvModuleId(module), 3);
        ASSERT_RESULT_OK(rc, "clkrstOpenSession");
        rc = clkrstSetClockRate(&session, hz);
        ASSERT_RESULT_OK(rc, "clkrstSetClockRate");

        clkrstCloseSession(&session);
    }
    else
    {
        rc = pcvSetClockRate(Board::GetPcvModule(module), hz);
        ASSERT_RESULT_OK(rc, "pcvSetClockRate");
    }
}

std::uint32_t Board::GetHz(SysClkModule module)
{
    Result rc = 0;
    std::uint32_t hz = 0;

    if(module == HorizonOCModule_Display) {
        if(Board::GetConsoleType() != HorizonOCConsoleType_Hoag)
            DisplayRefresh_GetRate(&hz, false);
        else
            hz = 60;
        return hz;
    }
    
    if(HOSSVC_HAS_CLKRST)
    {
        ClkrstSession session = {0};

        rc = clkrstOpenSession(&session, Board::GetPcvModuleId(module), 3);
        ASSERT_RESULT_OK(rc, "clkrstOpenSession");

        rc = clkrstGetClockRate(&session, &hz);
        ASSERT_RESULT_OK(rc, "clkrstSetClockRate");

        clkrstCloseSession(&session);
    }
    else
    {
        rc = pcvGetClockRate(Board::GetPcvModule(module), &hz);
        ASSERT_RESULT_OK(rc, "pcvGetClockRate");
    }

    return hz;
}

std::uint32_t Board::GetRealHz(SysClkModule module)
{
    u32 hz = 0;
    switch(module)
    {
        case SysClkModule_CPU:
            return t210ClkCpuFreq();
        case SysClkModule_GPU:
            return t210ClkGpuFreq();
        case SysClkModule_MEM:
            return t210ClkMemFreq();
        case HorizonOCModule_Display:
            if(Board::GetConsoleType() != HorizonOCConsoleType_Hoag)
                DisplayRefresh_GetRate(&hz, false);
            else
                hz = 60;
            return hz;
        default:
            ASSERT_ENUM_VALID(SysClkModule, module);
    }

    return 0;
}

void Board::GetFreqList(SysClkModule module, std::uint32_t* outList, std::uint32_t maxCount, std::uint32_t* outCount)
{
    Result rc = 0;
    PcvClockRatesListType type;
    s32 tmpInMaxCount = maxCount;
    s32 tmpOutCount = 0;



    if(HOSSVC_HAS_CLKRST)
    {
        ClkrstSession session = {0};

        rc = clkrstOpenSession(&session, Board::GetPcvModuleId(module), 3);
        ASSERT_RESULT_OK(rc, "clkrstOpenSession");

        rc = clkrstGetPossibleClockRates(&session, outList, tmpInMaxCount, &type, &tmpOutCount);
        ASSERT_RESULT_OK(rc, "clkrstGetPossibleClockRates");

        clkrstCloseSession(&session);
    }
    else
    {
        rc = pcvGetPossibleClockRates(Board::GetPcvModule(module), outList, tmpInMaxCount, &type, &tmpOutCount);
        ASSERT_RESULT_OK(rc, "pcvGetPossibleClockRates");
    }

    if(type != PcvClockRatesListType_Discrete)
    {
        ERROR_THROW("Unexpected PcvClockRatesListType: %u (module = %s)", type, Board::GetModuleName(module, false));
    }

    *outCount = tmpOutCount;
}

void Board::ResetToStock()
{
    Result rc = 0;
    if(hosversionAtLeast(9,0,0))
    {
        std::uint32_t confId = 0;
        rc = apmExtGetCurrentPerformanceConfiguration(&confId);
        ASSERT_RESULT_OK(rc, "apmExtGetCurrentPerformanceConfiguration");

        SysClkApmConfiguration* apmConfiguration = NULL;
        for(size_t i = 0; sysclk_g_apm_configurations[i].id; i++)
        {
            if(sysclk_g_apm_configurations[i].id == confId)
            {
                apmConfiguration = &sysclk_g_apm_configurations[i];
                break;
            }
        }

        if(!apmConfiguration)
        {
            ERROR_THROW("Unknown apm configuration: %x", confId);
        }

        Board::SetHz(SysClkModule_CPU, apmConfiguration->cpu_hz);
        Board::SetHz(SysClkModule_GPU, apmConfiguration->gpu_hz);
        Board::SetHz(SysClkModule_MEM, apmConfiguration->mem_hz);
    }
    else
    {
        std::uint32_t mode = 0;
        rc = apmExtGetPerformanceMode(&mode);
        ASSERT_RESULT_OK(rc, "apmExtGetPerformanceMode");

        rc = apmExtSysRequestPerformanceMode(mode);
        ASSERT_RESULT_OK(rc, "apmExtSysRequestPerformanceMode");
    }
}

void Board::ResetToStockCpu()
{
    Result rc = 0;
    if(hosversionAtLeast(9,0,0))
    {
        std::uint32_t confId = 0;
        rc = apmExtGetCurrentPerformanceConfiguration(&confId);
        ASSERT_RESULT_OK(rc, "apmExtGetCurrentPerformanceConfiguration");

        SysClkApmConfiguration* apmConfiguration = NULL;
        for(size_t i = 0; sysclk_g_apm_configurations[i].id; i++)
        {
            if(sysclk_g_apm_configurations[i].id == confId)
            {
                apmConfiguration = &sysclk_g_apm_configurations[i];
                break;
            }
        }

        if(!apmConfiguration)
        {
            ERROR_THROW("Unknown apm configuration: %x", confId);
        }

        Board::SetHz(SysClkModule_CPU, apmConfiguration->cpu_hz);
    }
    else
    {
        std::uint32_t mode = 0;
        rc = apmExtGetPerformanceMode(&mode);
        ASSERT_RESULT_OK(rc, "apmExtGetPerformanceMode");

        rc = apmExtSysRequestPerformanceMode(mode);
        ASSERT_RESULT_OK(rc, "apmExtSysRequestPerformanceMode");
    }
}

void Board::ResetToStockMem()
{
    Result rc = 0;
    if(hosversionAtLeast(9,0,0))
    {
        std::uint32_t confId = 0;
        rc = apmExtGetCurrentPerformanceConfiguration(&confId);
        ASSERT_RESULT_OK(rc, "apmExtGetCurrentPerformanceConfiguration");

        SysClkApmConfiguration* apmConfiguration = NULL;
        for(size_t i = 0; sysclk_g_apm_configurations[i].id; i++)
        {
            if(sysclk_g_apm_configurations[i].id == confId)
            {
                apmConfiguration = &sysclk_g_apm_configurations[i];
                break;
            }
        }

        if(!apmConfiguration)
        {
            ERROR_THROW("Unknown apm configuration: %x", confId);
        }

        Board::SetHz(SysClkModule_MEM, apmConfiguration->mem_hz);
    }
    else
    {
        std::uint32_t mode = 0;
        rc = apmExtGetPerformanceMode(&mode);
        ASSERT_RESULT_OK(rc, "apmExtGetPerformanceMode");

        rc = apmExtSysRequestPerformanceMode(mode);
        ASSERT_RESULT_OK(rc, "apmExtSysRequestPerformanceMode");
    }
}

void Board::ResetToStockGpu()
{
    Result rc = 0;
    if(hosversionAtLeast(9,0,0))
    {
        std::uint32_t confId = 0;
        rc = apmExtGetCurrentPerformanceConfiguration(&confId);
        ASSERT_RESULT_OK(rc, "apmExtGetCurrentPerformanceConfiguration");

        SysClkApmConfiguration* apmConfiguration = NULL;
        for(size_t i = 0; sysclk_g_apm_configurations[i].id; i++)
        {
            if(sysclk_g_apm_configurations[i].id == confId)
            {
                apmConfiguration = &sysclk_g_apm_configurations[i];
                break;
            }
        }

        if(!apmConfiguration)
        {
            ERROR_THROW("Unknown apm configuration: %x", confId);
        }

        Board::SetHz(SysClkModule_GPU, apmConfiguration->gpu_hz);
    }
    else
    {
        std::uint32_t mode = 0;
        rc = apmExtGetPerformanceMode(&mode);
        ASSERT_RESULT_OK(rc, "apmExtGetPerformanceMode");

        rc = apmExtSysRequestPerformanceMode(mode);
        ASSERT_RESULT_OK(rc, "apmExtSysRequestPerformanceMode");
    }
}

void Board::ResetToStockDisplay() {
    if(Board::GetConsoleType() != HorizonOCConsoleType_Hoag)
        DisplayRefresh_SetRate(60);
}

u8 Board::GetHighestDockedDisplayRate() {
    if(Board::GetConsoleType() != HorizonOCConsoleType_Hoag)
        return DisplayRefresh_GetDockedHighestAllowed();
    else
        return 60;
}

std::uint32_t Board::GetTemperatureMilli(SysClkThermalSensor sensor)
{
    std::int32_t millis = 0;

    if(sensor == SysClkThermalSensor_SOC)
    {
        millis = tmp451TempSoc();
    }
    else if(sensor == SysClkThermalSensor_PCB)
    {
        millis = tmp451TempPcb();
    }
    else if(sensor == SysClkThermalSensor_Skin)
    {
        if(HOSSVC_HAS_TC)
        {
            Result rc;
            rc = tcGetSkinTemperatureMilliC(&millis);
            ASSERT_RESULT_OK(rc, "tcGetSkinTemperatureMilliC");
        }
    }
    else if (sensor == HorizonOCThermalSensor_Battery) {
        batteryInfoGetChargeInfo(&info);
        millis = batteryInfoGetTemperatureMiliCelsius(&info);
    }
    else if (sensor == HorizonOCThermalSensor_PMIC) {
        millis = 50000;
    }
    else
    {
        ASSERT_ENUM_VALID(SysClkThermalSensor, sensor);
    }

    return std::max(0, millis);
}

std::int32_t Board::GetPowerMw(SysClkPowerSensor sensor)
{
    switch(sensor)
    {
        case SysClkPowerSensor_Now:
            return max17050PowerNow();
        case SysClkPowerSensor_Avg:
            return max17050PowerAvg();
        default:
            ASSERT_ENUM_VALID(SysClkPowerSensor, sensor);
    }

    return 0;
}

std::uint32_t Board::GetPartLoad(SysClkPartLoad loadSource)
{		
    switch(loadSource)
    {
        case SysClkPartLoad_EMC:
            return t210EmcLoadAll();
        case SysClkPartLoad_EMCCpu:
            return t210EmcLoadCpu();
        case HocClkPartLoad_GPU:
            return GPU_Load_u;
        case HocClkPartLoad_CPUAvg:
            return idletick0;
        case HocClkPartLoad_BAT:
            batteryInfoGetChargeInfo(&info);
            return info.BatteryAge;
        case HocClkPartLoad_FAN:
            return fanLevel;
        default:
            ASSERT_ENUM_VALID(SysClkPartLoad, loadSource);
    }

    return 0;
}


SysClkSocType Board::GetSocType() {
    return g_socType;
}

HorizonOCConsoleType Board::GetConsoleType() {
    return g_consoleType;
}

u8 Board::GetDramID() {
    return g_dramID;
}

void Board::FetchHardwareInfos()
{
    fuseReadSpeedos();
    u64 sku = 0, dramID = 0;
    Result rc = splInitialize();
    ASSERT_RESULT_OK(rc, "splInitialize");

    rc = splGetConfig(SplConfigItem_HardwareType, &sku);
    ASSERT_RESULT_OK(rc, "splGetConfig");

    rc = splGetConfig(SplConfigItem_DramId, &dramID);
    ASSERT_RESULT_OK(rc, "splGetConfig");
    
    splExit();

    switch(sku)
    {
        case 2:
        case 3:
        case 4:
        case 5:
            g_socType = SysClkSocType_Mariko;
            break;
        default:
            g_socType = SysClkSocType_Erista;
    }

    g_consoleType = (HorizonOCConsoleType)sku;
    g_dramID = (u8)dramID;

}

/*
* Switch Power domains (max77620):
* Name  | Usage         | uV step | uV min | uV default | uV max  | Init
*-------+---------------+---------+--------+------------+---------+------------------
*  sd0  | SoC           | 12500   | 600000 |  625000    | 1400000 | 1.125V (pkg1.1)
*  sd1  | SDRAM         | 12500   | 600000 | 1125000    | 1125000 | 1.1V   (pkg1.1)
*  sd2  | ldo{0-1, 7-8} | 12500   | 600000 | 1325000    | 1350000 | 1.325V (pcv)
*  sd3  | 1.8V general  | 12500   | 600000 | 1800000    | 1800000 |
*  ldo0 | Display Panel | 25000   | 800000 | 1200000    | 1200000 | 1.2V   (pkg1.1)
*  ldo1 | XUSB, PCIE    | 25000   | 800000 | 1050000    | 1050000 | 1.05V  (pcv)
*  ldo2 | SDMMC1        | 50000   | 800000 | 1800000    | 3300000 |
*  ldo3 | GC ASIC       | 50000   | 800000 | 3100000    | 3100000 | 3.1V   (pcv)
*  ldo4 | RTC           | 12500   | 800000 |  850000    |  850000 | 0.85V  (AO, pcv)
*  ldo5 | GC Card       | 50000   | 800000 | 1800000    | 1800000 | 1.8V   (pcv)
*  ldo6 | Touch, ALS    | 50000   | 800000 | 2900000    | 2900000 | 2.9V   (pcv)
*  ldo7 | XUSB          | 50000   | 800000 | 1050000    | 1050000 | 1.05V  (pcv)
*  ldo8 | XUSB, DP, MCU | 50000   | 800000 | 1050000    | 2800000 | 1.05V/2.8V (pcv)

typedef enum {
    PcvPowerDomainId_Max77620_Sd0  = 0x3A000080,
    PcvPowerDomainId_Max77620_Sd1  = 0x3A000081, // vdd2
    PcvPowerDomainId_Max77620_Sd2  = 0x3A000082,
    PcvPowerDomainId_Max77620_Sd3  = 0x3A000083,
    PcvPowerDomainId_Max77620_Ldo0 = 0x3A0000A0,
    PcvPowerDomainId_Max77620_Ldo1 = 0x3A0000A1,
    PcvPowerDomainId_Max77620_Ldo2 = 0x3A0000A2,
    PcvPowerDomainId_Max77620_Ldo3 = 0x3A0000A3,
    PcvPowerDomainId_Max77620_Ldo4 = 0x3A0000A4,
    PcvPowerDomainId_Max77620_Ldo5 = 0x3A0000A5,
    PcvPowerDomainId_Max77620_Ldo6 = 0x3A0000A6,
    PcvPowerDomainId_Max77620_Ldo7 = 0x3A0000A7,
    PcvPowerDomainId_Max77620_Ldo8 = 0x3A0000A8,
    PcvPowerDomainId_Max77621_Cpu  = 0x3A000003,
    PcvPowerDomainId_Max77621_Gpu  = 0x3A000004,
    PcvPowerDomainId_Max77812_Cpu  = 0x3A000003,
    PcvPowerDomainId_Max77812_Gpu  = 0x3A000004,
    PcvPowerDomainId_Max77812_Dram = 0x3A000005, // vddq
} PowerDomainId;

*/

std::uint32_t Board::GetVoltage(HocClkVoltage voltage)
{
    RgltrSession session;
    Result rc = 0;
    u32 out = 0;
    switch(voltage)
    {
        case HocClkVoltage_SOC:
            rc = rgltrOpenSession(&session, PcvPowerDomainId_Max77620_Sd0);
            ASSERT_RESULT_OK(rc, "rgltrOpenSession")
            rgltrGetVoltage(&session, &out);
            rgltrCloseSession(&session);
            break;
        case HocClkVoltage_EMCVDD2:
            rc = rgltrOpenSession(&session, PcvPowerDomainId_Max77620_Sd1);
            ASSERT_RESULT_OK(rc, "rgltrOpenSession")
            rgltrGetVoltage(&session, &out);
            rgltrCloseSession(&session);
            break;
        case HocClkVoltage_CPU:
            if(Board::GetSocType() == SysClkSocType_Mariko)
                rc = rgltrOpenSession(&session, PcvPowerDomainId_Max77621_Cpu);
            else
                rc = rgltrOpenSession(&session, PcvPowerDomainId_Max77812_Cpu);
            ASSERT_RESULT_OK(rc, "rgltrOpenSession")
            rgltrGetVoltage(&session, &out);
            rgltrCloseSession(&session);
            break;
        case HocClkVoltage_GPU:
            if(Board::GetSocType() == SysClkSocType_Mariko)
                rc = rgltrOpenSession(&session, PcvPowerDomainId_Max77621_Gpu);
            else
                rc = rgltrOpenSession(&session, PcvPowerDomainId_Max77812_Gpu);
            ASSERT_RESULT_OK(rc, "rgltrOpenSession")
            rgltrGetVoltage(&session, &out);
            rgltrCloseSession(&session);
            break;
        case HocClkVoltage_EMCVDDQ_MarikoOnly:
            if(Board::GetSocType() == SysClkSocType_Mariko) {
                rc = rgltrOpenSession(&session, PcvPowerDomainId_Max77812_Dram);
                ASSERT_RESULT_OK(rc, "rgltrOpenSession")
                rgltrGetVoltage(&session, &out);
                rgltrCloseSession(&session);
            } else {
                out = Board::GetVoltage(HocClkVoltage_EMCVDD2);
            }
            break;
        case HocClkVoltage_Display:
            rc = rgltrOpenSession(&session, PcvPowerDomainId_Max77620_Ldo0);
            ASSERT_RESULT_OK(rc, "rgltrOpenSession")
            rgltrGetVoltage(&session, &out);
            rgltrCloseSession(&session);
            break;
        case HocClkVoltage_Battery:
            batteryInfoGetChargeInfo(&info);
            out = info.VoltageAvg;
            break;
        default:
            ASSERT_ENUM_VALID(HocClkVoltage, voltage);
    }

    return out > 0 ? out : 0;
}

#define MC_REGISTER_BASE 0x70019000
#define MC_REGISTER_REGION_SIZE 0x1000

#define EMC_REGISTER_BASE 0x7001b000
#define EMC_REGISTER_REGION_SIZE 0x1000

#define GET_CYCLE_CEIL(PARAM) u32(CEIL(double(PARAM) / tCK_avg))

#define WRITE_REGISTER_EMC(TIMING_OFFSET, VALUE)                \
    do {                                                    \
        args = {};                                          \
        args.X[0] = 0xF0000002;                             \
        args.X[1] = EMC_REGISTER_BASE + (TIMING_OFFSET);    \
        args.X[2] = 0xFFFFFFFF;                             \
        args.X[3] = (VALUE);                                \
        svcCallSecureMonitor(&args);                        \
    } while (false)

#define WRITE_REGISTER_MC(TIMING_OFFSET, VALUE)                \
    do {                                                    \
        args = {};                                          \
        args.X[0] = 0xF0000002;                             \
        args.X[1] = MC_REGISTER_BASE + (TIMING_OFFSET);    \
        args.X[2] = 0xFFFFFFFF;                             \
        args.X[3] = (VALUE);                                \
        svcCallSecureMonitor(&args);                        \
    } while (false)


// NOTE: needs patch to exosphere to expose emc region to secmon. MC does NOT need this patch

u32 tRCD_values[]  =  { 18, 17, 16, 15, 14, 13, 12, 11 };
u32 tRP_values[]   =  { 18, 17, 16, 15, 14, 13, 12, 11 };
u32 tRAS_values[]  =  { 42, 36, 34, 32, 30, 28, 26, 24, 22, 20 };
double tRRD_values[]   = { /*10.0,*/ 7.5, 6.0, 5.0, 4.0, 3.0, 2.0, 1.0 }; /* 10.0 is used for <2133mhz; do we care? */
u32 tRFC_values[]   = { 140, 130, 120, 110, 100, 90, 80, 70, 60, 50, 40 };
u32 tWTR_values[]   = { 10, 9, 8, 7, 6, 5, 4, 3, 2, 1 };
u32 tREFpb_values[] = { 3900, 5850, 7800, 11700, 15600, 99999 };

// Credit to Lightos for these timings!


void Board::UpdateShadowRegs(u32 tRCD_i, u32 tRP_i, u32 tRAS_i, u32 tRRD_i, u32 tRFC_i, u32 tRTW_i, u32 tWTR_i, u32 tREFpb_i, u32 ramFreq, u32 rlAdd, u32 wlAdd, bool hpMode) {
    // timing stuff

    SecmonArgs args = {};

    constexpr double MC_ARB_DIV = 4.0;
    constexpr u32 MC_ARB_SFA = 2;

    double tCK_avg = 1000'000.0 / ramFreq;
    u32 BL = 16;
    u32 RL = 28 + rlAdd;
    u32 WL = 14 + wlAdd;
    u32 RL_DBI = RL + 4;

    u32 tRCD = tRCD_values[tRCD_i];
    u32 tRPpb = tRP_values[tRP_i];
    u32 tRAS = tRAS_values[tRAS_i];
    double tRRD = tRRD_values[tRRD_i];
    u32 tRFCpb = tRFC_values[tRFC_i];
    u32 tWTR = 10 - tWTR_values[tWTR_i];
    u32 tFAW = static_cast<u32>(tRRD * 4.0);

    double tDQSCK_max = 3.5;
    u32 tWPRE = 2;

    double tRPST = 0.5;

    u32 tR2W = CEIL(RL_DBI + (tDQSCK_max / tCK_avg) + (BL / 2) - WL + tWPRE + FLOOR(tRPST) + 9.0) - (tRTW_i * 3);

    u32 tRC = tRAS + tRPpb;
    u32 tRFCab = tRFCpb * 2;
    u32 tRPab = tRPpb + 3;

    u32 tW2R = CEIL(MAX(WL + (0.010322547033278747 * (ramFreq / 1000.0)), (WL * -0.2067922202979121) + FLOOR(((RL_DBI * -0.1331159971685554) + WL) * 3.654131957826108)) - (tWTR / tCK_avg));

    double tMMRI = tRCD + (tCK_avg * 3);
    double pdex2mrr = tMMRI + 10;
    u32 emc_cfg = hpMode ? 0x13200000 : 0xF3200000;

    u32 refresh_raw = 0xFFFF;
    if (tREFpb_i != 6) {
        refresh_raw = CEIL(tREFpb_values[tREFpb_i] / tCK_avg) - 0x40;
        refresh_raw = MIN(refresh_raw, static_cast<u32>(0xFFFF));
    }

    u32 trefbw = refresh_raw + 0x40;
    trefbw = MIN(trefbw, static_cast<u32>(0x3FFF));

    u32 tR2P = 12 + (rlAdd / 2);
    u32 tW2P = (CEIL(WL * 1.7303) * 2) - 5;

    double tXSR = (double) (tRFCab + 7.5);

    args = {};                                         
    args.X[0] = 0xF0000002;                             
    args.X[1] = EMC_REGISTER_BASE + EMC_INTSTATUS_0;    
    svcCallSecureMonitor(&args);                        

    if(args.X[1] == (EMC_REGISTER_BASE + EMC_INTSTATUS_0)) { // if param 1 is identical read failed, exosphere needs patch!
        writeNotification("Horizon OC\nExosphere not patched\nfor EMC r/w"); 
        return;
    }

    // actually write the timings
    WRITE_REGISTER_EMC(EMC_CFG_0, emc_cfg);
    WRITE_REGISTER_EMC(EMC_RD_RCD_0, GET_CYCLE_CEIL(tRCD));
    WRITE_REGISTER_EMC(EMC_WR_RCD_0, GET_CYCLE_CEIL(tRCD));
    WRITE_REGISTER_EMC(EMC_RC_0, MIN(GET_CYCLE_CEIL(tRC), static_cast<u32>(0xB8)));
    WRITE_REGISTER_EMC(EMC_RAS_0, MIN(GET_CYCLE_CEIL(tRAS), static_cast<u32>(0x7F)));
    WRITE_REGISTER_EMC(EMC_RRD_0, GET_CYCLE_CEIL(tRRD));
    WRITE_REGISTER_EMC(EMC_RFCPB_0, GET_CYCLE_CEIL(tRFCpb));
    WRITE_REGISTER_EMC(EMC_RFC_0, GET_CYCLE_CEIL(tRFCab));
    WRITE_REGISTER_EMC(EMC_RP_0, GET_CYCLE_CEIL(tRPpb));
    WRITE_REGISTER_EMC(EMC_TRPAB_0, MIN(GET_CYCLE_CEIL(tRPab), static_cast<u32>(0x3F)));
    WRITE_REGISTER_EMC(EMC_R2W_0, tR2W);
    WRITE_REGISTER_EMC(EMC_W2R_0, tW2R);
    WRITE_REGISTER_EMC(EMC_REFRESH_0, refresh_raw);
    WRITE_REGISTER_EMC(EMC_PRE_REFRESH_REQ_CNT_0, refresh_raw / 4);
    WRITE_REGISTER_EMC(EMC_TREFBW_0, trefbw);
    WRITE_REGISTER_EMC(EMC_PDEX2MRR_0, GET_CYCLE_CEIL(pdex2mrr));
    WRITE_REGISTER_EMC(EMC_TXSR_0, MIN(GET_CYCLE_CEIL(tXSR), static_cast<u32>(0x3fe)));
    WRITE_REGISTER_EMC(EMC_TXSRDLL_0, MIN(GET_CYCLE_CEIL(tXSR), static_cast<u32>(0x3fe)));

    WRITE_REGISTER_MC(MC_EMEM_ARB_TIMING_RCD_0, CEIL(GET_CYCLE_CEIL(tRCD)   / MC_ARB_DIV) - 2);
    WRITE_REGISTER_MC(MC_EMEM_ARB_TIMING_RP_0, CEIL(GET_CYCLE_CEIL(tRPpb)  / MC_ARB_DIV) - 1);
    WRITE_REGISTER_MC(MC_EMEM_ARB_TIMING_RC_0, CEIL(GET_CYCLE_CEIL(tRC)    / MC_ARB_DIV) - 1);
    WRITE_REGISTER_MC(MC_EMEM_ARB_TIMING_RAS_0, CEIL(GET_CYCLE_CEIL(tRAS)   / MC_ARB_DIV) - 2);
    WRITE_REGISTER_MC(MC_EMEM_ARB_TIMING_FAW_0, CEIL(GET_CYCLE_CEIL(tFAW)   / MC_ARB_DIV) - 1);
    WRITE_REGISTER_MC(MC_EMEM_ARB_TIMING_RRD_0, CEIL(GET_CYCLE_CEIL(tRRD)   / MC_ARB_DIV) - 1);
    WRITE_REGISTER_MC(MC_EMEM_ARB_TIMING_RFCPB_0, CEIL(GET_CYCLE_CEIL(tRFCpb) / MC_ARB_DIV) - 1);

    WRITE_REGISTER_MC(MC_EMEM_ARB_TIMING_R2W_0, CEIL(tR2W / MC_ARB_DIV) - 1 + MC_ARB_SFA);
    WRITE_REGISTER_MC(MC_EMEM_ARB_TIMING_W2R_0, CEIL(tW2R / MC_ARB_DIV) - 1 + MC_ARB_SFA);

    WRITE_REGISTER_MC(MC_EMEM_ARB_TIMING_RAP2PRE_0, CEIL(tR2P / MC_ARB_DIV));
    WRITE_REGISTER_MC(MC_EMEM_ARB_TIMING_WAP2PRE_0, CEIL(tW2P / MC_ARB_DIV) + MC_ARB_SFA);

    u32 da_turns = 0;
    da_turns |= u8((CEIL(tR2W / MC_ARB_DIV) - 1 + MC_ARB_SFA) / 2) << 16;
    da_turns |= u8((CEIL(tW2R / MC_ARB_DIV) - 1 + MC_ARB_SFA) / 2) << 24;
    WRITE_REGISTER_MC(MC_EMEM_ARB_DA_TURNS_0, da_turns);

    u32 da_covers = 0;
    u8 r_cover = ((CEIL(tR2P / MC_ARB_DIV)) + (CEIL(GET_CYCLE_CEIL(tRPpb)  / MC_ARB_DIV) - 1) + (CEIL(GET_CYCLE_CEIL(tRCD)   / MC_ARB_DIV) - 2)) / 2;
    u8 w_cover = ((CEIL(tW2P / MC_ARB_DIV) + MC_ARB_SFA) + (CEIL(GET_CYCLE_CEIL(tRPpb)  / MC_ARB_DIV) - 1) + (CEIL(GET_CYCLE_CEIL(tRCD)   / MC_ARB_DIV) - 2)) / 2;
    da_covers |= ((u32)(CEIL(GET_CYCLE_CEIL(tRC) / (u32)MC_ARB_DIV) - 1) / 2);
    da_covers |= (r_cover << 8);
    da_covers |= (w_cover << 16);

    WRITE_REGISTER_MC(MC_EMEM_ARB_DA_COVERS_0, da_covers);
    // TODO: modify mc_emem_arb_misc0
    
    WRITE_REGISTER_MC(MC_TIMING_CONTROL_0, 0x1); // update timing regs as they are shadowed
    WRITE_REGISTER_EMC(EMC_TIMING_CONTROL_0, 0x1);
}


bool Board::IsDram8GB() {
    SecmonArgs args = {};                                         
    args.X[0] = 0xF0000002;                             
    args.X[1] = MC_REGISTER_BASE + MC_EMEM_CFG_0;    
    svcCallSecureMonitor(&args);                        

    if(args.X[1] == (MC_REGISTER_BASE + MC_EMEM_CFG_0)) { // if param 1 is identical read failed
        writeNotification("Horizon OC\nSecmon read failed!\n This may be a hardware issue!"); 
        return false;
    }  else
        return args.X[1] == 0x00002000 ? true : false;
}