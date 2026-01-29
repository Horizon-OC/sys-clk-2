/* --------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <p-sam@d3vs.net>, <natinusala@gmail.com>, <m4x@m4xw.net>
 * wrote this file. As long as you retain this notice you can do whatever you
 * want with this stuff. If you meet any of us some day, and you think this
 * stuff is worth it, you can buy us a beer in return.  - The sys-clk authors
 * --------------------------------------------------------------------------
 */


#pragma once

#include "../../ipc.h"
#include "base_gui.h"

class BaseMenuGui : public BaseGui
{
    protected:

    public:
        // u8 dockedHighestAllowedRefreshRate = 60;
        SysClkContext* context;
        std::uint64_t lastContextUpdate;
        SysClkConfigValueList configList;
        bool g_hardwareModelCached = false;
        bool g_isMariko = false;
        bool g_isAula = false;
        bool g_isHoag = false;
        SetSysProductModel HWmodel = SetSysProductModel_Invalid;
        
        bool IsAula() {
            if (!g_hardwareModelCached) {
                setsysGetProductModel(&HWmodel);
                g_hardwareModelCached = true;
            }
            g_isAula = (HWmodel == SetSysProductModel_Aula);
            return g_isAula;
        }
        bool IsHoag() {
            if (!g_hardwareModelCached) {
                setsysGetProductModel(&HWmodel);
                g_hardwareModelCached = true;
            }
            g_isHoag = (HWmodel == SetSysProductModel_Hoag);
            return g_isHoag;
        }
        bool IsMariko() {
            if (!g_hardwareModelCached) {
                setsysGetProductModel(&HWmodel);
                g_hardwareModelCached = true;
            }
            g_isMariko = (HWmodel == SetSysProductModel_Iowa || 
            HWmodel == SetSysProductModel_Hoag || 
            HWmodel == SetSysProductModel_Calcio || 
            HWmodel == SetSysProductModel_Aula);

            return g_isMariko;
        }

        bool IsErista() {
            return !IsMariko();
        }
        BaseMenuGui();
        ~BaseMenuGui();
        void preDraw(tsl::gfx::Renderer* renderer) override;
        tsl::elm::List* listElement;
        tsl::elm::Element* baseUI() override;
        void refresh() override;
        virtual void listUI() = 0;

    private:
        char displayStrings[32][32];  // Pre-formatted display strings
        tsl::Color tempColors[7];     // Pre-computed temperature colors
};
