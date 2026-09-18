# YSE ESP32-S3 HMI

Custom ESP32-S3 board profile for GC0308 camera, TF card, I2S microphone, NS4168 I2S amplifier, and ST7796 SPI touch LCD.

Most hardware-specific pins are centralized in `config.h`.

## 自定义待命 GIF 动图

设备进入待命状态后，会循环播放待命 GIF；进入聆听、思考、说话等交互状态时，
GIF 会停止并恢复原有的表情显示流程。交互结束、设备重新进入待命状态后，
待命 GIF 会重新开始播放。

### 1. 准备 GIF 文件

建议使用以下规格：

- 文件名：`idle.gif`
- 推荐尺寸：`128 x 128`，与当前表情显示区域一致
- 推荐帧率：8～15 FPS
- 推荐大小：500 KB 以内
- 格式：标准动态 GIF
- 背景：建议使用黑色或透明背景

尺寸或文件过大会增加 Flash 占用和 GIF 解码时的内存压力。

### 2. 放入板型目录

在当前板型目录下创建 `assets` 文件夹，并将文件放到：

```text
main/boards/yse-esp32s3-hmi/assets/idle.gif
```

不要直接覆盖 `managed_components` 中的 GIF，因为更新依赖组件时该文件可能被还原。

### 3. 修改 GIF 资源路径

打开项目根目录的 `main/CMakeLists.txt`，找到 `YSE_IDLE_GIF_SOURCE`，
将资源路径修改为：

```cmake
set(YSE_IDLE_GIF_SOURCE
    ${CMAKE_CURRENT_SOURCE_DIR}/boards/yse-esp32s3-hmi/assets/idle.gif)
```

构建时，该 GIF 会被复制并嵌入到固件中，不需要在程序运行时读取 TF 卡。

### 4. 重新编译和烧录

```bash
idf.py fullclean
idf.py build
idf.py flash
```

如果已经连接串口，也可以指定端口：

```bash
idf.py -p <串口> flash monitor
```

例如 Linux/WSL 环境中的串口可能是 `/dev/ttyUSB0`，Windows 中可能是
`COM5`。实际名称以设备管理器或 `idf.py` 检测结果为准。

### 5. 更换另一个 GIF

使用新的动图覆盖 `assets/idle.gif`，然后重新执行：

```bash
idf.py build
idf.py flash
```

如果构建系统没有检测到资源变化，可先执行 `idf.py fullclean` 再重新构建。
