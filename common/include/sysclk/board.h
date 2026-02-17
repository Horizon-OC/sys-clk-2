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
#include <stddef.h>
#include <stdbool.h>

typedef enum
{
    SysClkSocType_Erista = 0,
    SysClkSocType_Mariko,
    SysClkSocType_EnumMax
} SysClkSocType;

typedef enum
{
    SysClkConsoleType_Icosa = 0,
    SysClkConsoleType_Copper,
    SysClkConsoleType_Hoag,
    SysClkConsoleType_Iowa,
    SysClkConsoleType_Calcio,
    SysClkConsoleType_Aula,
    SysClkConsoleType_EnumMax,
} SysClkConsoleType;

typedef enum {
    SysClkVoltage_SOC = 0,
    SysClkVoltage_EMCVDD2,
    SysClkVoltage_CPU,
    SysClkVoltage_GPU,
    SysClkVoltage_EMCVDDQ_MarikoOnly,
    SysClkVoltage_Display,
    SysClkVoltage_Battery,
    SysClkVoltage_EnumMax,
} SysClkVoltage;

typedef enum
{
    SysClkProfile_Handheld = 0,
    SysClkProfile_HandheldCharging,
    SysClkProfile_HandheldChargingUSB,
    SysClkProfile_HandheldChargingOfficial,
    SysClkProfile_Docked,
    SysClkProfile_EnumMax
} SysClkProfile;

typedef enum
{
    SysClkModule_CPU = 0,
    SysClkModule_GPU,
    SysClkModule_MEM,
    SysClkModule_EnumMax,
} SysClkModule;

typedef enum
{
    SysClkThermalSensor_SOC = 0,
    SysClkThermalSensor_PCB,
    SysClkThermalSensor_Skin,
    SysClkThermalSensor_Battery,
    SysClkThermalSensor_PMIC,
    SysClkThermalSensor_EnumMax
} SysClkThermalSensor;

typedef enum
{
    SysClkPowerSensor_Now = 0,
    SysClkPowerSensor_Avg,
    SysClkPowerSensor_EnumMax
} SysClkPowerSensor;

typedef enum
{
    SysClkPartLoad_EMC = 0,
    SysClkPartLoad_EMCCpu,
    SysClkPartLoad_GPU,
    SysClkPartLoad_CPUMax,
    SysClkPartLoad_BAT,
    SysClkPartLoad_FAN,
    SysClkPartLoad_EnumMax
} SysClkPartLoad;


typedef enum
{
    ReverseNX_NotFound = 0,
    ReverseNX_SystemDefault = 0,
    ReverseNX_Handheld,
    ReverseNX_Docked,
} ReverseNXMode;


typedef enum {
    SysClkSpeedo_CPU = 0,
    SysClkSpeedo_GPU,
    SysClkSpeedo_SOC,
    SysClkSpeedo_EnumMax,
} SysClkSpeedo;

typedef enum {
    GpuSchedulingMode_DoNotOverride = 0,
    GpuSchedulingMode_Enabled,
    GpuSchedulingMode_Disabled,
    GpuSchedulingMode_EnumMax,
} GpuSchedulingMode;

#define SYSCLK_ENUM_VALID(n, v) ((v) < n##_EnumMax)

static inline const char* sysclkFormatModule(SysClkModule module, bool pretty)
{
    switch(module)
    {
        case SysClkModule_CPU:
            return pretty ? "CPU" : "cpu";
        case SysClkModule_GPU:
            return pretty ? "GPU" : "gpu";
        case SysClkModule_MEM:
            return pretty ? "Memory" : "mem";
        default:
            return "null";
    }
}

static inline const char* sysclkFormatThermalSensor(SysClkThermalSensor thermSensor, bool pretty)
{
    switch(thermSensor)
    {
        case SysClkThermalSensor_SOC:
            return pretty ? "SOC" : "soc";
        case SysClkThermalSensor_PCB:
            return pretty ? "PCB" : "pcb";
        case SysClkThermalSensor_Skin:
            return pretty ? "Skin" : "skin";
        case SysClkThermalSensor_Battery:
            return pretty ? "BAT" : "battery";
        case SysClkThermalSensor_PMIC:
            return pretty ? "PMIC" : "pmic";

        default:
            return NULL;
    }
}

static inline const char* sysclkFormatPowerSensor(SysClkPowerSensor powSensor, bool pretty)
{
    switch(powSensor)
    {
        case SysClkPowerSensor_Now:
            return pretty ? "Now" : "now";
        case SysClkPowerSensor_Avg:
            return pretty ? "Avg" : "avg";
        default:
            return NULL;
    }
}

static inline const char* sysclkFormatProfile(SysClkProfile profile, bool pretty)
{
    switch(profile)
    {
        case SysClkProfile_Docked:
            return pretty ? "Docked" : "docked";
        case SysClkProfile_Handheld:
            return pretty ? "Handheld" : "handheld";
        case SysClkProfile_HandheldCharging:
            return pretty ? "Charging" : "handheld_charging";
        case SysClkProfile_HandheldChargingUSB:
            return pretty ? "USB Charger" : "handheld_charging_usb";
        case SysClkProfile_HandheldChargingOfficial:
            return pretty ? "PD Charger" : "handheld_charging_official";
        default:
            return NULL;
    }
}


static inline const char* SysClkFormatVoltage(SysClkVoltage voltage, bool pretty)
{
    switch(voltage)
    {
        case SysClkVoltage_CPU:
            return pretty ? "CPU" : "cpu";
        case SysClkVoltage_GPU:
            return pretty ? "GPU" : "gpu";
        case SysClkVoltage_EMCVDD2:
            return pretty ? "VDD2" : "emcvdd2";
        case SysClkVoltage_EMCVDDQ_MarikoOnly:
            return pretty ? "VDDQ" : "vddq";
        case SysClkVoltage_SOC:
            return pretty ? "SOC" : "soc";
        case SysClkVoltage_Display:
            return pretty ? "Display" : "display";
        default:
            return NULL;
    }
}