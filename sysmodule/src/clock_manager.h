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


#pragma once

#include <atomic>
#include <sysclk.h>
#include <switch.h>

#include "config.h"
#include "board.h"
#include <nxExt/cpp/lockable_mutex.h>
#include "integrations.h"
void governorThread(void*);

class ReverseNXSync;

class ClockManager
{
  public:
    static ClockManager* GetInstance();
    static void Initialize();
    static void Exit();


    ClockManager();
    virtual ~ClockManager();
    void FixCpuBug();

    SysClkContext GetCurrentContext();
    Config* GetConfig();
    void SetRunning(bool running);
    bool Running();
    void GetFreqList(SysClkModule module, std::uint32_t* list, std::uint32_t maxCount, std::uint32_t* outCount);
    void Tick();
    void ResetToStockClocks();
    void WaitForNextTick();
    void SetRNXRTMode(ReverseNXMode mode);
    void SetKipData();
    void GetKipData();
    static void GovernorThread(void* arg);
    void UpdateRamTimings();
    struct {
      std::uint32_t count;
      std::uint32_t list[SYSCLK_FREQ_LIST_MAX];
    } freqTable[SysClkModule_EnumMax];
    int GetSpeedoBracket (int speedo);
    unsigned int GetGpuVoltage (unsigned int freq, int speedo);
    void calculateGpuVmin(void);
  protected:
    bool IsAssignableHz(SysClkModule module, std::uint32_t hz);
    inline std::uint32_t GetMaxAllowedHz(SysClkModule module, SysClkProfile profile);
    std::uint32_t GetNearestHz(SysClkModule module, std::uint32_t inHz, std::uint32_t maxHz);
    bool ConfigIntervalTimeout(SysClkConfigValue intervalMsConfigValue, std::uint64_t ns, std::uint64_t* lastLogNs);
    void RefreshFreqTableRow(SysClkModule module);
    bool RefreshContext();

    static ClockManager *instance;

    std::atomic_bool running;
    LockableMutex contextMutex;
    Config* config;
    SysClkContext* context;
    std::uint64_t lastTempLogNs;
    std::uint64_t lastFreqLogNs;
    std::uint64_t lastPowerLogNs;
    std::uint64_t lastCsvWriteNs;
    ReverseNXSync *rnxSync;
};
