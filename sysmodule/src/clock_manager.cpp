 
/* --------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <p-sam@d3vs.net>, <natinusala@gmail.com>, <m4x@m4xw.net>
 * wrote this file. As long as you retain this notice you can do whatever you
 * want with this stuff. If you meet any of us some day, and you think this
 * stuff is worth it, you can buy us a beer in return.  - The sys-clk authors
 * --------------------------------------------------------------------------
 */


#include "clock_manager.h"
#include <cstring>
#include "file_utils.h"
#include "board.h"
#include "process_management.h"
#include "errors.h"
#include "ipc_service.h"
#include <cstring>
#include <cstdio>

#define HOSPPC_HAS_BOOST (hosversionAtLeast(7,0,0))

ClockManager *ClockManager::GetInstance()
{
    return instance;
}

void ClockManager::Exit()
{
    if (instance)
    {
        delete instance;
    }
}

void ClockManager::Initialize()
{
    if (!instance)
    {
        instance = new ClockManager();
    }
}

ClockManager::ClockManager()
{
    this->config = Config::CreateDefault();
    this->context = new SysClkContext;
    this->context->applicationId = 0;
    this->context->profile = SysClkProfile_Handheld;
    for (unsigned int module = 0; module < SysClkModule_EnumMax; module++)
    {
        this->context->freqs[module] = 0;
        this->context->realFreqs[module] = 0;
        this->context->overrideFreqs[module] = 0;
        this->RefreshFreqTableRow((SysClkModule)module);
    }

    this->running = false;
    this->lastTempLogNs = 0;
    this->lastCsvWriteNs = 0;

    for(int i = 0; i < SysClkSpeedo_EnumMax; i++) {
        this->context->speedos[i] = Board::getSpeedo((SysClkSpeedo)i);
        this->context->iddq[i] = Board::getIDDQ((SysClkSpeedo)i);
    }

    this->context->dramID = Board::GetDramID();
    this->context->isDram8GB = Board::IsDram8GB();
}

ClockManager::~ClockManager()
{
    delete this->config;
    delete this->context;
}

SysClkContext ClockManager::GetCurrentContext()
{
    std::scoped_lock lock{this->contextMutex};
    return *this->context;
}

Config *ClockManager::GetConfig()
{
    return this->config;
}

void ClockManager::SetRunning(bool running)
{
    this->running = running;
}

bool ClockManager::Running()
{
    return this->running;
}

void ClockManager::GetFreqList(SysClkModule module, std::uint32_t *list, std::uint32_t maxCount, std::uint32_t *outCount)
{
    ASSERT_ENUM_VALID(SysClkModule, module);

    *outCount = std::min(maxCount, this->freqTable[module].count);
    memcpy(list, &this->freqTable[module].list[0], *outCount * sizeof(this->freqTable[0].list[0]));
}

bool ClockManager::IsAssignableHz(SysClkModule module, std::uint32_t hz)
{
    switch (module)
    {
    case SysClkModule_CPU:
        return hz >= 600000000;
    case SysClkModule_MEM:
        return hz >= 665600000;
    default:
        return true;
    }
}

std::uint32_t ClockManager::GetMaxAllowedHz(SysClkModule module, SysClkProfile profile)
{
    if (module == SysClkModule_GPU)
    {
        if (profile < SysClkProfile_HandheldCharging)
        {
            switch(Board::GetSocType()) {
                case SysClkSocType_Erista:
                    return 460800000;
                case SysClkSocType_Mariko:
                    return 614400000;
                default:
                    return 0;
            }
        }
        else if (profile <= SysClkProfile_HandheldChargingUSB)
        {
            return 768000000;
        }
    }
    return 0;
}

std::uint32_t ClockManager::GetNearestHz(SysClkModule module, std::uint32_t inHz, std::uint32_t maxHz)
{
    std::uint32_t *freqs = &this->freqTable[module].list[0];
    size_t count = this->freqTable[module].count - 1;

    size_t i = 0;
    while (i < count)
    {
        if (maxHz > 0 && freqs[i] >= maxHz)
        {
            break;
        }

        if (inHz <= ((std::uint64_t)freqs[i] + freqs[i + 1]) / 2)
        {
            break;
        }

        i++;
    }

    return freqs[i];
}

