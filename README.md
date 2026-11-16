# ESP-IDF 分层嵌入式框架模板

一个把 **引脚 / 时序 / 器件 / 业务** 四件事分开的 ESP-IDF 工程骨架。

用来替代「所有代码都塞在 `main.c` 里、换块板子全改一遍」的写法。

当前示例跑在 **ESP32 + SSD1306 OLED + 电位器 + 舵机 + 按键** 上，实现了：

掉电保存的电位器校准 · OLED 亮度记忆 · WiFi 联网 · NTP 校时 · 天气获取 · 表情动画 · 数据面板切换。

---

## 为什么要有这个模板

自己写嵌入式，最常见的三个阶段：

| 阶段 | 代码长什么样 | 问题 |
|---|---|---|
| 1 | 一个 `main.c`，几百行，全在一个 `while(1)` 里 | 加第二个器件就开始乱 |
| 2 | 拆成 `led.c` / `oled.c` / `motor.c` | 每个 `.c` 里都在直接写 `GPIO22`、`400000`，换板子要全文搜 |
| 3 | **按「变化的原因」分层** | ← 这个模板 |

分层的判断标准不是「器件有几类」，是 **变化轴**：

- **换芯片** → 只有 `hal_layer` 要改
- **换器件型号** → 只有 `drv` 要改
- **换板子接线** → 只有 `bsp_config.h` 要改
- **换产品逻辑** → 只有 `app` 要改

---

## 分层

```
app        业务：谁在什么时候做什么（产品决策，不可复用）
 │
 ├─ svc    服务：可复用的零件（滤波、联网、NTP、天气）
 │
 └─ bsp    板级：这块板子接了什么、接在哪个脚、怎么初始化
     │
     └─ drv   驱动：把某个具体器件包装成接口（LED / 按键 / OLED / 舵机 / 电位器）
         │
         └─ hal_layer   硬件抽象：把芯片外设包装成接口（GPIO / ADC / I2C / PWM / NVS / WiFi）
```

```
app  →  bsp  →  drv  →  hal_layer
  ↘   svc  →  hal_layer
```

### 分层规则

| 层 | 允许做什么 | 禁止做什么 |
|---|---|---|
| **hal_layer** | 包 IDF 的驱动 API，只暴露芯片能力 | 头文件里出现 IDF 类型（`i2c_master_bus_handle_t` 这类） |
| **drv** | 处理器件差异（型号、参数、时序换算） | 直接写引脚号；自己决定「什么时候动」 |
| **bsp** | 集中所有引脚和器件参数，按顺序初始化 | 写业务逻辑；建任务 |
| **svc** | 跨项目可复用的纯逻辑 / 协议封装 | 依赖 `drv` / `bsp`；依赖某个产品的业务 |
| **middlewares** | 第三方或移植模块（如表情动画） | 直接读写引脚 |
| **app** | 决定业务节奏、任务划分、界面内容 | 直接操作 `hal_layer` 里的外设（唯一例外见下） |

**唯一的例外**：`app` 允许直接 `#include "nvs_hal.h"`。
因为 NVS 没有引脚、没有时序、没有寄存器，它是**共享设施**而不是「外设」。
其他 `hal` 头文件（`gpio_hal` / `adc_hal` / `pwm_hal` / `iic_hal`）一律不许进 `app`。

---

## 目录结构

```
00_basic/
├── CMakeLists.txt                  # 顶层，注册 middlewares 为额外组件目录
├── partitions.csv                  # 自定义分区表（nvs / phy_init / factory / vfs / storage）
├── sdkconfig.defaults              # 关键配置（可提交，sdkconfig 本身不提交）
├── main/
│   ├── main.c                      # 只有 app_init()，不写业务
│   └── idf_component.yml           # 组件依赖（u8g2 / cjson）
└── components/
    ├── hal_layer/
    │   ├── gpio_hal/               # 引脚电平、上下拉
    │   ├── intr_hal/               # GPIO 中断
    │   ├── adc_hal/                # 单次 / 连续 / DMA 采样
    │   ├── uart_hal/               # 串口
    │   ├── iic_hal/                # I2C 主机（读 / 写 / 读写寄存器 / 探测）
    │   ├── spi_hal/                # SPI 主机
    │   ├── pwm_hal/                # LEDC PWM（占空比用万分比）
    │   ├── nvs_hal/                # 掉电保存的键值配置存储
    │   └── wifi_hal/               # WiFi STA 连接与事件
    ├── drv/
    │   ├── led_drv/                # LED（极性、闪烁）
    │   ├── key_drv/                # 按键（自带 10ms 扫描任务：单击/双击/多击/长按）
    │   ├── oled_drv/               # SSD1306 / SH1106（I2C，u8g2 桥接，字体枚举、亮度、绘图）
    │   ├── pot_drv/                # 电位器（校准范围、千分比）
    │   └── servo_drv/              # 舵机（50Hz，平滑移动）
    ├── bsp/
    │   ├── bsp_config.h            # ★ 换板子只改这个文件
    │   ├── bsp.c                   # 按顺序初始化各驱动
    │   └── bsp.h
    ├── svc/
    │   ├── filter_lowpass_svc/     # 一阶低通滤波（纯计算，无硬件依赖）
    │   ├── wifi_svc/               # WiFi 连接管理 + 退避重连
    │   └── net_svc/                # NTP 校时 / 天气获取
    ├── middlewares/
    │   └── eye_expression/         # 表情动画（从 Arduino 移植）
    └── app/
        └── app.c                   # 任务划分 + 业务
```

