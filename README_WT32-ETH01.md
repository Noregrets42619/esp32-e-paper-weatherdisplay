# WT32-ETH01 使用说明

本项目已迁移至本机 VS Code 所配置的 ESP-IDF 环境，目标为 **ESP32**，联网方式为 **Wi-Fi**。2026-09-28 实机日志已确认 Wi-Fi、NTP 校时和天气请求成功。屏幕完整型号是 **Waveshare 4.2inch e-Paper Module (G)**（400 × 300，黑白黄红四色），2026-10-05 用户已确认 G 型诊断图显示正常，现已切回天气模式，完整天气画面待烧录验证。此前仅按 V2 标识选择黑白屏驱动不适用，其 BUSY 极性与 G 型相反。

组件目录保留历史名称 `epd4in2b`，实际驱动为 `src/epd4in2g.c`：BUSY 低电平忙、高电平就绪；复位高 200ms、低 2ms、高 200ms；每像素 2 位颜色编码；全刷新及 90 秒等待超时。天气画面直接绘制为黑、白、黄、红四色的 30000 字节缓冲区，诊断图显示四色色带。此固件不适用于普通黑白 V1/V2 或 (B) 三色屏。

单屏诊断开关位于 `menuconfig → WT32-ETH01 Configuration → Screen-only diagnostic (no Wi-Fi, SPI 100 kHz)`，本机及仓库默认均已关闭。正常模式使用原有 Wi-Fi 配置、郫都区 Open-Meteo 天气和深度休眠；SPI 保持已验证的 100kHz。新增日志分别报告 30000 字节图像传输耗时和屏幕 BUSY 刷新等待耗时。

## 屏幕接线

| 墨水屏信号 | WT32-ETH01 GPIO | 板上标识 / 说明 |
| --- | --- | --- |
| DIN / MOSI | GPIO14 | IO14 |
| CLK / SCK | GPIO17 | TXD / TX2（扩展接口，非烧录串口） |
| CS | GPIO4 | IO4 |
| DC | GPIO33 | 485_EN |
| RST | GPIO32 | CFG |
| BUSY | GPIO35 | IO35，只作输入 |
| VCC | 3V3 | 屏幕模块 3.3V 供电 |
| GND | GND | 共地 |

引脚定义在 `components/epd4in2b/include/epdif.h`。此方案避开以太网信号引脚及 ESP32 启动配置引脚 GPIO0/2/5/12/15。GPIO17 在 V1.4 原理图上连接串口 TX 指示灯，用作屏幕时钟时指示灯可能闪烁。不要把 GPIO17 接成烧录口 TXD（GPIO1）。

板卡自身的 5V 和 3V3 供电输入二选一，不能同时作为输入供电。以上接线按官方 V1.4 原理图和引脚说明核对，实物仍需检查丝印和版本。

## 可选 OTA 按键

GPIO4 已用作屏幕 CS。可选升级按键改到 **GPIO39**，默认关闭。

启用时，在 `idf.py menuconfig` → `WT32-ETH01 Configuration` 打开 `Enable OTA button`，按键接 GPIO39 与 GND，并在 GPIO39 与 3V3 之间接 **10kΩ 上拉电阻**。GPIO39 没有内部上拉。复位时按住按键触发升级；OTA URL 在 WiFi Configuration 中设置。OTA 流程本次未做实机验证。

## 开发环境和编译

本机路径直接沿用 VS Code 用户配置：

- ESP-IDF：`D:\ESP\espidf\v5.5\esp-idf`
- 工具目录：`D:\ESP\espidf_tools`
- 已安装的 Python 虚拟环境：`D:\ESP\espidf_tools\python_env\idf5.5_py3.11_env`
- 实际 IDF 源码版本：`v5.5.4-299-ge46782886f`

项目 `.vscode/settings.json` 已保存上述 IDF/工具路径，目标设为 `esp32`，本机串口配置为 COM8，烧录前需确认实际端口。用 VS Code 单独打开本项目目录，运行 **ESP-IDF: Open ESP-IDF Terminal**，然后执行：

```powershell
idf.py set-target esp32
idf.py menuconfig
idf.py build
```

本机已从 `ws2812-8x8-wifi-matrix` 工程 Git 保存的原版 `Train_led_wifi/main/blink_example_main.c` 导入 Wi-Fi 名称和密码，保存在 `sdkconfig.wifi.local` 中。该文件已被 Git 忽略，属于需保留的本地配置，不是缓存。CMake 在生成配置时自动加载它；原工程的当前修改没有被更改，密码没有写入公开源码或本说明。

如需换网，可通过 `idf.py menuconfig` → `WiFi Configuration` 修改当前生成的配置；若希望清理后重建仍使用新网络，同时更新 `sdkconfig.wifi.local`。已有 `sdkconfig` 中的值优先于默认配置。其他电脑没有此本地文件时，需要填写自己的 2.4GHz Wi-Fi 信息。

时区在 `WT32-ETH01 Configuration` 设置，默认 `CST-8`（UTC+8）。天气默认设为成都市郫都区郫筒一带（WGS84：30.80993°N，103.88253°E）。可在 `Open-Meteo Weather Configuration` 修改地点名称、经纬度和接口时区。屏幕使用中文四色界面，地点显示 `成都市郫都区`。不需要注册账号或填写 API Key。

烧录需自行确认串口，再执行 `idf.py -p COMx flash monitor`。本次没有执行烧录。

### 编译成功但编辑器显示头文件错误

`C/C++(1696)` 来自 VS Code 的 IntelliSense，不是 IDF 编译器。项目已提供 `.vscode/c_cpp_properties.json`，使用本机 ESP32 GCC 和 `build/compile_commands.json`，并配置 IDF、项目及生成配置头文件的查找路径。打开天气屏项目目录后，如红线仍未刷新，运行 `Developer: Reload Window`；必要时再运行 `C/C++: Reset IntelliSense Database`。

