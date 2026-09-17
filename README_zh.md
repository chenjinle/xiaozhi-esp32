# An MCP-based Chatbot

（中文 | [English](README.md) | [日本語](README_ja.md)）

## 介绍

👉 [人类：给 AI 装摄像头 vs AI：当场发现主人三天没洗头【bilibili】](https://www.bilibili.com/video/BV1bpjgzKEhd/)

👉 [手工打造你的 AI 女友，新手入门教程【bilibili】](https://www.bilibili.com/video/BV1XnmFYLEJN/)

小智 AI 聊天机器人作为一个语音交互入口，利用 Qwen / DeepSeek 等大模型的 AI 能力，通过 MCP 协议实现多端控制。

<img src="docs/mcp-based-graph.jpg" alt="通过MCP控制万物" width="320">

### 版本说明

当前 v2 版本与 v1 版本分区表不兼容，所以无法从 v1 版本通过 OTA 升级到 v2 版本。分区表说明参见 [partitions/v2/README.md](partitions/v2/README.md)。

使用 v1 版本的所有硬件，可以通过手动烧录固件来升级到 v2 版本。

v1 的稳定版本为 1.9.2，可以通过 `git checkout v1` 来切换到 v1 版本，该分支会持续维护到 2026 年 2 月。

### 已实现功能

- Wi-Fi / ML307 Cat.1 4G
- 离线语音唤醒 [ESP-SR](https://github.com/espressif/esp-sr)
- 支持两种通信协议（[Websocket](docs/websocket_zh.md) 或 MQTT+UDP）
- 采用 OPUS 音频编解码
- 基于流式 ASR + LLM + TTS 架构的语音交互
- 声纹识别，识别当前说话人的身份 [3D Speaker](https://github.com/modelscope/3D-Speaker)
- OLED / LCD 显示屏，支持表情显示
- 电量显示与电源管理
- 支持多语言（中文、英文、日文）
- 支持 ESP32-C3、ESP32-S3、ESP32-P4 芯片平台
- 通过设备端 MCP 实现设备控制（音量、灯光、电机、GPIO 等）
- 通过云端 MCP 扩展大模型能力（智能家居控制、PC桌面操作、知识搜索、邮件收发等）
- 自定义唤醒词、字体、表情与聊天背景，支持网页端在线修改 ([自定义Assets生成器](https://github.com/78/xiaozhi-assets-generator))

## 硬件

### 面包板手工制作实践

详见飞书文档教程：

👉 [《小智 AI 聊天机器人百科全书》](https://ccnphfhqs21z.feishu.cn/wiki/F5krwD16viZoF0kKkvDcrZNYnhb?from=from_copylink)

面包板效果图如下：

![面包板效果图](docs/v1/wiring2.jpg)

### 支持 70 多个开源硬件（仅展示部分）

- <a href="https://oshwhub.com/li-chuang-kai-fa-ban/li-chuang-shi-zhan-pai-esp32-s3-kai-fa-ban" target="_blank" title="立创·实战派 ESP32-S3 开发板">立创·实战派 ESP32-S3 开发板</a>
- <a href="https://github.com/espressif/esp-box" target="_blank" title="乐鑫 ESP32-S3-BOX3">乐鑫 ESP32-S3-BOX3</a>
- <a href="https://docs.m5stack.com/zh_CN/core/CoreS3" target="_blank" title="M5Stack CoreS3">M5Stack CoreS3</a>
- <a href="https://docs.m5stack.com/en/atom/Atomic%20Echo%20Base" target="_blank" title="AtomS3R + Echo Base">M5Stack AtomS3R + Echo Base</a>
- <a href="https://gf.bilibili.com/item/detail/1108782064" target="_blank" title="神奇按钮 2.4">神奇按钮 2.4</a>
- <a href="https://www.waveshare.net/shop/ESP32-S3-Touch-AMOLED-1.8.htm" target="_blank" title="微雪电子 ESP32-S3-Touch-AMOLED-1.8">微雪电子 ESP32-S3-Touch-AMOLED-1.8</a>
- <a href="https://github.com/Xinyuan-LilyGO/T-Circle-S3" target="_blank" title="LILYGO T-Circle-S3">LILYGO T-Circle-S3</a>
- <a href="https://oshwhub.com/tenclass01/xmini_c3" target="_blank" title="虾哥 Mini C3">虾哥 Mini C3</a>
- <a href="https://oshwhub.com/movecall/cuican-ai-pendant-lights-up-y" target="_blank" title="Movecall CuiCan ESP32S3">璀璨·AI 吊坠</a>
- <a href="https://github.com/WMnologo/xingzhi-ai" target="_blank" title="无名科技Nologo-星智-1.54">无名科技 Nologo-星智-1.54TFT</a>
- <a href="https://www.seeedstudio.com/SenseCAP-Watcher-W1-A-p-5979.html" target="_blank" title="SenseCAP Watcher">SenseCAP Watcher</a>
- <a href="https://www.bilibili.com/video/BV1BHJtz6E2S/" target="_blank" title="ESP-HI 超低成本机器狗">ESP-HI 超低成本机器狗</a>

<div style="display: flex; justify-content: space-between;">
  <a href="docs/v1/lichuang-s3.jpg" target="_blank" title="立创·实战派 ESP32-S3 开发板">
    <img src="docs/v1/lichuang-s3.jpg" width="240" />
  </a>
  <a href="docs/v1/espbox3.jpg" target="_blank" title="乐鑫 ESP32-S3-BOX3">
    <img src="docs/v1/espbox3.jpg" width="240" />
  </a>
  <a href="docs/v1/m5cores3.jpg" target="_blank" title="M5Stack CoreS3">
    <img src="docs/v1/m5cores3.jpg" width="240" />
  </a>
  <a href="docs/v1/atoms3r.jpg" target="_blank" title="AtomS3R + Echo Base">
    <img src="docs/v1/atoms3r.jpg" width="240" />
  </a>
  <a href="docs/v1/magiclick.jpg" target="_blank" title="神奇按钮 2.4">
    <img src="docs/v1/magiclick.jpg" width="240" />
  </a>
  <a href="docs/v1/waveshare.jpg" target="_blank" title="微雪电子 ESP32-S3-Touch-AMOLED-1.8">
    <img src="docs/v1/waveshare.jpg" width="240" />
  </a>
  <a href="docs/v1/lilygo-t-circle-s3.jpg" target="_blank" title="LILYGO T-Circle-S3">
    <img src="docs/v1/lilygo-t-circle-s3.jpg" width="240" />
  </a>
  <a href="docs/v1/xmini-c3.jpg" target="_blank" title="虾哥 Mini C3">
    <img src="docs/v1/xmini-c3.jpg" width="240" />
  </a>
  <a href="docs/v1/movecall-cuican-esp32s3.jpg" target="_blank" title="CuiCan">
    <img src="docs/v1/movecall-cuican-esp32s3.jpg" width="240" />
  </a>
  <a href="docs/v1/wmnologo_xingzhi_1.54.jpg" target="_blank" title="无名科技Nologo-星智-1.54">
    <img src="docs/v1/wmnologo_xingzhi_1.54.jpg" width="240" />
  </a>
  <a href="docs/v1/sensecap_watcher.jpg" target="_blank" title="SenseCAP Watcher">
    <img src="docs/v1/sensecap_watcher.jpg" width="240" />
  </a>
  <a href="docs/v1/esp-hi.jpg" target="_blank" title="ESP-HI 超低成本机器狗">
    <img src="docs/v1/esp-hi.jpg" width="240" />
  </a>
</div>

## 软件

### 固件烧录

新手第一次操作建议先不要搭建开发环境，直接使用免开发环境烧录的固件。

固件默认接入 [xiaozhi.me](https://xiaozhi.me) 官方服务器，个人用户注册账号可以免费使用 Qwen 实时模型。

👉 [新手烧录固件教程](https://ccnphfhqs21z.feishu.cn/wiki/Zpz4wXBtdimBrLk25WdcXzxcnNS)

### 烧录后使用（语音对话与 MCP 控制）

烧录完成后，语音对话与设备控制可以直接使用，无需额外配置：

1. 首次开机先配网（屏幕 / 声波 / BluFi 等方式，按板子提示操作）。
2. 在 [xiaozhi.me](https://xiaozhi.me) 注册账号并绑定设备（屏幕会显示设备 ID / 激活码）。
3. 绑定成功后即可直接语音对话，默认使用 Qwen 实时模型。

#### MCP 控制原理

设备通过 MCP（Model Context Protocol）把自身能力暴露给后台，大模型自动发现并调用设备工具：

1. 设备启动后通过 WebSocket / MQTT 连接后台，并在 `hello` 消息中声明支持 MCP。
2. 后台通过 `initialize` 初始化 MCP 会话，再通过 `tools/list` 获取设备支持的工具列表。
3. 当用户说出类似"打开继电器"的指令时，大模型理解意图后，通过 `tools/call` 下发调用请求。
4. 设备执行对应工具（如 GPIO 输出）并返回结果。

以语音控制继电器为例：

```text
你说："打开继电器"
→ 语音流上传后台，ASR 转成文字
→ 大模型理解意图，决定调用设备工具 self.relay.set
→ 后台下发 MCP 调用：tools/call { name: "self.relay.set", arguments: { "on": true } }
→ ESP32 拉高继电器 GPIO → 继电器吸合
→ 设备返回执行结果，大模型语音回复"已打开"
```

#### 设备端 MCP 工具

不同板子注册的工具不同，具体以板子代码中 `McpServer::AddTool` 注册为准。以 `yse-esp32s3-hmi` 板子为例，内置工具如下：

| 工具名 | 功能 | GPIO |
|--------|------|------|
| `self.relay.set` | 继电器开/关 | GPIO3 |
| `self.fan.set` | 风扇开/关 | GPIO7 |
| `self.light.set` | WS2812B 彩灯开/关/RGB（四颗灯一起亮） | GPIO8 |
| `self.light.set_chase` | WS2812B 彩灯走马灯动画（可调颜色/速度） | GPIO8 |
| `self.buzzer.set` | 蜂鸣器开/关 | GPIO9 |
| `self.sensor.read_temperature_humidity` | 读取 DHT11 温湿度 | GPIO4 |
| `self.peripherals.get_status` | 查询外设状态 | - |
| `self.camera.start_preview` | 开启 USB UVC 摄像头并在液晶上实时显示 | USB（GPIO19/20） |
| `self.camera.stop_preview` | 关闭摄像头监控画面 | USB |

所有板子还提供通用工具，如 `self.get_device_status`、`self.audio_speaker.set_volume`、`self.screen.set_brightness`、`self.camera.take_photo` 等。

小车电机已停用（引脚让给 4G 模块和传感器）。

**USB UVC 摄像头说明**：摄像头插板子 USB 口（GPIO19/20），开机默认不开启；说"打开摄像头/打开监控"才启动并显示画面，说"关闭摄像头"停止。摄像头开启期间 USB 串口/JTAG 不可用（同一物理口），需要烧录/看日志时拔掉摄像头并重启板子即可恢复。

**顶部信息栏**：屏幕顶部常驻显示 日期、时间、温湿度、天气。温湿度来自 DHT11（每 30 秒刷新）；天气来自心知天气 API（每 30 分钟刷新），需在 `config.h` 里把 `WEATHER_API_KEY` 填成你注册的心知天气 key，`WEATHER_CITY` 改成所在城市（拼音/城市名/城市 ID）。

#### 彩灯与传感器接线（yse-esp32s3-hmi）

WS2812B 彩灯接 GPIO8，灯带前四颗灯一起控制；`self.light.set_chase` 参数：`on`（开关）、`red/green/blue`（颜色，默认白色）、`speed_ms`（移动间隔，默认 120ms）。

DHT11 温湿度传感器数据脚接 **GPIO4**，VCC 接 3.3V，GND 共地，数据脚需 4.7k~10kΩ 上拉（多数模块自带）。语音指令示例："现在温度多少度" / "现在湿度是多少"。

PIR 人体感应模块（**模拟量输出**）接 **GPIO5**（ADC1_CH4）。固件后台任务用 ADC 持续采样，超过阈值后：自动点亮彩灯（暖白色，氛围灯）+ 触发语音问候"主人你好"；**人体离开超过 `PERIPHERAL_PIR_OFF_DELAY_MS`（默认 15000ms = 15 秒）后自动熄灭灯光**。阈值在 `config.h` 里配置：`PERIPHERAL_PIR_ADC_THRESHOLD`（默认 500，16 位标度，与 MicroPython `read_u16` 一致）和 `PERIPHERAL_PIR_ADC_RELEASE`（默认 100）。

- **灵敏度可语音/软件调节**：MCP 工具 `self.pir.set_sensitivity`（参数 threshold 0~65535，越小越灵敏），说"把人体感应调灵敏一点"即可
- **联动继电器（默认开启）**：PIR 检测到人时同步闭合继电器（GPIO3，可接智能镜背光），人离开延时后同步断开；如需关闭联动，把 `config.h` 里 `PERIPHERAL_PIR_CONTROL_RELAY` 改为 0
- **调阈值方法**：烧录后看串口日志每 5 秒打印的 `PIR ADC raw16=` 数值，把阈值设为"有人时数值和无人时数值之间的值"即可

MQ-2 烟雾传感器 **DO 输出**接 **GPIO18**（按实测：DO 高电平=超标，蓝灯亮=超标；报警极性可在 `config.h` 的 `PERIPHERAL_MQ2_DO_ACTIVE_HIGH` 修改，1=高电平报警，0=低电平报警）。固件后台任务检测到超标后：蜂鸣器长鸣 + 彩灯红色闪烁声光报警；浓度恢复正常后自动解除。未接模块时引脚固定为"不报警"电平，不会误报警。

#### 4G 双网络（Wi-Fi + ML307）

板子支持 Wi-Fi 与 ML307 Cat.1 4G 双网络，同一时间只启用一个：

- 默认 **Wi-Fi** 优先（可在代码 `yse_esp32s3_hmi.cc` 构造函数最后一个参数改默认值，0=Wi-Fi，1=4G）
- **开机启动时双击 BOOT 键**：在 Wi-Fi / 4G 之间切换，选择会保存到 NVS 并自动重启
- 不会两个同时连接，也不会自动回退；若选 4G 但模块未插卡/无信号，需手动双击 BOOT 切回 Wi-Fi

ML307 接线：

| ML307 模块 | ESP32-S3 引脚 |
|-----------|--------------|
| TX | GPIO16（ML307_RX_PIN） |
| RX | GPIO15（ML307_TX_PIN） |
| GND | GND |
| VCC | 按模块要求供电（一般 3.8~4.2V，勿直接用 5V） |

#### 模块引脚与工作方式总表（yse-esp32s3-hmi）

（摄像头、SD 卡与小车电机未启用，相关引脚已复用）

| 模块 | 引脚 | 类型 | 工作方式 |
|------|------|------|---------|
| 麦克风 | GPIO40（WS）/ GPIO42（SCK）/ GPIO41（DIN） | I2S 输入 | 采集语音，采样率 24kHz |
| 喇叭 | GPIO21（DOUT）/ GPIO47（BCLK）/ GPIO48（LRCK） | I2S 输出 | 播放 TTS 语音与提示音 |
| 屏幕 | GPIO6（CS，预留）/ GPIO13（MOSI）/ GPIO12（CLK）/ GPIO10（DC） | SPI | ST7796 480×320 显示（老款无 CS 屏不接 CS） |
| 屏幕 | GPIO11（RST）/ GPIO14（背光） | GPIO | 屏幕复位 / PWM 背光（MISO 不接） |
| 触摸 | GPIO1（SDA）/ GPIO2（SCL）/ GPIO43（RST）/ GPIO44（INT） | I2C + GPIO | FT5x06 触摸输入 |
| BOOT 键 | GPIO0 | 输入 | 单击=对话开关；开机双击=切换 Wi-Fi/4G |
| 继电器 | **GPIO3** | 输出，高电平吸合 | ① 语音 `self.relay.set` ② PIR 有人联动吸合、离开延时断开（默认开启） |
| 风扇 | GPIO7 | 输出，高电平开 | 语音 `self.fan.set` |
| RGB 彩灯 | GPIO8 | WS2812B×4（RMT） | ① 语音 `self.light.set` 开/关/变色 ② 语音 `self.light.set_chase` 走马灯 ③ PIR 有人→自动暖白 ④ 烟雾报警→红色闪烁 |
| 蜂鸣器 | GPIO9 | 输出，高电平响 | ① 语音 `self.buzzer.set` ② 烟雾超标→长鸣报警 |
| DHT11 温湿度 | GPIO4 | 单总线（需上拉 4.7k~10k） | 语音 `self.sensor.read_temperature_humidity` 查询温湿度 |
| PIR 人体感应 | GPIO5 | ADC1_CH4 模拟量输入 | 后台任务每 100ms 采样：超阈值→开灯+继电器+问候"主人你好"；离开 15 秒→自动关灯。灵敏度可语音调 `self.pir.set_sensitivity` |
| MQ-2 烟雾 | GPIO18 | 数字输入 DO，高电平=超标（蓝灯亮） | 后台任务检测：超标→蜂鸣器长鸣+红灯闪烁；恢复→自动解除。灵敏度由模块电位器调 |
| ML307 4G | GPIO15（TX）/ GPIO16（RX） | UART | Wi-Fi/4G 双网络，同一时间只用一个，开机双击 BOOT 切换 |
| USB | GPIO19 / GPIO20 | USB | 烧录与日志 |
| PSRAM | GPIO26~37 | 保留 | 八线 PSRAM，不可用 |
| 空闲 | GPIO17 / GPIO46 | - | GPIO17 为 ADC2 脚（数字用途可用，模拟量受 Wi-Fi 影响）；GPIO46 为 strapping 脚，仅上电采样 |
| 不可用 | GPIO38 / GPIO39 | - | 未引出/不可用 |
| 不推荐 | GPIO45 | - | VDD_SPI 电压选择脚 |

**自动运行的后台逻辑**（无需语音触发）：

1. **PIR 人体感应**：有人 → 彩灯暖白 + 继电器吸合 + 问候"主人你好"；人离开超过 `PERIPHERAL_PIR_OFF_DELAY_MS`（默认 15 秒）→ 自动熄灯、断开继电器
2. **MQ-2 烟雾报警**：DO 变高（超标，蓝灯亮）连续 300ms → 蜂鸣器长鸣 + 彩灯红色闪烁；DO 恢复低电平 → 自动解除报警
3. **传感器未接不影响语音对话**：PIR 未接读到 0（无人）、MQ-2 未接读到低（不超标）、DHT11 未接返回读取失败，均不会卡死或误触发

#### 使用前提

- 硬件接线：继电器等外设需接到板子对应的 GPIO（以板子 `config.h` 为准），并与 ESP32 共地。
- 后台需支持 MCP：官方 `xiaozhi.me` 服务器已支持，无需额外配置。
- 云端 MCP（智能家居、HomeAssistant 等）需在后台控制台单独添加 MCP 服务器配置。

### 开发环境

- Cursor 或 VSCode
- 安装 ESP-IDF 插件，选择 SDK 版本 5.4 或以上
- Linux 比 Windows 更好，编译速度快，也免去驱动问题的困扰
- 本项目使用 Google C++ 代码风格，提交代码时请确保符合规范

### 开发者文档

- [自定义开发板指南](docs/custom-board_zh.md) - 学习如何为小智 AI 创建自定义开发板
- [MCP 协议物联网控制用法说明](docs/mcp-usage_zh.md) - 了解如何通过 MCP 协议控制物联网设备
- [MCP 协议交互流程](docs/mcp-protocol_zh.md) - 设备端 MCP 协议的实现方式
- [MQTT + UDP 混合通信协议文档](docs/mqtt-udp_zh.md)
- [一份详细的 WebSocket 通信协议文档](docs/websocket_zh.md)

## 大模型配置

如果你已经拥有一个小智 AI 聊天机器人设备，并且已接入官方服务器，可以登录 [xiaozhi.me](https://xiaozhi.me) 控制台进行配置。

👉 [后台操作视频教程（旧版界面）](https://www.bilibili.com/video/BV1jUCUY2EKM/)

## 相关开源项目

在个人电脑上部署服务器，可以参考以下第三方开源的项目：

- [xinnan-tech/xiaozhi-esp32-server](https://github.com/xinnan-tech/xiaozhi-esp32-server) Python 服务器
- [joey-zhou/xiaozhi-esp32-server-java](https://github.com/joey-zhou/xiaozhi-esp32-server-java) Java 服务器
- [AnimeAIChat/xiaozhi-server-go](https://github.com/AnimeAIChat/xiaozhi-server-go) Golang 服务器
- [hackers365/xiaozhi-esp32-server-golang](https://github.com/hackers365/xiaozhi-esp32-server-golang) Golang 服务器

使用小智通信协议的第三方客户端项目：

- [huangjunsen0406/py-xiaozhi](https://github.com/huangjunsen0406/py-xiaozhi) Python 客户端
- [TOM88812/xiaozhi-android-client](https://github.com/TOM88812/xiaozhi-android-client) Android 客户端
- [100askTeam/xiaozhi-linux](http://github.com/100askTeam/xiaozhi-linux) 百问科技提供的 Linux 客户端
- [78/xiaozhi-sf32](https://github.com/78/xiaozhi-sf32) 思澈科技的蓝牙芯片固件
- [QuecPython/solution-xiaozhiAI](https://github.com/QuecPython/solution-xiaozhiAI) 移远提供的 QuecPython 固件

## 关于项目

这是一个由虾哥开源的 ESP32 项目，以 MIT 许可证发布，允许任何人免费使用，修改或用于商业用途。

我们希望通过这个项目，能够帮助大家了解 AI 硬件开发，将当下飞速发展的大语言模型应用到实际的硬件设备中。

如果你有任何想法或建议，请随时提出 Issues 或加入 [Discord](https://discord.gg/C759fGMBcZ) 或 QQ 群：1011329060

## Star History

<a href="https://star-history.com/#78/xiaozhi-esp32&Date">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/svg?repos=78/xiaozhi-esp32&type=Date&theme=dark" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/svg?repos=78/xiaozhi-esp32&type=Date" />
   <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=78/xiaozhi-esp32&type=Date" />
 </picture>
</a>