---

## 已实现模块

### hal_layer

| 模块 | 内容 | 验证 |
|---|---|---|
| `gpio_hal` | 初始化 / 读 / 写 / 中断 | ✅ |
| `adc_hal` | 单次读取、连续采样、DMA | ✅ |
| `iic_hal` | 主机总线池、写 / 读 / 读寄存器 / 写寄存器 / 设备探测 | ✅ |
| `pwm_hal` | LEDC 定时器与通道、频率 / 占空比（万分比）运行时修改 | ✅ |
| `nvs_hal` | u8 / u16 / u32 / str 读写、按键删除、整体擦除、配置版本号 | ✅ |
| `wifi_hal` | STA 连接、状态机、断开原因码 | ✅ |
| `uart_hal` | 初始化 / 读 / 写 / 清缓冲 | ⚠️ 早期验证，当前示例未使用 |
| `spi_hal` | 主机总线、设备添加、收发 | ❌ **未接设备验证** |
| `intr_hal` | GPIO 中断 | ❌ **未使用**（驱动层目前用轮询） |

### drv

| 模块 | 内容 | 验证 |
|---|---|---|
| `oled_drv` | 型号枚举、字体枚举、行列 / 像素两套文字 API、画点线框圆、亮度、镜像 | ✅ |
| `servo_drv` | 角度 / 脉宽互转、平滑移动、上电中位缓冲归零 | ✅ |
| `pot_drv` | 三态（未初始化 / 未校准 / 可用）、校准范围校验、千分比换算 | ✅ |
| `key_drv` | 单击 / 双击 / 多击 / 长按，自带扫描任务 | ✅ |
| `led_drv` | 开关、翻转、闪烁 | ✅ 早期验证，当前注释掉 |

### svc

| 模块 | 内容 | 验证 |
|---|---|---|
| `filter_lowpass_svc` | 一阶低通（位移参数控制时间常数），可复位 | ✅ 用于电位器 |
| `wifi_svc` | 连接管理、状态回调、取 IP | ✅ |
| `net_time` | SNTP 校时，取年月日时分秒星期 | ✅ |
| `net_weather` | HTTP 查询天气 + JSON 解析 | ✅（日志验证） |

### middlewares

| 模块 | 内容 | 验证 |
|---|---|---|
| `eye_expression` | 表情动画（眨眼、注视、多种情绪），25fps | ✅ |

---

## 硬件

### 接线

| 器件 | 接口 | 引脚 | 参数 |
|---|---|---|---|
| OLED SSD1306 128×64 | I2C | SCL=22, SDA=21 | 400 kHz, addr 0x3C |
| 电位器 | ADC1 | GPIO35 | 12 bit |
| 舵机 | PWM | GPIO25 | 50 Hz, 500~2500 us |
| 按键 | GPIO | GPIO4 | 低电平有效，20ms 防抖 |
| LED ×3 | GPIO | 13 / 12 / 14 | 高电平点亮 |

### 引脚限制（ESP32）

- **GPIO6~11**：接 flash，绝对不能用
- **GPIO34~39**：只能输入，不能输出 PWM，没有内部上拉
- **ADC**：只能用 **ADC1（GPIO32~39）**，ADC2 与 WiFi 冲突

全部集中在 `components/bsp/bsp_config.h`。

---

## 快速开始

### 环境

- ESP-IDF **v6.1**
- 目标芯片 **esp32**（4MB flash）

### 步骤

```bash
# 1. 克隆
git clone https://github.com/klflfodj/esp-idf-basic-template.git
cd esp-idf-basic-template

# 2. 装组件依赖（u8g2 / cjson）
idf.py reconfigure

# 3. 改你的板子配置（引脚、器件参数）
#    编辑 components/bsp/bsp_config.h

# 4. 改 WiFi（当前硬编码在 components/app/app.c 的 svc_wifi_init 调用里）
#    ssid: 第一个参数
#    city: APP_WEATHER_CITY 宏

# 5. 编译烧录
idf.py build
idf.py -p COMx flash monitor
```

