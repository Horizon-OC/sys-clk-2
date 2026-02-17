# sys-clk-pro

Switch sysmodule allowing you to set cpu/gpu/mem clocks according to the running application and docked state.

Support is only provided for FW 16.0.0+. This MAY work on older firmwares but support is NOT guaranteed

features over stock sys-clk
- built with libultrahand
- voltage readings via rgltr
- reading of battery temparature
- reading of FAN speed
- settings menu for new settings
- extra safety features
- cool logo
- CPU/GPU load driver
- speedo/iddq reading
- mitigate pcv cldvfs bug
- slight powersave in sleep mode
- Undervolting

## Clock table (MHz)

### MEM clocks
* 2133 → Mariko Max (4266 Modules)
* 1996 → JEDEC.
* 1866 → Mariko Max (3733 Modules)
* 1600 → official docked, boost mode, Erista Max
* 1331 → official handheld
* 1065
* 800
* 665

### CPU clocks
* 1963 → mariko max clock
* 1887
* 1785 → erista max clock, boost mode
* 1683
* 1581
* 1428
* 1326
* 1224 → sdev oc
* 1122
* 1020 → official docked & handheld
* 918
* 816
* 714
* 612 → sleep mode

### GPU clocks
* 1075 → mariko max clock
* 998
* 921 → erista max clock
* 844
* 768 → official docked
* 691
* 614
* 537
* 460 → max handheld
* 384 → official handheld
* 307 → official handheld
* 230
* 153
* 76 → boost mode

**Notes:**
1. GPU overclock is capped at 460MHz in handheld and capped at 768MHz if charging, unless you're using the official charger.
2. Clocks higher than 768MHz need the official charger is plugged in.
