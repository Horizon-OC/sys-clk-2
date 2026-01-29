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
#include "base_menu_gui.h"
#include <unordered_map>
#include <string>
#include <vector>
#include "freq_choice_gui.h"
#include "value_choice_gui.h"

class MiscGui : public BaseMenuGui
{
public:
    MiscGui();
    ~MiscGui();
    void listUI() override;
    void refresh() override;
    
protected:
    SysClkConfigValueList* configList;
    std::map<SysClkConfigValue, tsl::elm::ListItem*> configButtons;
    std::map<SysClkConfigValue, ValueRange> configRanges;
    std::map<SysClkConfigValue, std::vector<NamedValue>> configNamedValues;
    std::map<SysClkConfigValue, tsl::elm::ToggleListItem*> configToggles;
    std::map<SysClkConfigValue, std::tuple<tsl::elm::TrackBar*, tsl::elm::ListItem*, std::vector<uint64_t>>> configTrackbars;
    
    void addConfigToggle(SysClkConfigValue configVal, const char* altName);
    void addConfigButton(SysClkConfigValue configVal, 
        const char* altName, 
        const ValueRange& range,
        const std::string& categoryName,
        const ValueThresholds* thresholds,
        const std::map<uint32_t, std::string>& labels = {},
        const std::vector<NamedValue>& namedValues = {},
        bool showDefaultValue = true);
    void addFreqButton(SysClkConfigValue configVal,
                            const char* altName,
                            SysClkModule module,
                            const std::map<uint32_t, std::string>& labels = {});
    void updateConfigToggles();
    
    tsl::elm::ToggleListItem* enabledToggle;
    u8 frameCounter = 60;
};