### 烧录后能做什么

| 操作 | 效果 |
|---|---|
| 单击 | 切换表情 |
| 双击 | 切换「旋钮控舵机 / 旋钮控亮度」 |
| 多击 | 切换「表情页 / 数据页」 |
| 长按 | 进入 / 结束电位器校准（转到底再按一次） |

校准值和亮度会存进 NVS，**下次上电不用重做**。

---

## 几个设计决定

### 1. 占空比用万分比，不用百分比

`hal_pwm_set_duty(pin, 5000)` = 50%。分辨率固定 1/10000，避免 0~100 精度不够、0~255 又不直观。

### 2. 电位器「未校准不可用」

`drv_pot_raw_to_permille()` 在没校准范围时直接返回 `false`，**不返回一个假的 0~1000 值**。
理由：假数据比报错更难查。

校准范围有下限校验（跨度 < 500 直接拒绝），防止「只转了一点点」存进 NVS。

### 3. 舵机上电先给中位，再平滑归零

上电时不知道舵机停在哪，直接写 0° 可能瞬间打满。
所以先给中位（90°），再用中位缓冲平滑移到 0°。

### 4. NVS 每次操作都 open → 操作 → commit → close

不持有长生命周期句柄，因此**天然线程安全、不会忘 commit**。
代价是慢一点，但配置写入是低频操作（校准 / 切模式），完全够用。

### 5. 所有 OLED 操作只在 `TaskOLED` 里做

其他任务要改亮度，只改 `s_brightness` 变量，由 OLED 任务统一写入。
原因见下方踩坑记录第 5 条。

---

## 踩坑记录

**1. I2C 的 SCL / SDA 接反**
现象是「init 成功，但探测不到设备」。看起来像两个独立问题，其实是一个。
**先量 SDA / SCL 对 GND 的电压**，比改代码快。

**2. 全局变量和局部变量同名（遮蔽）**
编译不报错，运行时反复重启。
`-Wshadow` 能提前发现，建议打开。

**3. 分区表里的名字必须小写 `nvs`**
写 `NVS` 会得到 `ESP_ERR_NOT_FOUND`。
这个错误信息看起来像「找不到数据」，实际是「找不到分区」。

**4. 新加 `.c` 文件后必须 `idf.py reconfigure`**
组件用的是 `SRC_DIRS`（目录级），CMake 只在 configure 时扫描。
不加文件不会报错，只是**那个文件不参与编译**，然后头文件全变成红线。

**5. 多个任务同时操作同一个 OLED 会花屏**
`oled_drv` 里有一个共享的发送缓冲，`u8g2` 攒字节用的是它。
`TaskPot`（高优先级）如果在 `TaskOLED` 传输中途抢占并调用了 OLED，
会把缓冲冲掉，导致数据错乱。
**解法：一条总线只归一个任务用。**

**6. 跨任务共享的变量要加 `volatile`**
编译器有权把「循环内没被修改的静态变量」的读取提到循环外。
症状是「变量明明在变，读出来是旧值」。换优化等级后突然坏。

**7. I2C 400kHz 传一帧 1024 字节约 25ms**
不是 delay，是物理传输时间。算帧率的时候要把它算进去。

**8. 不要每次 `idf.py fullclean`**
增量编译 20 秒，全量重编 3~5 分钟。
只有「构建系统状态坏了」才需要 clean。

---

## 这个模板适合谁

- 从 Arduino / STM32 转到 ESP-IDF，不熟悉组件系统的
- 项目开始变多器件、想理清楚代码组织方式的
- 想看看「分层到底怎么落在一堆 `.c` 文件上」的

**不适合**：想要一个可以直接出产品的完整工程。这里没有 OTA、没有错误码体系、没有单元测试。

---

## 已知不足

- `uart_hal` / `spi_hal` / `intr_hal` 没有接入示例
- 错误处理统一用 `bool`，没有错误码（无法区分「超时」和「NACK」）
- 没有看门狗、没有任务健康监控
- WiFi SSID 和城市硬编码在 `app.c`，应移到 NVS 或 `bsp_config.h`
- `iic_hal` 的设备表在多任务并发访问时没有加锁（当前靠「一条总线一个任务」规避）

---

## 许可

MIT License，见 [LICENSE](LICENSE)。

依赖的第三方组件：

| 组件 | 许可 |
|---|---|
| ESP-IDF | Apache-2.0 |
| [u8g2](https://github.com/olikraus/u8g2) | **GPL-2.0 或商业授权** |
| cJSON | MIT |

> ⚠️ `u8g2` 是 GPL-2.0。本仓库的 `.gitignore` 排除了 `managed_components/`，
> 所以**不包含也不分发** u8g2 的代码；使用本项目时请自行安装，并遵守 u8g2 的许可条款。
