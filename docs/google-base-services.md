# Rothko Google 基础组件

## 这是什么

Rothko 的本地 LineageOS 24 候选复用国行 HyperOS `OS3.0.307.0.WNNCNXM` 的三份预签名 APK。用户可安装签名兼容的完整 Play 商店，并从商店选择其他应用。

| 包 | 安装位置 | APK SHA-256 |
| --- | --- | --- |
| `com.google.android.gms` | `/product/priv-app/GmsCore/GmsCore.apk` | `7e8da09f20d490af64f3432ffa0b82fd035546e54bc6c7b9ac6650edbe8533b3` |
| `com.google.android.gsf` | `/system_ext/priv-app/GoogleServicesFramework/GoogleServicesFramework.apk` | `927666b6a277af7066d092e56ca5ee18e0bd7f281b7149e4e5981eb2c1aa1868` |
| `com.android.vending` | `/product/priv-app/GooglePlayServicesUpdater/GooglePlayServicesUpdater.apk` | `d31ceb5db0cd3560cb71246f466ce286baffd14993100ce1e075561738812e9f` |

## 有什么用

预装的 `com.android.vending` 是 Google 签名的 headless 更新占位包，无商店启动入口。构建保持 OEM 签名、签名轮换链和分区布局。设备权限 XML 复用原厂对应分区对这三个包的条目，按原厂 feature 声明提供 `cn.google.services` 和 `com.google.android.feature.services_updater`。项目未实现 HyperOS 的联动启停策略，包启停遵循 Android 原有规则。证书兼容性与 Play Protect 认证是独立条件。

## 使用场景

需要保留 LineageOS 上游设置和初始化流程，同时允许用户自行安装完整 Play 商店。设备侧集成只选择三项核心包，完整商店及其他 Google 应用由用户选择。OEM 整体 Google RRO 涉及 WebView、定位、凭据提供方和其他系统角色，超出本候选范围。

## 如何使用

在 Android 源码的 Rothko 设备目录内，通过现有 extract-utils 从固定原厂分区 dump 提取 `Google base services (stock CN)` 段。精确 SHA-1 由 proprietary-files.txt 约束，配套私有 vendor 仓库基线为 `d9df5733126c4dc963497c16abc6a944faa59f8a`，候选为 `15fb53cb0e700357f3649420d7ced5cd9fb6e845`，树为 `1007c598f7636ad00fd5bb6176d31d44dee9ca1b`。保持该 vendor 提交与本设备配置配套。

```sh
PYTHONPATH=../../../tools/extract-utils python3 extract-files.py --no-cleanup \
  --section 'Google base services (stock CN)' /absolute/path/to/stock-dump
```

离线已通过三份签名、Soong 预签名 APK 检查、必需 uses-library、原始字节、安装路径及 Android 17 核心特权权限声明检查。用于对照的 [Android 17 MindTheGapps](https://github.com/MindTheGapps/17.0.0-arm64/releases/tag/MindTheGapps-17.0.0-arm64-20261022_132445) 中 `Phonesky.apk` SHA-256 为 `259bdd44ebaed8a0378b9773d5b838e9cb45383ae084458b3ec44e06a88c1039`。其包名与占位包相同，版本代码为 85180920，高于占位包的 82275402，签名证书 SHA-256 均为 `f0fd6c5b410f25cb25c3b53346c8972fae30f8ee7411df910480ad6b2d60db83`。对照 APK 的 Android 17 核心特权权限声明也已由原厂条目覆盖。这项比较用于验证 Android 更新的安装前提。

本次未构建、刷机或实测应用更新。验收时需确认首次启动、系统包扫描、完整版覆盖安装、Google 账号登录及下载，以及卸载商店更新后恢复到无界面占位包。APK 保存在本地配套 vendor 仓库。