bool ClockManager::ConfigIntervalTimeout(SysClkConfigValue intervalMsConfigValue, std::uint64_t ns, std::uint64_t *lastLogNs)
{
    std::uint64_t logInterval = this->GetConfig()->GetConfigValue(intervalMsConfigValue) * 1000000ULL;
    bool shouldLog = logInterval && ((ns - *lastLogNs) > logInterval);

    if (shouldLog)
    {
        *lastLogNs = ns;
    }

    return shouldLog;
}

void ClockManager::RefreshFreqTableRow(SysClkModule module)
{
    std::scoped_lock lock{this->contextMutex};

    std::uint32_t freqs[SYSCLK_FREQ_LIST_MAX];
    std::uint32_t count;

    FileUtils::LogLine("[mgr] %s freq list refresh", Board::GetModuleName(module, true));
    Board::GetFreqList(module, &freqs[0], SYSCLK_FREQ_LIST_MAX, &count);

    std::uint32_t *hz = &this->freqTable[module].list[0];
    this->freqTable[module].count = 0;
    for (std::uint32_t i = 0; i < count; i++)
    {
        if (!this->IsAssignableHz(module, freqs[i]))
        {
            continue;
        }

        *hz = freqs[i];
        FileUtils::LogLine("[mgr] %02u - %u - %u.%u MHz", this->freqTable[module].count, *hz, *hz / 1000000, *hz / 100000 - *hz / 1000000 * 10);

        this->freqTable[module].count++;
        hz++;
    }

    FileUtils::LogLine("[mgr] count = %u", this->freqTable[module].count);
}


bool prevBoostMode = true;

void ClockManager::Tick()
{
    std::scoped_lock lock{this->contextMutex};
    std::uint32_t mode = 0;
    AppletOperationMode opMode = appletGetOperationMode();
    Result rc = apmExtGetCurrentPerformanceConfiguration(&mode);
    ASSERT_RESULT_OK(rc, "apmExtGetCurrentPerformanceConfiguration");

    if(this->config->GetConfigValue(SysClkConfigValue_HandheldTDP) && opMode == AppletOperationMode_Handheld) {
        if(Board::GetConsoleType() == SysClkConsoleType_Hoag) {
            if(Board::GetPowerMw(SysClkPowerSensor_Now) < -(int)this->config->GetConfigValue(SysClkConfigValue_LiteTDPLimit)) {
                ResetToStockClocks();
                return;
            }
        } else {
            if(Board::GetPowerMw(SysClkPowerSensor_Now) < -(int)this->config->GetConfigValue(SysClkConfigValue_HandheldTDPLimit)) {
                ResetToStockClocks();
                return;
            }
        }
    }

    if(((tmp451TempSoc() / 1000) > (int)this->config->GetConfigValue(SysClkConfigValue_ThermalThrottleThreshold)) && this->config->GetConfigValue(SysClkConfigValue_ThermalThrottle)) {
        ResetToStockClocks();
        return;
    }
    bool isBoost = apmExtIsBoostMode(mode);

    prevBoostMode = isBoost;

    if (this->RefreshContext() || this->config->Refresh())
    {
        std::uint32_t targetHz = 0;
        std::uint32_t maxHz = 0;
        std::uint32_t nearestHz = 0;

        if(apmExtIsBoostMode(mode) && !this->config->GetConfigValue(SysClkConfigValue_OverwriteBoostMode)) {
            // ResetToStockClocks();
            return;
        }

        for (unsigned int module = 0; module < SysClkModule_EnumMax; module++)
        {
            targetHz = this->context->overrideFreqs[module];

            if (!targetHz)
            {
                targetHz = this->config->GetAutoClockHz(this->context->applicationId, (SysClkModule)module, this->context->profile, false);
                if(!targetHz)
                    targetHz = this->config->GetAutoClockHz(GLOBAL_PROFILE_ID, (SysClkModule)module, this->context->profile, false);
            }

            if (targetHz)
            {
                maxHz = this->GetMaxAllowedHz((SysClkModule)module, this->context->profile);
                nearestHz = this->GetNearestHz((SysClkModule)module, targetHz, maxHz);

                if (nearestHz != this->context->freqs[module]) {
                    FileUtils::LogLine(
                        "[mgr] %s clock set : %u.%u MHz (target = %u.%u MHz)",
                        Board::GetModuleName((SysClkModule)module, true),
                        nearestHz / 1000000, nearestHz / 100000 - nearestHz / 1000000 * 10,
                        targetHz / 1000000, targetHz / 100000 - targetHz / 1000000 * 10
                    );

                    Board::SetHz((SysClkModule)module, nearestHz);
                    this->context->freqs[module] = nearestHz;

                    if(module == SysClkModule_CPU) {
                        if(Board::GetSocType() == SysClkSocType_Mariko) {
                            Board::SetCpuUvLevel(this->config->GetConfigValue(SysClkConfigValue_MarikoCpuUvLow), this->config->GetConfigValue(SysClkConfigValue_MarikoCpuUvHigh), 1581000000);
                        } else {
                            Board::SetCpuUvLevel(this->config->GetConfigValue(SysClkConfigValue_EristaCpuUv), 0, 1581000000);
                        }
                    }
                }
            }
        }
    }
}

