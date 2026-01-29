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

typedef enum {
    SysClkConfigValue_PollingIntervalMs = 0,
    SysClkConfigValue_TempLogIntervalMs,
    SysClkConfigValue_FreqLogIntervalMs,
    SysClkConfigValue_PowerLogIntervalMs,
    SysClkConfigValue_CsvWriteIntervalMs,

    SysClkConfigValue_OverwriteBoostMode,

    SysClkConfigValue_ThermalThrottle,
    SysClkConfigValue_ThermalThrottleThreshold,

    SysClkConfigValue_HandheldTDP,
    SysClkConfigValue_HandheldTDPLimit,

    SysClkConfigValue_LiteTDPLimit,

    SysClkConfigValue_EnforceBoardLimit,

    SysClkConfigValue_FixCpuVoltBug,

    SysClkConfigValue_EnumMax,
} SysClkConfigValue;

typedef struct {
    uint64_t values[SysClkConfigValue_EnumMax];
} SysClkConfigValueList;

static inline const char* sysclkFormatConfigValue(SysClkConfigValue val, bool pretty)
{
    switch(val)
    {
        case SysClkConfigValue_PollingIntervalMs:
            return pretty ? "Polling Interval (ms)" : "poll_interval_ms";
        case SysClkConfigValue_TempLogIntervalMs:
            return pretty ? "Temperature logging interval (ms)" : "temp_log_interval_ms";
        case SysClkConfigValue_FreqLogIntervalMs:
            return pretty ? "Frequency logging interval (ms)" : "freq_log_interval_ms";
        case SysClkConfigValue_PowerLogIntervalMs:
            return pretty ? "Power logging interval (ms)" : "power_log_interval_ms";
        case SysClkConfigValue_CsvWriteIntervalMs:
            return pretty ? "CSV write interval (ms)" : "csv_write_interval_ms";

        case SysClkConfigValue_OverwriteBoostMode:
            return pretty ? "Overwrite Boost Mode" : "ow_boost";

        case SysClkConfigValue_ThermalThrottle:
            return pretty ? "Thermal Throttle" : "thermal_throttle";

        case SysClkConfigValue_ThermalThrottleThreshold:
            return pretty ? "Thermal Throttle Threshold" : "thermal_throttle_threshold";

        case SysClkConfigValue_HandheldTDP:
            return pretty ? "Handheld TDP" : "handheld_tdp";

        case SysClkConfigValue_HandheldTDPLimit:
            return pretty ? "Handheld TDP Limit" : "tdp_limit";

        case SysClkConfigValue_LiteTDPLimit:
            return pretty ? "Handheld TDP Limit" : "tdp_limit_l";

        case SysClkConfigValue_EnforceBoardLimit:
            return pretty ? "Enforce Board Limit" : "enforce_board_limit";

        case SysClkConfigValue_FixCpuVoltBug:
            return pretty ? "Fix CPU Volt Bug" : "cpu_volt_bugfix";
        default:
            return pretty ? "[cfg] no enum format string" : "err_no_format_string";
    }
}

static inline uint64_t sysclkDefaultConfigValue(SysClkConfigValue val)
{
    switch(val)
    {
        case SysClkConfigValue_PollingIntervalMs:
            return 300ULL;
        case SysClkConfigValue_TempLogIntervalMs:
        case SysClkConfigValue_FreqLogIntervalMs:
        case SysClkConfigValue_PowerLogIntervalMs:
        case SysClkConfigValue_CsvWriteIntervalMs:
        case SysClkConfigValue_ThermalThrottle:
        case SysClkConfigValue_HandheldTDP:
        case SysClkConfigValue_EnforceBoardLimit:
        case SysClkConfigValue_FixCpuVoltBug:
            return 1ULL;
        case SysClkConfigValue_OverwriteBoostMode:
            return 0ULL;

        case SysClkConfigValue_ThermalThrottleThreshold:
            return 70ULL;
        case SysClkConfigValue_HandheldTDPLimit:
            return 8600ULL;
        case SysClkConfigValue_LiteTDPLimit:
            return 6400ULL;
        default:
            return 0ULL;
    }
}

static inline uint64_t sysclkValidConfigValue(SysClkConfigValue val, uint64_t input)
{
    switch(val)
    {
        case SysClkConfigValue_ThermalThrottleThreshold:
        case SysClkConfigValue_HandheldTDPLimit:
        case SysClkConfigValue_LiteTDPLimit:
        case SysClkConfigValue_PollingIntervalMs:
            return input > 0;
        
        case SysClkConfigValue_TempLogIntervalMs:
        case SysClkConfigValue_FreqLogIntervalMs:
        case SysClkConfigValue_PowerLogIntervalMs:
        case SysClkConfigValue_CsvWriteIntervalMs:
        case SysClkConfigValue_OverwriteBoostMode:
        case SysClkConfigValue_ThermalThrottle:
        case SysClkConfigValue_HandheldTDP:
        case SysClkConfigValue_EnforceBoardLimit:
        case SysClkConfigValue_FixCpuVoltBug:
            return (input & 0x1) == input;
                default:
            return false;
    }
}