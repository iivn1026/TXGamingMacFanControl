# Upstream comparison and licensing / 上游比较与许可

Compared against glassecho/FanControl-macOS v1.5.0, commit fa86682ceac9f98683d00bb1b6f0bc7ddd0abb07.

| Area | FanControl-macOS v1.5.0 | TXGamingMacFanControl 0.4.0 |
| --- | --- | --- |
| Target | Desktop Nuvoton NCT679x/NCT6779D SuperIO | ASUS TX Gaming FX608LM laptop |
| Control path | Nuvoton/SMCSuperIO and gpufan components | New ASUS ATKD WMI bridge, firmware profiles |
| Interface | Upstream GUI | New SwiftUI dashboard |
| Modes | Includes manual/curve capabilities | Silent, standard, performance firmware profiles |
| Monitoring source | helper/fanctl.c | Derived statusreader.c, dispatcher restricted to read commands |
| Background control | gpufand-related workflow | No startup daemon |
| Graphics | Upstream assets | Independent monochrome icon |
| Validation | Upstream's own supported platforms | One FX608LM / Sequoia 15.8.1 machine |

The helper is a substantial derivative of upstream C code, not an independently rewritten monitoring implementation. Its original write routines remain in source but are inaccessible through the public dispatcher. No upstream SwiftUI files, Nuvoton kext, gpufand executable or icon are bundled.

## License obligations

The upstream [BSD-3-Clause license](https://github.com/glassecho/FanControl-macOS/blob/v1.5.0/LICENSE) explicitly allows modified source and binary redistribution subject to its conditions:

- Source distributions retain copyright, conditions and disclaimer.
- Binary distributions reproduce those notices in documentation or accompanying materials.
- Author/contributor names cannot imply endorsement without permission.

The exact upstream license is included as LICENSE-FanControl.txt, attribution as NOTICE.txt; both are bundled inside the application and DMG. New implementation code uses the separate LICENSE. A source link and clear attribution are provided; BSD-3-Clause does not itself require publishing as a GitHub fork or using the original project name.

These provisions provide a basis for redistribution of the covered code when respected. This comparison is not a blanket guarantee about all intellectual property claims or third-party material. New icon and interface avoid copying upstream visual assets. This is an independent adaptation, not an official upstream release.

## 简体中文

上游面向桌面 Nuvoton SuperIO，本项目面向已测试的华硕 FX608LM，通过新增 WMI 桥接切换三种固件模式。监控 C 程序属于上游代码的实质性改编，不能宣称全部原创；原有写入函数仍在源码内，但命令入口仅允许读取。未打包上游界面、Nuvoton 驱动、gpufand 或图标。

BSD-3-Clause 允许满足条件的修改及再分发。源码必须保留版权、全部条款和免责说明；二进制需在随附材料中包含这些内容；不能未经许可借作者名称宣称背书。以上材料均随项目、应用和 DMG 分发，并注明来源。许可证本身不强制使用原项目名或以 fork 形式发布。

在遵守这些条款的范围内，可以分发所覆盖的代码；这不等于对所有潜在知识产权问题作绝对保证。项目链接和清楚的来源说明已保留。