后续开发阶段保留构建文件，待用户明确要求时再统一清理。删除 `build` 后，需要重新构建或执行 `idf.py reconfigure`，才能恢复编译数据库及 `sdkconfig.h`。

默认按 4MB Flash 配置，使用 `partitions.csv` 中两个各 1.5MiB 的 OTA 应用分区，为后续天气接口适配预留空间。首次使用这份分区表时，需要完整烧录 bootloader、分区表和应用，不能只向旧分区 OTA 更新应用。

## 本次代码修改

- 重新分配屏幕引脚，并避免原 OTA 按键与 CS 冲突。
- 更新组件 CMake 声明，迁移至 `esp_event`、`esp_netif`、新版 SNTP、SPI2 及 HTTP 客户端。
- HTTPS 使用 IDF CA 证书包，保留证书校验；先进行 NTP 同步，再请求天气。
- 天气请求完成后再停止 Wi-Fi；失败时不刷新墨水屏。响应缓冲区、JSON 数组和字符串复制增加边界检查。
- 修复新版编译器报出的重复全局变量定义，以及绘图尺寸参数遮蔽全局变量的问题。
- 修复跨午夜休眠秒数计算，绘制 7 列预报，避免原来的第 8 列超出屏幕。

## Open-Meteo 天气接口

已移除旧 Dark Sky 组件和证书，改用 `components/weather` 中的 Open-Meteo 客户端。默认查询郫都区郫筒一带；坐标来自 Open-Meteo 地理编码接口的 `Pitong`（郫筒、四川、成都市）结果。它代表区内一个查询点，并不是整个行政区的平均天气。接口返回的网格中心坐标可能与请求点相差几公里。

获取当前温度、相对湿度、海平面气压、10 米风速与风向、天气代码、昼夜状态，以及七天最高/最低温和天气代码。风速请求单位固定为 m/s，屏幕显示时转换为 km/h；湿度和概率转换为原界面的 0–1 比例。Unix 时间通过北京时间转换一次，避免跨日偏移。

屏幕上的 `今日最高降水概率` 是**当天最大小时降水概率**，不是当前时刻的概率；接口缺失此值时显示 `暂无数据`。天气代码映射为中文说明和彩色绘图，包括独立的雷暴和冰雹图标。接口错误、响应过大或关键字段缺失时，保留墨水屏原画面，不显示伪造数据。顶部 `数据更新` 时间来自接口中的当前天气时间。

原刷新安排保持不变：北京时间 07:00–22:50 每 10 分钟请求一次，其他时段休眠；网络连接失败后约 3 小时重试。免费 API 适用于非商业用途，无需账号或密钥。商业用途需另行按官网方案办理。

屏幕标注 `数据：Open-Meteo.com`。天气数据来自 [Open-Meteo](https://open-meteo.com/)，按 [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) 提供；本项目对数据进行单位换算、整数舍入、中文文字和图标映射。

## 验证与清理

2026-10-05 已将天气界面改为原生 2bpp 四色缓冲区。黄色太阳和月亮、红色预报最高温、按降水概率变色的提示栏，配合中文地点、天气、星期和风向。中文 WeatherSans 字体为 Noto Sans SC 的字符子集，许可保存在 `main/fonts/OFL.txt`；补充自定义地点字形的操作见 README。已通过相同绘图代码的主机端预览、UTF-8 与边界检查，以及全部天气代码的中文映射检查。已使用本机 ESP-IDF 编译，固件大小 `0x106280` 字节，1.5MiB 应用分区剩余 32%；中文四色天气界面的实机显示待烧录验证。100kHz SPI 与已显示成功的 G 型初始化、刷新时序保持一致。

以下为早期验证记录。开发阶段构建目录继续保留，等待用户最终清理指令。

2026-09-23 使用上述本机 ESP-IDF `v5.5.4-299-ge46782886f` 环境完成 Open-Meteo 版本编译，并通过分区大小检查。固件大小 `0x10c9b0` 字节，1.5MiB 应用分区剩余约 30%。源码差异通过 `git diff --check` 检查。

本机对配置中的郫都坐标发送真实 HTTPS 请求成功，检查了七天数组长度、摄氏度和 m/s 单位、UTC+8、每日时间为本地零点及当前天气与首日日期一致；已核对生成配置中的地点、经纬度、时区和默认关闭 OTA 按键。此处验证的是实际响应格式和编译，尚未在 ESP32 上执行解析与显示。
本次未进行实机显示、联网和 OTA 测试。源码、VS Code 配置、默认配置、分区表和本说明属于需保留的项目文件。

已完成清理：先使用 ESP-IDF `fullclean` 清除构建产物，再逐项删除临时配置、脚本、日志、天气响应样本、下载的原理图及渲染图；上次遗留的环境导出临时文件也已删除。已确认项目 `.codex-build` 目录和该环境临时文件均不存在。源码和 VS Code 配置保留，再次编译会重新生成固件。

## 资料

- [WT32-ETH01 引脚说明](https://wiki.wireless-tag.com/docs/zh/WT32-ETH01/board_features.html)
- [官方规格书与 V1.4 原理图](https://wiki.wireless-tag.com/docs/zh/WT32-ETH01/board_resources.html)
- [Open-Meteo 接口文档](https://open-meteo.com/en/docs) / [额度与授权](https://open-meteo.com/en/pricing)
- [默认位置的地理编码查询](https://geocoding-api.open-meteo.com/v1/search?name=Pitong&count=5&language=zh&countryCode=CN&format=json)
