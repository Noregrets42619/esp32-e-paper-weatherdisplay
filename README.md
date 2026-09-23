# WT32-ETH01 墨水屏天气显示器

使用 **WT32-ETH01 + SPI 墨水屏**显示当前天气和七天预报，通过 **2.4GHz Wi-Fi** 连接 [Open-Meteo](https://open-meteo.com/) 获取天气数据。

> **首次使用前，请务必填写你自己的 Wi-Fi 名称和密码。** 仓库不包含可直接使用的网络凭据，默认的 `myssid` / `mypassword` 只是占位内容。

原工程：[henri98/esp32-e-paper-weatherdisplay](https://github.com/henri98/esp32-e-paper-weatherdisplay)。

## 功能与当前状态

- 显示温度、相对湿度、海平面气压、风速和风向。
- 显示七天最高温、最低温及天气图标。
- 区分晴天的白天与夜间图标；显示当天最大小时降水概率。
- 天气请求失败时保留原画面，完成刷新后进入深度睡眠。
- 默认地点为**四川省成都市郫都区**，使用北京时间，可自行修改。
- 已接入 Open-Meteo；个人非商业用途无需注册账号或申请 API Key。

目前采用 Wi-Fi 联网，**尚未实现 RJ45 有线联网**。已通过 ESP-IDF 5.5.4 环境编译，并在电脑上验证天气接口响应；屏幕显示、板卡联网和 OTA 仍需实机验证。

## 硬件准备

| 硬件 | 要求 |
| --- | --- |
| 主控板 | WT32-ETH01，项目目标为 `esp32`，默认按 4MB Flash 配置 |
| 墨水屏 | 与项目 `epd4in2b` 驱动兼容的 SPI 屏，分辨率 400 × 300 |
| USB 转串口模块 | 支持 3.3V UART 电平，用于烧录和查看日志 |
| 电源、连接线 | 稳定供电并共地 |
| OTA 按键 | 可选；启用时还需一个 10kΩ 上拉电阻 |

屏幕驱动沿用 4.2 英寸墨水屏方案。**相同尺寸不代表驱动兼容**，不同颜色版本、控制芯片和硬件版本需要核对，不能保证所有 4.2 英寸屏都能直接使用。

板卡资料：[功能与引脚说明](https://wiki.wireless-tag.com/docs/zh/WT32-ETH01/board_features.html) · [规格书与原理图](https://wiki.wireless-tag.com/docs/zh/WT32-ETH01/board_resources.html)

### 墨水屏接线

| 墨水屏信号 | WT32-ETH01 | 说明 |
| --- | --- | --- |
| DIN / MOSI | GPIO14 | IO14 |
| CLK / SCK | GPIO17 | 扩展接口 TXD / TX2 |
| CS | GPIO4 | IO4 |
| DC | GPIO33 | 485_EN |
| RST | GPIO32 | CFG |
| BUSY | GPIO35 | IO35，只作输入 |
| VCC | 3V3 | 屏幕模块 3.3V 供电 |
| GND | GND | 共地 |

注意接线时区分以下两点：

- **GPIO17 是扩展接口的 TX2，不是用于烧录的 UART0 TXD。** 它接有板载串口指示灯，用作屏幕时钟时指示灯可能闪烁。
- 板卡自身的 **5V 与 3V3 供电输入二选一**，不要同时从两个输入端供电。

需要调整屏幕引脚时，修改 [`components/epd4in2b/include/epdif.h`](components/epd4in2b/include/epdif.h)。

## 开发环境

本项目使用 **ESP-IDF 5.5 系列**，已验证的源码版本为 `v5.5.4-299-ge46782886f`，编译目标是 `esp32`。

可以使用 ESP-IDF 命令行环境，或 VS Code 的 Espressif IDF 扩展。仓库中的 `.vscode` 配置保留了开发机的 Windows 安装路径，**其他电脑使用前要改成自己的路径**：

- `.vscode/settings.json`：IDF 目录、工具目录和 Python 路径。
- `.vscode/c_cpp_properties.json`：ESP32 GCC 路径及对应工具链版本。

用 VS Code 打开本项目目录，在命令面板中运行 `ESP-IDF: Open ESP-IDF Terminal`。后续命令都在项目根目录、已激活的 IDF 环境下执行。

## 快速开始

### 1. 下载项目并选择芯片

```sh
git clone https://github.com/Noregrets42619/esp32-e-paper-weatherdisplay.git
cd esp32-e-paper-weatherdisplay
idf.py set-target esp32
```

`set-target` 主要用于首次配置或切换芯片，不必每次构建都执行。

### 2. 填写 Wi-Fi 名称和密码

```sh
idf.py menuconfig
```

进入 **WiFi Configuration**，修改并保存：

| 配置项 | 填写内容 |
| --- | --- |
| WiFi SSID | 你自己的 2.4GHz Wi-Fi 名称，注意大小写 |
| WiFi Password | 对应的 Wi-Fi 密码 |
| DNS Name | 可选，设备在网络中的名称 |

**不要跳过这一步，也不要直接使用默认占位值。** WT32-ETH01 使用的 ESP32 不支持连接 5GHz-only Wi-Fi。

如果希望重新生成配置后仍保留网络信息，可以在项目根目录自行创建 `sdkconfig.wifi.local`：

```ini
CONFIG_ESP_WIFI_SSID="YOUR_2_4GHZ_WIFI_NAME"
CONFIG_ESP_WIFI_PASSWORD="YOUR_WIFI_PASSWORD"
```

把示例内容换成自己的网络信息。CMake 会自动加载这个文件，它已被 `.gitignore` 排除，**不要强制提交真实密码**。已有 `sdkconfig` 中的配置优先于默认值；若已经生成过 `sdkconfig`，请同时通过 `menuconfig` 更新当前 Wi-Fi 配置。

### 3. 设置天气地点和时区

在 **Open-Meteo Weather Configuration** 中设置：

| 配置项 | 默认值 | 用途 |
| --- | --- | --- |
| Display place name | `Pidu, Chengdu` | 屏幕上的地点名称 |
| WGS84 latitude | `30.80993` | 纬度 |
| WGS84 longitude | `103.88253` | 经度 |
| Open-Meteo IANA timezone | `Asia/Shanghai` | 天气接口的时区 |

默认坐标代表郫都区郫筒一带。地点名称只用于显示，**实际查询位置由经纬度决定**。屏幕目前使用英文字体，建议用英文或拼音填写地点名称。

在 **WT32-ETH01 Configuration** 中，`POSIX timezone` 默认是 `CST-8`，代表北京时间 UTC+8。修改地区时，需要让这里的时区与天气接口时区保持一致。

### 4. 编译

```sh
idf.py build
```

成功后，应用固件位于 `build/e-paper-weatherdisplay.bin`。

项目采用两个各 **1.5MiB** 的 OTA 应用分区，配置见 [`partitions.csv`](partitions.csv)。第一次使用本项目或变更分区表后，应完整烧录 bootloader、分区表和应用，不能只通过旧固件的 OTA 更新应用。

### 5. 烧录并查看日志

USB 转串口模块与板卡的**烧录串口 UART0**连接：TX 接板卡 RXD，RX 接板卡 TXD，GND 共地，串口逻辑电平使用 3.3V。不要误接前面用于屏幕时钟的 GPIO17。

若没有自动下载电路，将 IO0 接 GND 后复位或重新上电，使芯片进入下载模式，再运行：

```sh
idf.py -p COMx flash
```

把 `COMx` 换成实际串口。烧录完成后断开 IO0 与 GND 的连接并复位，随后查看日志：

```sh
idf.py -p COMx monitor
```

退出串口监视器使用 `Ctrl+]`。具备正常自动下载、复位条件时，也可以使用 `idf.py -p COMx flash monitor`。

## 使用说明

设备启动后连接 Wi-Fi，同步时间，获取天气并刷新墨水屏，随后进入深度睡眠。

默认按北京时间 **07:00–22:50 每 10 分钟**安排一次更新，晚间等待到次日 07:00。首次上电也会尝试更新；Wi-Fi 连接失败时约 3 小时后重试。计划时间位于 `main/main.c` 的 `update_times` 数组。

- `Data`：接口返回的当前天气数据时间。
- `Precip today (max)`：当天最大小时降水概率，**不是当前时刻的降水概率**；缺失时显示 `N/A`。
- 风速以 km/h 显示，气压以 hPa 显示。
- 天气请求失败时不清除旧画面，可从串口日志查看原因。

### 可选 OTA 升级

默认关闭 OTA 按键。需要使用时：

1. 将按键接在 **GPIO39 与 GND** 之间，并从 GPIO39 接 **10kΩ 电阻到 3V3**。GPIO39 没有内部上拉。
2. 在 `WT32-ETH01 Configuration` 中启用 `Enable OTA button`。
3. 在 `WiFi Configuration` 中填写自己的 `OTA URL`，指向局域网服务器上的应用固件 `.bin`。
4. 在设备启动并完成 Wi-Fi 连接前保持按键按下，程序会尝试下载更新。

GPIO4 已用于屏幕 CS，不能再按原工程接作升级按键。OTA 沿用 HTTP 更新流程，尚未进行实机验证；初次烧录请使用串口。

## 常见问题

**编译成功，但 VS Code 中头文件出现红线？**

`C/C++(1696)` 一般来自编辑器 IntelliSense。检查 ESP32 GCC 路径，并确认 `build/compile_commands.json` 和 `build/config/sdkconfig.h` 存在。必要时运行 `idf.py reconfigure`，再执行 `Developer: Reload Window` 或 `C/C++: Reset IntelliSense Database`。开发期间保留构建目录，删除后需要重新生成。

**Wi-Fi 一直连接失败？**

检查是否已填写自己的 SSID 和密码、是否连接 2.4GHz 网络，以及当前 `sdkconfig` 是否仍使用旧值。再通过串口日志检查连接过程。

**天气请求失败或画面没有变化？**

先确认网络能访问 Open-Meteo，且时间同步成功；HTTPS 证书校验需要正确时间。接口失败时保留旧画面是预期行为。若数据请求成功但屏幕不刷新，再检查屏幕型号、接线、BUSY 信号和供电。

**为什么接了网线也不能获取天气？**

当前固件仅初始化 Wi-Fi，板卡具备 RJ45 接口不代表程序已经启用以太网。

## 数据来源与许可

天气数据来自 [Open-Meteo](https://open-meteo.com/)，使用 [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) 授权。本项目对数据进行了单位转换、舍入和图标映射，屏幕保留 `Weather: Open-Meteo.com` 来源标注。

免费接口适用于非商业用途；商业使用请查看 [Open-Meteo 服务方案](https://open-meteo.com/en/pricing)。接口字段说明见 [官方文档](https://open-meteo.com/en/docs)。

项目代码许可证见 [LICENSE](LICENSE)。图标源自 [Weather Icons](https://github.com/erikflowers/weather-icons)，字体源自 [Ubuntu Font](https://design.ubuntu.com/font)，相应资源遵循各自许可证。
