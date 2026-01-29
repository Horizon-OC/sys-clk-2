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


#include "app_profile_gui.h"

#include "../format.h"
#include "fatal_gui.h"
AppProfileGui::AppProfileGui(std::uint64_t applicationId, SysClkTitleProfileList* profileList)
{
    this->applicationId = applicationId;
    this->profileList = profileList;
}

AppProfileGui::~AppProfileGui()
{
    delete this->profileList;
}

void AppProfileGui::openFreqChoiceGui(tsl::elm::ListItem* listItem, SysClkProfile profile, SysClkModule module)
{
    std::uint32_t hzList[SYSCLK_FREQ_LIST_MAX];
    std::uint32_t hzCount;
    Result rc = sysclkIpcGetFreqList(module, &hzList[0], SYSCLK_FREQ_LIST_MAX, &hzCount);
    if(R_FAILED(rc))
    {
        FatalGui::openWithResultCode("sysclkIpcGetFreqList", rc);
        return;
    }

    tsl::changeTo<FreqChoiceGui>(this->profileList->mhzMap[profile][module] * 1000000, hzList, hzCount, module, [this, listItem, profile, module](std::uint32_t hz) {
        this->profileList->mhzMap[profile][module] = hz / 1000000;
        listItem->setValue(formatListFreqMHz(this->profileList->mhzMap[profile][module]));
        Result rc = sysclkIpcSetProfiles(this->applicationId, this->profileList);
        if(R_FAILED(rc))
        {
            FatalGui::openWithResultCode("sysclkIpcSetProfiles", rc);
            return false;
        }

        return true;
    }, true
    );
}

void AppProfileGui::openValueChoiceGui(
    tsl::elm::ListItem* listItem,
    std::uint32_t currentValue,
    const ValueRange& range,
    const std::string& categoryName,
    ValueChoiceListener listener,
    const ValueThresholds& thresholds,
    bool enableThresholds,
    const std::map<std::uint32_t, std::string>& labels,
    const std::vector<NamedValue>& namedValues,
    bool showDefaultValue
)
{
    tsl::changeTo<ValueChoiceGui>(
        currentValue,
        range,
        categoryName,
        listener,
        thresholds,
        enableThresholds,
        labels,
        namedValues,
        showDefaultValue
    );
}

void AppProfileGui::addModuleListItem(SysClkProfile profile, SysClkModule module)
{
    tsl::elm::ListItem* listItem = new tsl::elm::ListItem(sysclkFormatModule(module, true));
    listItem->setValue(formatListFreqMHz(this->profileList->mhzMap[profile][module]));
    listItem->setClickListener([this, listItem, profile, module](u64 keys) {
        if((keys & HidNpadButton_A) == HidNpadButton_A)
        {
            this->openFreqChoiceGui(listItem, profile, module);
            return true;
        }
        else if((keys & HidNpadButton_Y) == HidNpadButton_Y)
        {
            // Reset to "Default" (0 MHz)
            this->profileList->mhzMap[profile][module] = 0;
            listItem->setValue(formatListFreqMHz(0));
            
            Result rc = sysclkIpcSetProfiles(this->applicationId, this->profileList);
            if(R_FAILED(rc))
            {
                FatalGui::openWithResultCode("sysclkIpcSetProfiles", rc);
                return false;
            }
            return true;
        }
        return false;
    });
    this->listElement->addItem(listItem);
}

void AppProfileGui::addModuleListItemToggle(SysClkProfile profile, SysClkModule module)
{
    const char* moduleName = sysclkFormatModule(module, true);
    std::uint32_t currentValue = this->profileList->mhzMap[profile][module];
    
    tsl::elm::ToggleListItem* toggle = new tsl::elm::ToggleListItem(moduleName, currentValue != 0);
    
    toggle->setStateChangedListener([this, profile, module](bool state) {
        this->profileList->mhzMap[profile][module] = state ? 1 : 0;
        
        Result rc = sysclkIpcSetProfiles(this->applicationId, this->profileList);
        if(R_FAILED(rc))
        {
            FatalGui::openWithResultCode("sysclkIpcSetProfiles", rc);
        }
    });
    
    this->listElement->addItem(toggle);
}

