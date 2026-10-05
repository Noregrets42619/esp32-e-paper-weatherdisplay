# WT32-ETH01 适配记录

工程使用 WT32-ETH01 和 Waveshare 4.2inch e-Paper Module (G)，目标芯片为 ESP32。Wi-Fi 联网、NTP 校时、Open-Meteo 天气请求、中文四色显示和定时休眠均已在实物上跑通。

## 硬件与接线

| 墨水屏信号 | WT32-ETH01 GPIO | 板上标识 |
| --- | --- | --- |
| DIN / MOSI | GPIO14 | IO14 |
| CLK / SCK | GPIO17 | 扩展接口 TXD / TX2 |
| CS | GPIO4 | IO4 |
| DC | GPIO33 | 485_EN |
| RST | GPIO32 | CFG |
| BUSY | GPIO35 | IO35，仅输入 |
| VCC | — | 3V3 |
| GND | — | GND，共地 |

GPIO17 是扩展 TX2，不是 UART0 烧录口。烧录使用 USB 转 TTL，TX 接板卡烧录 RXD，RX 接烧录 TXD，使用 3.3V 逻辑电平并共地。

屏幕供电接 3V3。板卡的 5V 与 3V3 输入二选一，不能同时供入。

## G 型驱动

屏幕完整型号是 **4.2inch e-Paper Module (G)**，400 × 300，黑、白、黄、红四色。它与普通黑白 V1/V2 的协议不同，不能只看 Rev2.2 或 V2 标记选择驱动。

组件目录保留历史名称 `epd4in2b`，实际使用 `src/epd4in2g.c`。SPI2 工作在 Mode 0、100kHz；BUSY 低电平忙、高电平就绪；复位为高 200ms、低 2ms、高 200ms。整帧 30000 字节，每像素 2 位。

驱动参考 [Waveshare G 型 ESP32 示例](https://github.com/waveshareteam/e-Paper/blob/master/E-paper_Separate_Program/4in2_e-Paper_G/ESP32/EPD_4in2g.cpp)。

## 开发与烧录

本机使用 VS Code Espressif IDF 扩展，ESP-IDF 路径为 `D:/ESP/espidf/v5.5/esp-idf`，实际版本为 `v5.5.4-299-ge46782886f`，工具目录为 `D:/ESP/espidf_tools`。

在 IDF 终端中执行：

```powershell
idf.py menuconfig
idf.py build
idf.py -p COMx flash
idf.py -p COMx monitor
```

首次配置时先执行 `idf.py set-target esp32`。`COMx` 换成 USB 转 TTL 的实际串口。没有自动下载电路时，IO0 接地后复位进入下载模式；烧录完成后断开 IO0 与 GND，再复位运行。

**烧录前在 `WiFi Configuration` 填写自己的 2.4GHz Wi-Fi 名称和密码。** 本地也可使用已被 Git 忽略的 `sdkconfig.wifi.local` 保存默认凭据；已有 `sdkconfig` 的值优先。

## 天气与显示

使用 Open-Meteo，默认地点为成都市郫都区郫筒一带，WGS84 坐标 30.80993、103.88253。接口时区为 `Asia/Shanghai`，显示时区为 `CST-8`。

请求当前温度、湿度、海平面气压、风速、风向、天气代码和昼夜状态，以及七天预报。风速由 m/s 换算为 km/h，降水栏显示当天最大小时降水概率。

屏幕使用中文字体，黄色绘制太阳和月亮，红色显示预报最高温。今日最高降水概率达到 50% 时提示栏变红。

默认北京时间 07:00–22:50 每十分钟更新，夜间等待到次日 07:00。天气获取失败时保留原画面。

## 手动刷新

常开、自复位轻触按键接在 **EN 与 GND** 之间。按下再松开，ESP32 复位，随后重新联网获取天气并刷新。当前启动流程即可完成这项操作。

IO0 只用于下载模式，正常运行时不要把它接地。

## 排查记录

- 编译成功但头文件标红：让 IntelliSense 使用 ESP32 GCC、`build/compile_commands.json` 和生成的 `sdkconfig.h`，然后重新加载窗口。
- 天气请求成功但屏幕空白：核对完整屏幕型号；早期误用黑白 V2 驱动，改成 G 型协议后正常显示。
- BUSY 超时：G 型按低电平忙、高电平就绪处理，刷新时等待 LOW→HIGH 周期。
- 电压测量：使用单屏诊断模式保持 ESP32 唤醒，在测量窗口内检查 VCC、RST 和 BUSY。

完整过程见 [复刻日志](https://noregrets42619.github.io/weather/porting-log/)，通用配置见 [README](README.md)。
