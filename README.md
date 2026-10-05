# WT32-ETH01 四色墨水屏天气显示器

用 WT32-ETH01 驱动 Waveshare 4.2inch e-Paper Module (G)，通过 Wi-Fi 获取成都市郫都区的天气，显示当前温度和七天预报。界面使用中文，颜色为黑、白、黄、红。

原工程：[henri98/esp32-e-paper-weatherdisplay](https://github.com/henri98/esp32-e-paper-weatherdisplay)。

**首次烧录前，必须填写自己的 2.4GHz Wi-Fi 名称和密码。`myssid`、`mypassword` 是占位值。**

![中文四色天气界面](docs/weather-ui-preview.png)

## 硬件

| 部件 | 本项目使用的型号与配置 |
| --- | --- |
| 主控 | WT32-ETH01，ESP32，构建目标 `esp32`，工程按 4MB Flash 配置 |
| 屏幕 | Waveshare **4.2inch e-Paper Module (G)**，400 × 300，黑白黄红四色 |
| 联网 | ESP32 自带的 2.4GHz Wi-Fi；当前没有启用 RJ45 以太网 |
| 烧录 | USB 转 TTL，使用 3.3V 逻辑电平，连接板卡 UART0 |
| 供电 | 稳定的 5V 或 3.3V 电源；板卡两个电源输入二选一，屏幕当前接 3V3 |

屏幕型号要看完整名称里的 **(G)**。普通 4.2 英寸黑白 V1/V2、(B) 三色屏和 G 型的协议不同，板上 `Rev2.2` 或 `V2` 标记不能替代完整型号。本驱动只用于 G 型。

板卡说明：[WT32-ETH01 功能与引脚](https://wiki.wireless-tag.com/docs/zh/WT32-ETH01/board_features.html)。屏幕驱动参考 [Waveshare 官方 G 型 ESP32 示例](https://github.com/waveshareteam/e-Paper/blob/master/E-paper_Separate_Program/4in2_e-Paper_G/ESP32/EPD_4in2g.cpp)。

### 屏幕接线

| 屏幕信号 | GPIO | WT32-ETH01 板上位置 |
| --- | --- | --- |
| DIN / MOSI | 14 | IO14 |
| CLK / SCK | 17 | 扩展接口 TXD / TX2 |
| CS | 4 | IO4 |
| DC | 33 | 485_EN |
| RST | 32 | CFG |
| BUSY | 35 | IO35，仅输入 |
| VCC | — | 3V3 |
| GND | — | GND，共地 |

GPIO17 是扩展接口的 TX2，**不是烧录 UART0 的 TXD**。引脚定义位于 [`epdif.h`](components/epd4in2b/include/epdif.h)。

当前使用 SPI2、Mode 0、100kHz。G 型 BUSY **低电平忙、高电平就绪**，图像为每像素 2 位，整帧 30000 字节。组件目录沿用原工程的 `epd4in2b` 名称，实际驱动是 [`epd4in2g.c`](components/epd4in2b/src/epd4in2g.c)。

## 天气 API

使用 [Open-Meteo Forecast API](https://open-meteo.com/en/docs)，请求地址为 `https://api.open-meteo.com/v1/forecast`。个人非商业用途使用公开免费接口，**无需注册账号，也不需要 API Key**；商业使用和调用限额见 [官方服务方案](https://open-meteo.com/en/pricing)。

默认地点为成都市郫都区郫筒一带，WGS84 坐标为 **30.80993°N、103.88253°E**。地点名称只控制屏幕文字，实际查询位置由经纬度决定。

| 请求参数 | 内容 |
| --- | --- |
| `current` | `temperature_2m`、`relative_humidity_2m`、`pressure_msl`、`wind_speed_10m`、`wind_direction_10m`、`weather_code`、`is_day` |
| `daily` | `weather_code`、`temperature_2m_max`、`temperature_2m_min`、`precipitation_probability_max` |
| `forecast_days` | `7` |
| `timezone` | `Asia/Shanghai` |
| `timeformat` | `unixtime` |
| `temperature_unit` | `celsius` |
| `wind_speed_unit` | `ms`，绘图时换算成 km/h |

接口返回模型计算的当前天气与预报，不是开发板上温湿度传感器的读数，见 [Open-Meteo 当前天气说明](https://open-meteo.com/en/docs#current)。

“今日最高降水概率”取当天的 `precipitation_probability_max`，不是此刻的降水概率，也不是降水量；缺失时显示“暂无数据”。Unix 时间经 `CST-8` 转为北京时间，顶部“数据更新”使用接口返回的当前天气时间。

网络部分先通过 NTP 校时，再使用 HTTPS 请求天气，证书校验使用 ESP-IDF CA 证书包。请求失败或关键字段不完整时保留旧画面。

## 编译与烧录

工程使用 **ESP-IDF 5.5 系列**，本机验证版本为 `v5.5.4-299-ge46782886f`。VS Code 中用 Espressif IDF 扩展打开工程，并运行 `ESP-IDF: Open ESP-IDF Terminal`。

```sh
git clone https://github.com/Noregrets42619/esp32-e-paper-weatherdisplay.git
cd esp32-e-paper-weatherdisplay
idf.py set-target esp32
idf.py menuconfig
```

`set-target` 用于首次配置或切换芯片。仓库中的 `.vscode` 文件带有本机 Windows 路径，换电脑时需要重新配置 IDF、工具链和 Python 路径。

### 填写 Wi-Fi 与地点

在 `WiFi Configuration` 中填写 **WiFi SSID** 和 **WiFi Password**，使用自己的 2.4GHz 网络。

在 `Open-Meteo Weather Configuration` 中设置：

```ini
CONFIG_PLACE_NAME="成都市郫都区"
CONFIG_LATITUDE="30.80993"
CONFIG_LONGITUDE="103.88253"
CONFIG_WEATHER_TIMEZONE="Asia/Shanghai"
```

`WT32-ETH01 Configuration` 中的 `POSIX timezone` 默认为 `CST-8`。修改地区时，要同时调整接口与本地时区。

也可以在根目录创建本地默认配置 `sdkconfig.wifi.local`：

```ini
CONFIG_ESP_WIFI_SSID="YOUR_2_4GHZ_WIFI_NAME"
CONFIG_ESP_WIFI_PASSWORD="YOUR_WIFI_PASSWORD"
```

此文件和生成的 `sdkconfig` 已被 Git 忽略。已有 `sdkconfig` 的值优先于默认配置，换网时通过 `menuconfig` 修改当前配置；真实密码不要提交到公开仓库。

### 构建和写入

```sh
idf.py build
idf.py -p COMx flash
idf.py -p COMx monitor
```

把 `COMx` 换成实际串口。USB 转 TTL 的 TX 接板卡烧录 RXD，RX 接烧录 TXD，GND 共地。没有自动下载电路时，将 IO0 接 GND 后复位进入下载模式；烧录完成后断开 IO0 与 GND，再复位运行。退出监视器用 `Ctrl+]`。

固件生成在 `build/e-paper-weatherdisplay.bin`。第一次使用或更换分区表时，通过 `idf.py flash` 完整烧录 bootloader、分区表和应用。

## 运行与显示

上电后依次连接 Wi-Fi、校时、请求天气、停止 Wi-Fi、刷新屏幕，最后进入深度睡眠。

Wi-Fi 联网、天气获取、中文四色显示和定时休眠均已在实物上跑通。

- 黄色用于太阳和月亮，红色用于预报最高温，黑色用于最低温与正文。
- 当天最高降水概率达到 50% 时提示栏为红色，否则为黄色。
- 默认北京时间 **07:00–22:50 每 10 分钟**更新，夜间等待到次日 07:00。
- Wi-Fi 连接失败后约 3 小时重试。刷新计划在 `main/main.c` 的 `update_times` 数组中。

墨水屏没有背光，观察是否正常要看内容有没有变化。串口中的 `Image transfer` 是 SPI 发送图像耗时，`full refresh: ready HIGH after ... ms` 是屏幕刷新时等待 BUSY 的耗时。

### 手动刷新

使用一个常开、自复位的轻触按键，一端接板卡 **EN**，另一端接 **GND**。按下再松开，ESP32 复位，程序重新连接 Wi-Fi、获取天气并刷新屏幕；深度睡眠期间也可以这样操作。

这里使用的是 EN 复位功能，不需要增加 GPIO 按键程序。IO0 保持正常运行状态，不要把刷新按键接到 IO0。

### 单屏诊断

遇到空白屏时，在 `menuconfig → WT32-ETH01 Configuration` 打开 `Screen-only diagnostic (no Wi-Fi, SPI 100 kHz)`，重新编译烧录。

诊断模式跳过联网，显示文字和黑、白、黄、红色带，不进入 ESP32 深度睡眠。失败后保留 60 秒电压测量窗口，随后将 RST 拉低。诊断结束后关闭该选项，再次编译烧录恢复天气模式。

`BUSY remained ...` 或 `ESP_ERR_TIMEOUT` 表示初始化或刷新未完成，应检查完整屏幕型号、BUSY 极性、接线和供电。仅凭 Wi-Fi 连接和天气请求成功，还不能判断屏幕驱动正常。

### 中文字体与界面

中文字体已随源码提供，正常编译不需要额外下载。自定义地点出现 `?` 时，将缺失字符加入 `main/fonts/characters.txt`，使用 [Noto Sans SC 可变字体](https://github.com/google/fonts/tree/main/ofl/notosanssc) 重新生成：

```sh
python -m pip install Pillow
python tools/generate_weather_fonts.py --font path/to/NotoSansSC.ttf
idf.py build
```

布局位于 `main/weather_ui.c`，四色绘图在 `main/color_canvas.c`。`tools/preview_weather_ui.py` 可用本机 GCC 和 ESP-IDF 源码生成像素预览，具体参数见脚本说明。

## 记录与许可

- 复刻日志：[Ecoli ESP32 项目记录](https://noregrets42619.github.io/weather/porting-log/)。
- 本机适配记录：[README_WT32-ETH01.md](README_WT32-ETH01.md)。
- 原项目代码采用 [MIT License](LICENSE)，保留原作者许可。
- 天气数据来自 [Open-Meteo](https://open-meteo.com/)，按 [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/) 提供；界面保留来源文字。
- WeatherSans 位图字体由 Noto Sans SC 生成，许可见 [SIL OFL 1.1](main/fonts/OFL.txt)。诊断模式保留的 Ubuntu 字体、原工程资源分别遵循其原许可。
