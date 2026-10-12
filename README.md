# TXGamingMacFanControl

<img src="assets/AppIcon.png" width="128" alt="TXGamingMacFanControl icon">

Fan telemetry and firmware profile control for ASUS TX Gaming FX608LM running macOS.

## Compatibility and verified behavior

| Item | Tested configuration |
| --- | --- |
| Laptop | ASUS TX Gaming FX608LM |
| CPU | Intel Core Ultra 7 255HX, 20 logical CPUs |
| Memory | 32 GB |
| OS | macOS Sequoia 15.8.1 |
| Architecture | Intel x86_64 |
| Bootloader | OpenCore 1.0.9 |
| Temperature source | Existing VirtualSMC/CPU temperature sensors |
| Fan interface | ASUS ATKD WMI and read-only FANM ACPI getter |

On 12 October 2026, CPU temperature, CPU/GPU fan RPM and silent (2), standard (0), performance (1) switching were verified on this machine. Each profile was confirmed by reading the actual firmware FANM field. A short performance test increased CPU/GPU fan speeds from approximately 2500/2700 to 2900/3100 RPM; the initial silent profile was restored.

Only this model/firmware combination has been tested. This is **not a universal driver for Intel ASUS laptops**. Long-term operation, sustained load and sleep/wake remain unverified. The Intel app requires macOS 13 or later; actual hardware validation was on 15.8.1.

## Download and install

Download the Intel DMG from [Releases](https://github.com/iivn1026/TXGamingMacFanControl/releases). Open it and drag **TXGamingMacFanControl.app** to **Applications**.

The DMG also includes `EFI Driver`, the installation guide and license notices. Copying the app alone does not install the OpenCore driver. Follow [INSTALL.md](INSTALL.md) to add the kext and SSDT to a compatible EFI. Keep a working backup boot device.

The app is ad-hoc signed, not Developer ID signed or notarized. If macOS blocks a trusted downloaded copy, use the per-app **Open Anyway** action in System Settings → Privacy & Security. Never disable Gatekeeper globally.

## Use

Open the app to view CPU temperature and CPU/GPU RPM. Select silent, standard or performance and authorize through the macOS administrator dialog. Firmware keeps automatic thermal fan control; these profiles may also change performance and power limits. Opening or refreshing the app does not switch profiles. Profile buttons are disabled if required firmware capabilities are unavailable.

There are no custom curves, fixed PWM controls, fan-stop commands, pump controls or automatic startup services.

For read-only diagnostics after building:

```sh
python3 collect-diagnostics.py
```

Diagnostic reports stay local and are not required for installation. Do not post a complete EFI or private machine identity data.

## Build

Install Apple's Command Line Tools and Python 3. Install `iasl` on PATH, or use the copy bundled with Hackintool. Run:

```sh
python3 build.py
python3 package.py
```

Outputs: the app, `TXGamingFanBridge.kext`, `SSDT-TXGFAN.aml`, and `dist/TXGamingMacFanControl-0.4.0-Intel.dmg`.

## Implementation

The x86_64 bridge uses a 20-byte WMNB parameter buffer and the tested standard endpoint `0x00120075`. DSTS capability/status value `0x00010003` is not treated as the active profile. `SSDT-TXGFAN` exposes read-only `ATKD.TGFM()` to return the existing firmware FANM field. Changes require administrator access, per-connection authorization, valid telemetry, a target whitelist and readback confirmation; failed confirmation attempts to restore the previous profile. Scalar responses are capped at 16.

## Upstream and license

The monitoring helper `src/statusreader.c` is derived from [glassecho/FanControl-macOS v1.5.0](https://github.com/glassecho/FanControl-macOS/releases/tag/v1.5.0), commit `fa86682ceac9f98683d00bb1b6f0bc7ddd0abb07`, copyright 2026 岁岁平安. Its dispatcher exposes read/sys/gpu/version only. The upstream write routines remain in the source but are unreachable through this helper's dispatcher.

The app, CLI broker, ASUS bridge and ACPI wrapper are new implementations. Source attribution and upstream BSD-3-Clause terms are retained in [NOTICE.txt](NOTICE.txt) and [LICENSE-FanControl.txt](LICENSE-FanControl.txt), and included in the app/DMG. New code uses [BSD-3-Clause](LICENSE). See [comparison and licensing notes](docs/COMPARISON.md). This project is independent and does not claim upstream endorsement.

API references: [Linux ASUS WMI](https://github.com/torvalds/linux/blob/master/drivers/platform/x86/asus-wmi.c), [G-Helper](https://github.com/seerge/g-helper), [AsusSMC](https://github.com/hieplpvip/AsusSMC).

---

## 简体中文

适用于 ASUS TX Gaming FX608LM 的 macOS 风扇监控与固件模式切换工具。已测试硬件：Core Ultra 7 255HX、20 个逻辑核心、32 GB 内存、macOS Sequoia 15.8.1、OpenCore 1.0.9。

### 已验证及限制

2026 年 10 月 12 日，本机 CPU 温度、CPU/GPU 风扇转速，以及安静（2）、标准（0）、性能（1）模式切换通过测试。每次切换都从真实 FANM 字段读回确认。短时性能测试中，转速约由 2500/2700 RPM 上升到 2900/3100 RPM，测试后已恢复安静模式。

仅验证上述机型及固件，**不能视为全部 Intel 华硕笔记本通用驱动**。长期运行、持续负载和睡眠唤醒尚未验证。应用最低系统版本为 macOS 13，硬件实测系统为 15.8.1。

### 安装与使用

从 [Releases](https://github.com/iivn1026/TXGamingMacFanControl/releases) 下载 Intel DMG，打开后把应用拖入“应用程序”。首次使用还需按 [INSTALL.md](INSTALL.md) 安装 EFI 中的桥接驱动和 SSDT；仅拖入应用不能完成驱动安装。

打开应用查看温度和转速，点击安静、标准或性能，在系统窗口中完成管理员授权。风扇仍由固件自动按温度调速；模式也可能改变功耗及性能。没有手动 PWM、停转、自定义曲线、水泵控制或新增开机自启。

应用采用临时签名，未进行 Apple 公证。可信下载若被系统拦截，可在“系统设置 → 隐私与安全性”针对该应用选择“仍要打开”，不要全局关闭 Gatekeeper。

### 来源与许可证

监控辅助程序改编自 [FanControl-macOS v1.5.0](https://github.com/glassecho/FanControl-macOS/releases/tag/v1.5.0)。上游版权声明、BSD-3-Clause 完整条款及免责说明已保留，并随源码、应用和 DMG 分发。界面、华硕桥接驱动、命令代理和 ACPI 包装表为新增实现。

BSD-3-Clause 允许在满足条款的情况下修改和再分发；必须保留版权、条款及免责说明，不能未经许可宣称原作者背书。README 的项目链接用于清楚说明来源。具体比较及许可范围见 [docs/COMPARISON.md](docs/COMPARISON.md)。
