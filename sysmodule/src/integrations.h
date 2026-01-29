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
 

#pragma once
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <nxExt.h>
#include <sysclk.h>
#include <switch.h>
#include "errors.h"
#include "file_utils.h"

#include "clock_manager.h"

class ReverseNXSync {
public:
    ReverseNXSync ();

    void ToggleSync(bool enable)    { m_sync_enabled = enable; };
    void Reset(uint64_t app_id)     { m_app_id = app_id; SetRTMode(ReverseNX_NotFound); GetToolMode(); }
    ReverseNXMode GetRTMode()           { return m_rt_mode; };
    void SetRTMode(ReverseNXMode mode)  { m_rt_mode = mode; };
    ReverseNXMode GetToolMode()         { return m_tool_mode = RecheckToolMode(); };
    SysClkProfile GetProfile(SysClkProfile real);
    ReverseNXMode GetMode();

protected:
    std::atomic<ReverseNXMode> m_rt_mode;
    ReverseNXMode m_tool_mode;
    uint64_t m_app_id = 0;
    bool m_tool_enabled;
    bool m_sync_enabled;

    ReverseNXMode GetToolModeFromPatch(const char* patch_path);
    ReverseNXMode RecheckToolMode();
};