void ClockManager::ResetToStockClocks() {
    Board::ResetToStockCpu();
    if(Board::GetSocType() == SysClkSocType_Mariko) {
        Board::SetCpuUvLevel(this->config->GetConfigValue(SysClkConfigValue_MarikoCpuUvLow), this->config->GetConfigValue(SysClkConfigValue_MarikoCpuUvHigh), 1581000000);
    } else {
        Board::SetCpuUvLevel(this->config->GetConfigValue(SysClkConfigValue_EristaCpuUv), 0, 1581000000);
    }
    Board::ResetToStockGpu();
}

void ClockManager::WaitForNextTick()
{
    if(!(Board::GetHz(SysClkModule_MEM) < 665000000))
        svcSleepThread(this->GetConfig()->GetConfigValue(SysClkConfigValue_PollingIntervalMs) * 1000000ULL);
    else
        svcSleepThread(5000 * 1000000ULL); // 5 seconds in sleep mode
}

bool ClockManager::RefreshContext()
{
    bool hasChanged = false;
    
    std::uint32_t mode = 0;
    Result rc = apmExtGetCurrentPerformanceConfiguration(&mode);
    ASSERT_RESULT_OK(rc, "apmExtGetCurrentPerformanceConfiguration");

    std::uint64_t applicationId = ProcessManagement::GetCurrentApplicationId();
    if (applicationId != this->context->applicationId)
    {
        FileUtils::LogLine("[mgr] TitleID change: %016lX", applicationId);
        this->context->applicationId = applicationId;
        hasChanged = true;
    }

    SysClkProfile profile = Board::GetProfile();
    if (profile != this->context->profile)
    {
        FileUtils::LogLine("[mgr] Profile change: %s", Board::GetProfileName(profile, true));
        this->context->profile = profile;
        hasChanged = true;
    }

    // restore clocks to stock values on app or profile change
    if (hasChanged)
    {
        // this->rnxSync->ToggleSync(this->GetConfig()->GetConfigValue(SysClkConfigValue_SyncReverseNXMode));
        Board::ResetToStock();
        this->WaitForNextTick();
    }

    std::uint32_t hz = 0;
    for (unsigned int module = 0; module < SysClkModule_EnumMax; module++)
    {
        hz = Board::GetHz((SysClkModule)module);
        if (hz != 0 && hz != this->context->freqs[module])
        {
            FileUtils::LogLine("[mgr] %s clock change: %u.%u MHz", Board::GetModuleName((SysClkModule)module, true), hz / 1000000, hz / 100000 - hz / 1000000 * 10);
            this->context->freqs[module] = hz;
            hasChanged = true;
        }

        hz = this->GetConfig()->GetOverrideHz((SysClkModule)module);
        if (hz != this->context->overrideFreqs[module])
        {
            if (hz)
            {
                FileUtils::LogLine("[mgr] %s override change: %u.%u MHz", Board::GetModuleName((SysClkModule)module, true), hz / 1000000, hz / 100000 - hz / 1000000 * 10);
            }
            else
            {
                FileUtils::LogLine("[mgr] %s override disabled", Board::GetModuleName((SysClkModule)module, true));
                switch (module)
                {
                case SysClkModule_CPU:
                    if(!(apmExtIsBoostMode(mode) || (this->config->GetConfigValue(SysClkConfigValue_OverwriteBoostMode) && apmExtIsBoostMode(mode))))
                        Board::ResetToStockCpu();
                        if(Board::GetSocType() == SysClkSocType_Mariko) {
                            Board::SetCpuUvLevel(this->config->GetConfigValue(SysClkConfigValue_MarikoCpuUvLow), this->config->GetConfigValue(SysClkConfigValue_MarikoCpuUvHigh), 1581000000);
                        } else {
                            Board::SetCpuUvLevel(this->config->GetConfigValue(SysClkConfigValue_EristaCpuUv), 0, 1581000000);
                        }
                    break;
                case SysClkModule_GPU:
                    Board::ResetToStockGpu();
                    break;
                case SysClkModule_MEM:
                    Board::ResetToStockMem();
                    break;
                }
            }
            this->context->overrideFreqs[module] = hz;
            hasChanged = true;
        }
    }

    std::uint64_t ns = armTicksToNs(armGetSystemTick());

    // temperatures do not and should not force a refresh, hasChanged untouched
    std::uint32_t millis = 0;
    bool shouldLogTemp = this->ConfigIntervalTimeout(SysClkConfigValue_TempLogIntervalMs, ns, &this->lastTempLogNs);
    for (unsigned int sensor = 0; sensor < SysClkThermalSensor_EnumMax; sensor++)
    {
        millis = Board::GetTemperatureMilli((SysClkThermalSensor)sensor);
        if (shouldLogTemp)
        {
            FileUtils::LogLine("[mgr] %s temp: %u.%u °C", Board::GetThermalSensorName((SysClkThermalSensor)sensor, true), millis / 1000, (millis - millis / 1000 * 1000) / 100);
        }
        this->context->temps[sensor] = millis;
    }

    // power stats do not and should not force a refresh, hasChanged untouched
    std::int32_t mw = 0;
    bool shouldLogPower = this->ConfigIntervalTimeout(SysClkConfigValue_PowerLogIntervalMs, ns, &this->lastPowerLogNs);
    for (unsigned int sensor = 0; sensor < SysClkPowerSensor_EnumMax; sensor++)
    {
        mw = Board::GetPowerMw((SysClkPowerSensor)sensor);
        if (shouldLogPower)
        {
            FileUtils::LogLine("[mgr] Power %s: %d mW", Board::GetPowerSensorName((SysClkPowerSensor)sensor, false), mw);
        }
        this->context->power[sensor] = mw;
    }

    // real freqs do not and should not force a refresh, hasChanged untouched
    std::uint32_t realHz = 0;
    bool shouldLogFreq = this->ConfigIntervalTimeout(SysClkConfigValue_FreqLogIntervalMs, ns, &this->lastFreqLogNs);
    for (unsigned int module = 0; module < SysClkModule_EnumMax; module++)
    {
        realHz = Board::GetRealHz((SysClkModule)module);
        if (shouldLogFreq)
        {
            FileUtils::LogLine("[mgr] %s real freq: %u.%u MHz", Board::GetModuleName((SysClkModule)module, true), realHz / 1000000, realHz / 100000 - realHz / 1000000 * 10);
        }
        this->context->realFreqs[module] = realHz;
    }

    // ram load do not and should not force a refresh, hasChanged untouched
    for (unsigned int loadSource = 0; loadSource < SysClkPartLoad_EnumMax; loadSource++)
    {
        this->context->partLoad[loadSource] = Board::GetPartLoad((SysClkPartLoad)loadSource);
    }

    for (unsigned int voltageSource = 0; voltageSource < SysClkVoltage_EnumMax; voltageSource++)
    {
        this->context->voltages[voltageSource] = Board::GetVoltage((SysClkVoltage)voltageSource);
    }

    if (this->ConfigIntervalTimeout(SysClkConfigValue_CsvWriteIntervalMs, ns, &this->lastCsvWriteNs))
    {
        FileUtils::WriteContextToCsv(this->context);
    }
    return hasChanged;
}