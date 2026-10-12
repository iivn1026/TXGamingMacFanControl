# Installation / 安装说明

## English

### Application

Open the Intel DMG, then drag TXGamingMacFanControl.app to Applications. The app is ad-hoc signed and not notarized. Use macOS's per-app Open Anyway action only after verifying the download source.

### OpenCore driver — tested FX608LM firmware only

Ready-to-install files are in the repository’s `EFI Driver/` directory and in the DMG. For the repository copy, download **Code → Download ZIP** and extract it; a kext is a complete folder bundle, not just its Info.plist.

1. Back up the entire working EFI and keep a bootable recovery USB.
2. Mount the EFI partition used by your macOS OpenCore installation.
3. Copy `EFI Driver/TXGamingFanBridge.kext` to `EFI/OC/Kexts/`.
4. Copy `EFI Driver/SSDT-TXGFAN.aml` to `EFI/OC/ACPI/`.
5. Merge the individual `ACPI/Add` and `Kernel/Add` entries from `EFI Driver/OpenCore-fan-entries.plist` into your existing config. It is a fragment, **not a replacement config.plist**. Enable both entries. Avoid duplicates.
6. Validate with the ocvalidate version matching your OpenCore. Keep existing identity, security and boot settings. Reboot through the edited EFI.
7. Open the app. Confirm temperature/RPM and current mode before changing profiles. If the driver or expected firmware getter is absent, profile controls remain unavailable.

The SSDT assumes the exact tested firmware field `\_SB.PC00.LPCB.EC0.FANM` and `\_SB.ATKD` device. Other machines require ACPI and firmware interface verification; matching brand/CPU alone is insufficient. The package does not include VirtualSMC or its sensor plugins; existing CPU temperature sensors are required for temperature telemetry.

Mode changes ask for administrator authorization. Keep firmware thermal management active. Firmware readback verifies the requested mode; it does not guarantee every thermal or power behavior on untested firmware.

### Remove / recover

Boot the backup EFI if needed. Disable/remove only the two fan-related OpenCore entries and their kext/AML files, validate the config, and reboot. Remove the application from Applications. No startup daemon is installed.

## 简体中文

### 安装应用

打开 Intel DMG，把 TXGamingMacFanControl.app 拖入“应用程序”。应用为临时签名，未公证。确认下载来源后，必要时通过系统“隐私与安全性”中的“仍要打开”允许该应用。

### 安装 EFI 驱动（仅已测试的 FX608LM 固件）

编译好的文件已附在仓库 `EFI Driver/` 目录和 DMG 内。从仓库下载时选择 **Code → Download ZIP** 后解压；kext 是完整文件夹包，不能只下载 Info.plist。

1. 备份完整可用 EFI，并保留能够启动的备用 U 盘。
2. 挂载 macOS 实际使用的 OpenCore EFI 分区。
3. 将 `EFI Driver/TXGamingFanBridge.kext` 复制到 `EFI/OC/Kexts/`。
4. 将 `EFI Driver/SSDT-TXGFAN.aml` 复制到 `EFI/OC/ACPI/`。
5. 将 `EFI Driver/OpenCore-fan-entries.plist` 中的 ACPI/Add 与 Kernel/Add 条目合并到现有配置，均启用，避免重复。它是配置片段，**不能直接替换完整 config.plist**。
6. 用匹配 OpenCore 版本的 ocvalidate 检查，保留原有身份、安全及启动参数，从修改后的 EFI 重启。
7. 打开应用，先确认温度、转速及当前模式，再切换模式。

SSDT 依赖本机固件中的 `\_SB.PC00.LPCB.EC0.FANM` 和 `\_SB.ATKD`。其他机型必须单独检查 ACPI 和固件接口，仅品牌或 CPU 相同不能保证兼容。安装包不包含 VirtualSMC 及温度传感器插件，需要已有温度驱动。

卸载时移除或禁用这两个风扇相关配置条目及对应 kext/AML，检查配置后重启，再删除应用。无法启动时用备份 EFI 恢复。
