/* --------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <p-sam@d3vs.net>, <natinusala@gmail.com>, <m4x@m4xw.net>
 * wrote this file. As long as you retain this notice you can do whatever you
 * want with this stuff. If you meet any of us some day, and you think this
 * stuff is worth it, you can buy us a beer in return.  - The sys-clk authors
 * --------------------------------------------------------------------------
 */


#include "about_gui.h"
#include "../format.h"
#include <tesla.hpp>
#include <string>

tsl::elm::ListItem* SpeedoItem = NULL;
tsl::elm::ListItem* IddqItem = NULL;

AboutGui::AboutGui()
{
    memset(strings, 0, sizeof(strings));
}

AboutGui::~AboutGui()
{
}

void AboutGui::listUI()
{
    this->listElement->addItem(
        new tsl::elm::CategoryHeader("Speedo/IDDQ")
    );

    SpeedoItem =
        new tsl::elm::ListItem("Speedos:");
    this->listElement->addItem(SpeedoItem);

    IddqItem =
        new tsl::elm::ListItem("IDDQ:");
    this->listElement->addItem(IddqItem);

    this->listElement->addItem(
        new tsl::elm::CategoryHeader("Credits")
    );

    this->listElement->addItem(
        new tsl::elm::CategoryHeader("Developers")
    );
    this->listElement->addItem(
        new tsl::elm::ListItem("p-sam")
    );

    this->listElement->addItem(
        new tsl::elm::ListItem("m4xw")
    );

    this->listElement->addItem(
        new tsl::elm::ListItem("natinusala")
    );

    // ---- Special Thanks ----
    this->listElement->addItem(
        new tsl::elm::CategoryHeader("Special Thanks")
    );

    this->listElement->addItem(
        new tsl::elm::ListItem("ScriesM - Atmosphere CFW")
    );

    this->listElement->addItem(
        new tsl::elm::ListItem("KazushiMe - sys-clk-OC")
    );

    this->listElement->addItem(
        new tsl::elm::ListItem("hanai3bi - sys-clk-EOS")
    );

    this->listElement->addItem(
        new tsl::elm::ListItem("b0rd2death - Ultrahand")
    );

    this->listElement->addItem(
        new tsl::elm::ListItem("MasaGratoR - Status Monitor")
    );
}

void AboutGui::update()
{
    BaseMenuGui::update();
}

void AboutGui::refresh()
{
    BaseMenuGui::refresh();
    
    if (!this->context)
        return;
    // Format strings once per refresh
    sprintf(strings[0], "%u/%u/%u", this->context->speedos[SysClkSpeedo_CPU], this->context->speedos[SysClkSpeedo_GPU], this->context->speedos[SysClkSpeedo_SOC]);
    sprintf(strings[1], "%u/%u/%u", this->context->iddq[SysClkSpeedo_CPU], this->context->iddq[SysClkSpeedo_GPU], this->context->iddq[SysClkSpeedo_SOC]);
    SpeedoItem->setValue(strings[0]);
    IddqItem->setValue(strings[1]);
}