void AppProfileGui::addModuleListItemValue(
    SysClkProfile profile,
    SysClkModule module,
    const std::string& categoryName,
    std::uint32_t min,
    std::uint32_t max,
    std::uint32_t step,
    const std::string& suffix,
    std::uint32_t divisor,
    int decimalPlaces,
    ValueThresholds thresholds
)
{
    tsl::elm::ListItem* listItem =
        new tsl::elm::ListItem(sysclkFormatModule(module, true));

    std::uint32_t storedValue = this->profileList->mhzMap[profile][module];
    if (storedValue == 0) {
        listItem->setValue(FREQ_DEFAULT_TEXT);
    } else {
        char buf[32];
        if (decimalPlaces > 0) {
            double displayValue = (double)storedValue / divisor;
            snprintf(buf, sizeof(buf), "%.*f%s", decimalPlaces, displayValue, suffix.c_str());
        } else {
            snprintf(buf, sizeof(buf), "%u%s", storedValue / divisor, suffix.c_str());
        }
        listItem->setValue(buf);
    }

    listItem->setClickListener(
        [this,
         listItem,
         profile,
         module,
         categoryName,
         min,
         max,
         step,
         suffix,
         divisor,
         decimalPlaces,
         thresholds](u64 keys)
        {
            if ((keys & HidNpadButton_A) == HidNpadButton_A)
            {
                std::uint32_t currentValue =
                    this->profileList->mhzMap[profile][module] * divisor;

                ValueRange range(
                    min,
                    max,
                    step,
                    suffix,
                    divisor,
                    decimalPlaces
                );

                this->openValueChoiceGui(
                    listItem,
                    currentValue,
                    range,
                    categoryName,

                    [this, listItem, profile, module, divisor, suffix, decimalPlaces, thresholds](std::uint32_t value) -> bool
                    {
                        this->profileList->mhzMap[profile][module] = value / divisor;

                        if (value == 0) {
                            listItem->setValue(FREQ_DEFAULT_TEXT);
                        } else {
                            char buf[32];
                            if (decimalPlaces > 0) {
                                double displayValue = (double)value / divisor;
                                snprintf(buf, sizeof(buf), "%.*f%s", 
                                        decimalPlaces, displayValue, suffix.c_str());
                            } else {
                                snprintf(buf, sizeof(buf), "%u%s", 
                                        value / divisor, suffix.c_str());
                            }
                            listItem->setValue(buf);
                        }

                        Result rc =
                            sysclkIpcSetProfiles(this->applicationId,
                                                 this->profileList);

                        if (R_FAILED(rc))
                        {
                            FatalGui::openWithResultCode(
                                "sysclkIpcSetProfiles", rc);
                            return false;
                        }
                        return true;
                    },

                    thresholds,
                    false
                );

                return true;
            }
            else if ((keys & HidNpadButton_Y) == HidNpadButton_Y)
            {
                this->profileList->mhzMap[profile][module] = 0;
                listItem->setValue(FREQ_DEFAULT_TEXT);

                Result rc =
                    sysclkIpcSetProfiles(this->applicationId,
                                         this->profileList);

                if (R_FAILED(rc))
                {
                    FatalGui::openWithResultCode("sysclkIpcSetProfiles", rc);
                    return false;
                }

                return true;
            }

            return false;
        });

    this->listElement->addItem(listItem);
}

void AppProfileGui::addProfileUI(SysClkProfile profile)
{
    Result rc = sysclkIpcGetConfigValues(&configList); // idk why this is needed, probably some refreshing issue
    if (R_FAILED(rc)) [[unlikely]] {
        FatalGui::openWithResultCode("sysclkIpcGetConfigValues", rc);
        return;
    }
    this->listElement->addItem(new tsl::elm::CategoryHeader(sysclkFormatProfile(profile, true) + std::string(" ") + ult::DIVIDER_SYMBOL + "  Reset"));
    this->addModuleListItem(profile, SysClkModule_CPU);
    this->addModuleListItem(profile, SysClkModule_GPU);
    this->addModuleListItem(profile, SysClkModule_MEM);
    #if IS_MINIMAL == 0
        ValueThresholds lcdThresholds(60, 65);
        if(!IsHoag() && configList.values[HorizonOCConfigValue_OverwriteRefreshRate]) {
            if(profile != SysClkProfile_Docked)
                this->addModuleListItemValue(profile, HorizonOCModule_Display, "Display", 40, configList.values[HorizonOCConfigValue_EnableUnsafeDisplayFreqs] ? 72 : 60, 1, " Hz", 1, 0, lcdThresholds);
            else
                this->addModuleListItemValue(profile, HorizonOCModule_Display, "Display", 50, 120, 5, " Hz", 1, 0);
        }
    #endif
    this->addModuleListItemToggle(profile, HorizonOCModule_Governor);
}

void AppProfileGui::listUI()
{
    this->addProfileUI(SysClkProfile_Docked);
    this->addProfileUI(SysClkProfile_Handheld);
    this->addProfileUI(SysClkProfile_HandheldCharging);
    this->addProfileUI(SysClkProfile_HandheldChargingOfficial);
    this->addProfileUI(SysClkProfile_HandheldChargingUSB);
}

void AppProfileGui::changeTo(std::uint64_t applicationId)
{
    SysClkTitleProfileList* profileList = new SysClkTitleProfileList;
    Result rc = sysclkIpcGetProfiles(applicationId, profileList);
    if(R_FAILED(rc))
    {
        delete profileList;
        FatalGui::openWithResultCode("sysclkIpcGetProfiles", rc);
        return;
    }

    tsl::changeTo<AppProfileGui>(applicationId, profileList);
}

void AppProfileGui::update()
{
    BaseMenuGui::update();

    if((this->context && this->applicationId != this->context->applicationId) &&  this->applicationId != SYSCLK_GLOBAL_PROFILE_TID)
    {
        tsl::changeTo<FatalGui>(
            "Application changed\n\n"
            "\n"
            "The running application changed\n\n"
            "while editing was going on.",
            ""
        );
    }
}
