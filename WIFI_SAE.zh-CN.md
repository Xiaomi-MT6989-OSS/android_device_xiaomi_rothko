# rothko WPA3 SoftAP 原厂实现恢复（2026-10-27）

## 已证实的范围

当前源码 hostapd 下，同一台 iPhone 17 连接 WPA3-Personal 或 WPA2/WPA3-Personal 热点均失败。AP 反复接受 SAE Confirm，但客户端重新认证，最终关联超时。纯 WPA3 为 H2E=1，混合模式为 H2E=0，因此不能归因于仅 H2E 失败。WPA2 对照能完成关联和四次握手。

运行原厂 OS3.0.307.0 hostapd、配套 V1 标准/小米 AIDL 库和隔离的 VNDK34 libcrypto/libssl 后，两种安全模式均完成 SAE、关联和四次握手，用户确认 iPhone 能连接并上网。驱动、固件、framework、热点密码及 SELinux 策略没有作为对照变量改变；服务仍由 init 启动，UID/GID 1010、NET_ADMIN/NET_RAW、hal_wifi_hostapd_default 域，SELinux Enforcing。

这证实可通过恢复原厂 SoftAP 用户态实现解决现场失败；尚未拆分到 hostapd 中某个函数或加密库中的某一行。AKMP UNKNOWN 日志在成功的原厂实现中也存在，不能当作故障证据。网友报告的 STA 连接 WPA3 路由器失败没有本地测试条件，仍待验证，不能据热点通过宣称其已修复。

## 正式源码选择

- 仅把源码 hostapd 替换为原厂 `hostapd` prebuilt 模块（`prefer: true` 覆盖同名源码模块），安装路径仍为 `/vendor/bin/hw/hostapd`。
- 原厂 init 原字节保留，原厂 VINTF XML 由 DEVICE_MANIFEST_FILE 合并进 vendor manifest；hostapd 为 AIDL V1，运行对照已确认当前 framework 的版本门控可使用该接口。
- 标准 V1 和小米 V1 接口库使用独立 SONAME/模块名，避免与上游生成库冲突。
- 原厂 `libcrypto-rothko-wifi.so` / `libssl-rothko-wifi.so` 仅由该原厂依赖闭包引用，不替换公共 libcrypto/libssl；libssl 对 libcrypto 的引用也重定向到私有库。
- 来源、SHA256、SONAME 与 DT_NEEDED 重定向记录在 `stock-runtime-compat.json`，提取列表在 `proprietary-files.txt`。沿用现有 extract-utils fixup 机制。
- 保留源码 wpa_supplicant 及当前设备 Wi-Fi HAL、驱动、固件和配置。没有降低 PMF/SAE 要求，没有修改上游 Wi-Fi 仓库或 SELinux 策略。

原厂 hostapd SHA256：`a8f51e4714c4c674e25f59bb4212ce55ece75037eff4fd29fc45fca3f942df96`。

## 验证与边界

五个提取后 ELF 的 `.text` 和符号版本索引与原厂相同，DT_NEEDED / SONAME 按映射逐项核对通过；原厂 init 和 VINTF XML 完全一致。仅执行定向提取和离线检查，未运行 lunch、Soong 或编译。extract-utils 整体重生成暴露的无关既有差异未纳入本次变更，只保留新增的五个模块和两条文件复制规则。

运行对照使用临时内存文件系统、bind mount 和独立 240 秒回退 watchdog。首次从 /data 直接 bind 的预检受到 nosuid 域转换限制，自动恢复，未放宽策略；改用 tmpfs 后在正式域中成功测试。对照结束后已核验原二进制及目录内相机库哈希恢复，临时挂载、文件、watchdog 与主机采集器全部清理，adbd 已恢复非 root。临时工具不进入设备产品。

源码候选仍需下一次用户授权的统一构建完成 Soong/ELF/VINTF/镜像核验，再验证保留数据启动、WPA3/混合模式/WPA2 热点实际联网和重启回归。手机目前已回退到原有镜像的 hostapd，不是持久部署本次修复。STA 模式须获得 WPA3 路由器或外部测试证据后独立验收。

内部脱敏证据：`out/bringup/wpa3-20261027/stock-hostapd-attempt-02.log`、`runtime-stock-hostapd/runtime-proof-02.txt`、`runtime-stock-hostapd/restored-proof.txt`、`production-source-audit.json`。

## 统一构建验收补充

最终候选 `out/bringup/wpa3-20261027/build-06` 成功，114 个增量步骤，退出码 0，正常 VINTF/SELinux 检查通过；15 份冻结镜像的 AVB 链验证通过。直接读取最终 vendor EROFS 镜像核对原厂 daemon、四份私有库与 init 字节一致，合并 manifest 恰有一个 AIDL V1 IHostapd/default，且源码 supplicant 保留。尚未刷入验收，release_approved=false。

构建接入最终方式：清空 BOARD_HOSTAPD_DRIVER，使上游源码 hostapd 与其非更新 APEX 禁用；vendor 同名 prefer prebuilt 提供 hostapd。V1 XML 与 stock 原字节相同，由 configs/wifi/android.hardware.wifi.hostapd.xml 合并，不通过 PRODUCT_COPY_FILES 或另一个同路径 fragment 安装。提取清单中的原厂 XML 仅 EXTRACT_ONLY 留档。上游 manifest 的试验性 V1 修改已撤销，external/wpa_supplicant_8 最终内容无净变更。运行逻辑均来自设备树/vendor。

此前几轮失败属于构建集成：Android 16 禁止复制 VINTF、同路径重复安装、以及覆盖模块后未选入 manifest；这些均已在最终镜像中逐项排除。提取清单模块名和 manifest 留档标记已同步，避免重新提取复现早期接入方式。
