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
 

#include "integrations.h"

ReverseNXSync::ReverseNXSync()
    : m_rt_mode(ReverseNX_NotFound), m_tool_mode(ReverseNX_NotFound) {
    FILE *fp = fopen("/atmosphere/contents/0000000000534C56/flags/boot2.flag", "r");
    m_tool_enabled = fp ? true : false;
    if (fp)
        fclose(fp);
}

SysClkProfile ReverseNXSync::GetProfile(SysClkProfile real) {
    switch (this->GetMode()) {
        case ReverseNX_Docked:
            return SysClkProfile_Docked;
        case ReverseNX_Handheld:
            if (real == SysClkProfile_Docked)
                return SysClkProfile_HandheldChargingOfficial;
        default:
            return real;
    }
}

ReverseNXMode ReverseNXSync::GetMode() {
    if (!this->m_sync_enabled)
        return ReverseNX_NotFound;
    if (this->m_rt_mode)
        return this->m_rt_mode;
    return this->m_tool_mode;
}

ReverseNXMode ReverseNXSync::GetToolModeFromPatch(const char* patch_path) {
    constexpr uint32_t DOCKED_MAGIC = 0x320003E0;
    constexpr uint32_t HANDHELD_MAGIC = 0x52A00000;
    FILE *fp = fopen(patch_path, "rb");
    if (fp) {
        uint32_t buf = 0;
        fread(&buf, sizeof(buf), 1, fp);
        fclose(fp);

        if (buf == DOCKED_MAGIC)
            return ReverseNX_Docked;
        if (buf == HANDHELD_MAGIC)
            return ReverseNX_Handheld;
    }

    return ReverseNX_NotFound;
}

ReverseNXMode ReverseNXSync::RecheckToolMode() {
    ReverseNXMode mode = ReverseNX_NotFound;
    if (this->m_tool_enabled) {
        const char* fileName = "_ZN2nn2oe18GetPerformanceModeEv.asm64"; // or _ZN2nn2oe18GetPerformanceModeEv.asm64
        const char* filePath = new char[72];
        SCOPE_EXIT { delete[] filePath; };
        /* Check per-game patch */
        snprintf((char*)filePath, 72, "/SaltySD/patches/%016lX/%s", this->m_app_id, fileName);
        mode = this->GetToolModeFromPatch(filePath);
        if (!mode) {
            /* Check global patch */
            snprintf((char*)filePath, 72, "/SaltySD/patches/%s", fileName);
            mode = this->GetToolModeFromPatch(filePath);
        }
    }

    return mode;
}