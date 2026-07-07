[**English**](README.md)

# token-ticker

桌面 LLM 配额监控站 —— 独立供电，无需电脑中转。

基于 [Waveshare ESP32-S3-RLCD-4.2](https://www.waveshare.com/esp32-s3-rlcd-4.2.htm) 的独立硬件设备，直接通过 HTTPS 拉取 LLM 提供商的使用配额并显示在屏幕上。无需桌面中转、无需本地代理、无需云端依赖。

---

## 功能特性

- **4.2 寸反射式 LCD** — 阳光下可读，无需背光
- **实时配额拉取** — 直接从 LLM 提供商 API 获取使用数据
- **日期时间显示** — SNTP 时间同步 + RTC 回写
- **电池监控** — ADC 读取并显示剩余百分比
- **温湿度检测** — 板载 SHTC3 传感器
- **Wi-Fi 联网** — 通过 SD 卡配置配网
- **SD 卡配置** — JSON 格式，无需重新编译烧录
- **NVS 持久化** — 配置和凭据跨重启保存
- **按键交互** — 手动刷新与电源控制
- **休眠与唤醒** — 明确的电源管理策略，保障电池续航

### 已支持供应商

| 供应商 | 状态 |
|--------|------|
| MiniMax（中国站） | 硬件已验证 |
| 其他供应商 | 已规划（架构支持扩展） |

---

## 硬件要求

| 组件 | 说明 |
|------|------|
| **开发板** | [Waveshare ESP32-S3-RLCD-4.2](https://www.waveshare.com/esp32-s3-rlcd-4.2.htm)（SKU: 33298 / 33507） |
| **主控** | ESP32-S3-WROOM-1-N16R8（双核 LX7 @ 240 MHz，16 MB Flash，8 MB PSRAM） |
| **屏幕** | 4.2 寸反射式 LCD，300 × 400，无背光 |
| **供电** | 18650 锂电池（板载电池座） |
| **存储** | Micro SD 卡（FAT32 格式，用于配置文件） |
| **配件** | USB-C 数据线（烧录用），RTC 备用电池（可选） |

### 官方资源

- [产品文档](https://docs.waveshare.net/ESP32-S3-RLCD-4.2/)
- [Waveshare GitHub 仓库](https://github.com/waveshareteam/ESP32-S3-RLCD-4.2)（Apache 2.0）
- [购买链接](https://www.waveshare.com/esp32-s3-rlcd-4.2.htm)

---

## 快速开始 — 编译与烧录

### 前置依赖

- [ESP-IDF](https://github.com/espressif/esp-idf) >= 5.5.0
- USB-C 数据线
- Micro SD 卡（FAT32 格式）

### 编译

```bash
cd firmware
. $IDF_PATH/export.ps1   # Windows PowerShell；Linux/macOS 使用 export.sh
idf.py set-target esp32s3
idf.py build
```

### 烧录

```bash
idf.py -p COM_PORT flash monitor
```

### 启用 exFAT / FAT 卷标（可选）

如果你的 TF 卡使用 **exFAT 格式**（通常容量 > 32 GB），默认的 ESP-IDF FATFS 组件不支持，需要手动启用。

在 `.espressif\v6.0.2\esp-idf\components` 中启用 exFAT 支持，将 `FF_FS_EXFAT` 的宏定义从 `0` 改为 `1`：

```c
#define FF_FS_EXFAT 1   // 从 0 改为 1
```

启用 64 位 LBA 支持（exFAT 需要大容量存储支持，因此也需要开启 `FF_LBA64`）：

```c
#define FF_LBA64 1      // 从 0 改为 1
```

然后在 menuconfig 中勾选对应选项：

```bash
idf.py menuconfig
```

依次进入：

```
  └── Component config
        └── FAT Filesystem support
              └── Use FATFS volume label   ← 勾选启用
```

> 若 SD 卡仍为 FAT32，可跳过本节。

### 配置

1. 将 Micro SD 卡格式化为 FAT32。
2. 在 SD 卡根目录创建 `token-ticker/config.json`（参见[配置说明](#配置说明)）。
3. 插入 SD 卡并启动设备。

完整示例见 [docs/examples/sdcard-config.example.json](docs/examples/sdcard-config.example.json)。

---

## 配置说明

设备首次启动时从 SD 卡读取配置，并持久化到 NVS。

```json
{
    "version": 1,
    "device": {
        "timezone": "Asia/Shanghai",
        "ntp_enabled": true,
        "ntp_sync_hours": 24,
        "sleep_schedule": {
            "enabled": true,
            "wake_time": "08:00",
            "sleep_time": "24:00",
            "manual_wake_minutes": 5
        }
    },
    "wifi": {
        "enabled": true,
        "ssid": "REPLACE_WITH_YOUR_WIFI_SSID",
        "password": "REPLACE_WITH_YOUR_WIFI_PASSWORD"
    },
    "display": {
        "active_provider_id": "minimax-cn",
        "weather_enabled": false
    },
    "providers": [
        {
            "id": "minimax-cn",
            "type": "minimax",
            "enabled": true,
            "region": "cn",
            "api_key": "REPLACE_WITH_YOUR_MINIMAX_API_KEY"
        }
    ],
    "weather": {
        "enabled": false
    }
}
```

| 配置项 | 说明 |
|--------|------|
| `device.timezone` | 时区，用于显示和 NTP 同步 |
| `device.sleep_schedule` | 设备休眠/唤醒计划 |
| `wifi.ssid` | Wi-Fi 网络名称 |
| `wifi.password` | Wi-Fi 密码 |
| `display.active_provider_id` | 屏幕显示的供应商 ID |
| `providers[].id` | 供应商唯一标识符 |
| `providers[].type` | 供应商类型（如 `minimax`） |
| `providers[].region` | 供应商区域（如 `cn`） |
| `providers[].api_key` | 供应商 API 密钥 |

---

## 项目结构

```
.
├── AGENTS.md              # AI 辅助开发规范
├── LICENSE                # Apache 2.0
├── README.md
├── docs/                  # 研究笔记、ADR、设计文档、操作手册
│   ├── adr/               # 架构决策记录
│   ├── architecture/      # 固件边界、路线图、设计方案
│   ├── examples/          # 配置示例与测试数据
│   ├── harness/           # 代理工作流与测试规范
│   ├── questions/         # 未关闭的产品/技术决策
│   └── research/          # 厂商文档、供应商笔记、参考项目分析
├── external/              # 下载的参考资源与仓库
├── firmware/              # ESP-IDF 固件源码
│   ├── app/               # 编排与任务调度
│   ├── bsp/               # 板级支持（显示、传感器、电源）
│   ├── domain/            # 配额模型与应用状态
│   ├── main/              # 入口点与组件注册
│   ├── platform/          # 系统服务（Wi-Fi、时间、HTTP、存储）
│   ├── providers/         # 供应商适配器（每供应商一个）
│   ├── test/              # 固件测试
│   └── ui/                # 渲染与交互流程
└── tools/                 # 本地预处理脚本
```

### 模块边界

- `bsp/` — 板级引脚常量、外设驱动、板级组装
- `platform/` — 可复用系统服务（Wi-Fi、SNTP、NVS、HTTPS、凭据管理）
- `providers/` — 直接云端集成、响应解析、错误处理
- `domain/` — 标准化配额模型、应用状态、更新策略
- `ui/` — 页面、组件、渲染调度（不含 HTTP 或凭据逻辑）
- `app/` — 生命周期编排、调度器、休眠/唤醒协调

---

## 文档索引

新贡献者建议按以下顺序阅读：

1. `README.md`
2. `AGENTS.md`
3. `docs/harness/README.md`
4. `docs/adr/ADR-0001-esp-idf-first.md`
5. `docs/architecture/repo-layout.md`
6. `docs/architecture/roadmap.md`

完整阅读顺序见 [docs/README.md](docs/README.md)。

---

## 路线图

| 阶段 | 状态 | 说明 |
|------|------|------|
| 0 — 调研准备 | 已完成 | 厂商资料库、ADR、框架评估 |
| 1 — 板级基线 | 已完成 | 屏幕、按键、RTC、传感器、电池 ADC、Wi-Fi |
| 2 — 平台服务 | 已完成 | HTTPS 客户端、NVS 配置、SNTP 同步、调度器、休眠 |
| 3 — 首个供应商集成 | 已完成 | MiniMax 中国站配额拉取 + 标准化渲染 |
| 4 — 产品体验与功耗 | 进行中 | 界面打磨、续航测量、过期/离线状态处理 |
| 5 — 多供应商扩展 | 已规划 | 供应商注册、独立凭据、UI 聚合 |

详情见 [docs/architecture/roadmap.md](docs/architecture/roadmap.md)。

---

## 设计约束

- **无桌面中转。** 设备配置完成后独立运行。
- **续航是第一优先级。** 所有设计决策均考虑功耗影响。
- **友好的 AI 代理支持。** 明确的构建系统、清晰的组件边界、本地化文档。
- **供应商无关架构。** 新增供应商应只需在供应商模块内修改。

---

## 贡献指南

本仓库同时支持人类开发者与 AI 辅助开发。详见 [AGENTS.md](AGENTS.md) 了解规范与工作流。

1. Fork 本仓库。
2. 创建特性分支。
3. 提交清晰的修改说明。
4. 提交 Pull Request。

提交前请确保：
- 固件编译无警告。
- 现有测试通过（详见 [docs/harness/testing-and-validation.md](docs/harness/testing-and-validation.md)）。
- 涉及架构变更的修改需附带 ADR 更新。

---

## 开源协议

本仓库使用 Apache License, Version 2.0。详见 [LICENSE](LICENSE)。

---

## 致谢

- [Waveshare](https://www.waveshare.com) — 提供 ESP32-S3-RLCD-4.2 开发板及参考资料
- [Espressif](https://www.espressif.com) — 提供 ESP-IDF 开发框架
- [MiniMax](https://www.minimaxi.com) — 首个支持的 LLM 供应商
