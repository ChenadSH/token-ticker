[**中文**](README_ZH.md)

# token-ticker

Standalone battery-powered desktop LLM quota monitor for your desk.

A self-contained device built on the [Waveshare ESP32-S3-RLCD-4.2](https://www.waveshare.com/esp32-s3-rlcd-4.2.htm) that fetches and displays LLM usage quotas directly from provider APIs — no desktop relay, no local proxy, no cloud dependency.

---

## Features

- **4.2" reflective LCD** — sunlight-readable, no backlight needed
- **Live quota polling** — fetches usage data directly from LLM provider APIs over HTTPS
- **Date & time display** — SNTP-synced with RTC write-back
- **Battery monitoring** — ADC reading with percentage display
- **Temperature & humidity** — onboard SHTC3 sensor
- **Wi-Fi connectivity** — provisioned via SD card config
- **SD card config** — JSON-based provisioning, no re-flash needed
- **NVS persistence** — stores secrets and config across reboots
- **Button interaction** — manual refresh and power control
- **Sleep & wake** — explicit power management for battery life

### Supported Providers

| Provider | Status |
|----------|--------|
| MiniMax (China) | Verified on hardware |
| Others | Planned (provider-agnostic architecture) |

---

## Hardware Requirements

| Component | Description |
|-----------|-------------|
| **Board** | [Waveshare ESP32-S3-RLCD-4.2](https://www.waveshare.com/esp32-s3-rlcd-4.2.htm) (SKU: 33298 / 33507) |
| **SoC** | ESP32-S3-WROOM-1-N16R8 (dual-core LX7 @ 240 MHz, 16 MB Flash, 8 MB PSRAM) |
| **Display** | 4.2" reflective LCD, 300 x 400, no backlight |
| **Power** | 18650 Li-ion battery (holder onboard) |
| **Storage** | Micro SD card (FAT32) for config |
| **Extras** | USB-C cable (for flashing), RTC backup battery (optional) |

### Board Resources

- [Product documentation](https://docs.waveshare.net/ESP32-S3-RLCD-4.2/)
- [Waveshare GitHub repository](https://github.com/waveshareteam/ESP32-S3-RLCD-4.2) (Apache 2.0)
- [Purchase link](https://www.waveshare.com/esp32-s3-rlcd-4.2.htm)

---

## Quick Start — Build & Flash

### Prerequisites

- [ESP-IDF](https://github.com/espressif/esp-idf) >= 5.5.0
- USB-C cable
- Micro SD card (FAT32 formatted)

### Build

```bash
cd firmware
. $IDF_PATH/export.ps1   # or export.sh on Linux/macOS
idf.py set-target esp32s3
idf.py build
```

### Flash

```bash
idf.py -p COM_PORT flash monitor
```

### Provision

1. Format a Micro SD card as FAT32.
2. Create `token-ticker/config.json` at the SD card root (see [Configuration](#configuration)).
3. Insert the SD card and boot the device.

See [docs/examples/sdcard-config.example.json](docs/examples/sdcard-config.example.json) for a complete example.

---

## Configuration

The device reads configuration from an SD card on first boot and persists to NVS.

```json
{
  "wifi": {
    "ssid": "YourNetwork",
    "password": "YourPassword"
  },
  "providers": {
    "minimax": {
      "group_id": "your-group-id",
      "api_key": "your-api-key",
      "base_url": "https://api.minimax.chat/v1"
    }
  },
  "refresh_interval_min": 15,
  "timezone_offset": 8
}
```

| Key | Description |
|-----|-------------|
| `wifi.ssid` | Wi-Fi network name |
| `wifi.password` | Wi-Fi password |
| `providers.<name>.group_id` | Provider group/account ID |
| `providers.<name>.api_key` | Provider API key |
| `refresh_interval_min` | Polling interval in minutes |
| `timezone_offset` | UTC offset in hours |

---

## Project Structure

```
.
├── AGENTS.md              # AI-assisted development conventions
├── LICENSE                # Apache 2.0
├── README.md
├── docs/                  # Research notes, ADRs, design docs, runbooks
│   ├── adr/               # Architecture Decision Records
│   ├── architecture/      # Firmware boundaries, roadmap, design proposals
│   ├── examples/          # Config examples and fixture payloads
│   ├── harness/           # Agent workflow and testing conventions
│   ├── questions/         # Open product/technical decisions
│   └── research/          # Vendor docs, provider notes, reference analysis
├── external/              # Downloaded reference assets and repos
├── firmware/              # ESP-IDF firmware source
│   ├── app/               # Orchestration and tasks
│   ├── bsp/               # Board support (display, sensor, power)
│   ├── domain/            # Quota models and app state
│   ├── main/              # Entry point and component registration
│   ├── platform/          # System services (Wi-Fi, time, HTTP, storage)
│   ├── providers/         # Provider adapters (one per provider)
│   ├── test/              # Firmware tests
│   └── ui/                # Rendering and interaction flow
└── tools/                 # Local preprocessing scripts
```

### Module Boundaries

- `bsp/` — board-specific pin constants, peripheral drivers, board assembly
- `platform/` — reusable system services (Wi-Fi, SNTP, NVS, HTTPS, secrets)
- `providers/` — direct cloud integrations, response parsing, error handling
- `domain/` — normalized quota models, app state, update policies
- `ui/` — pages, widgets, render scheduling (no HTTP or credential logic)
- `app/` — lifecycle orchestration, scheduler, wake/sleep coordination

---

## Documentation Map

For new contributors, read in this order:

1. `README.md`
2. `AGENTS.md`
3. `docs/harness/README.md`
4. `docs/adr/ADR-0001-esp-idf-first.md`
5. `docs/architecture/repo-layout.md`
6. `docs/architecture/roadmap.md`

See [docs/README.md](docs/README.md) for the full reading order.

---

## Roadmap

| Phase | Status | Description |
|-------|--------|-------------|
| 0 — Research Bootstrap | Done | Vendor corpus, ADRs, framework evaluation |
| 1 — Board Baseline | Done | Display, buttons, RTC, sensor, battery ADC, Wi-Fi |
| 2 — Platform Services | Done | HTTPS client, NVS config, SNTP sync, scheduler, sleep |
| 3 — First Provider | Done | MiniMax CN quota polling with normalized rendering |
| 4 — Product UX & Power | In progress | Layout polish, battery-life measurement, stale/offline states |
| 5 — Multi-Provider | Planned | Provider registry, per-provider credentials, UI aggregation |

See [docs/architecture/roadmap.md](docs/architecture/roadmap.md) for details.

---

## Design Constraints

- **No desktop relay.** The device runs independently after provisioning.
- **Battery life is a feature.** All design decisions consider power impact.
- **AI-agent friendly.** Explicit build system, clear component boundaries, local docs.
- **Provider-agnostic.** Adding a new provider should be provider-local work.

---

## Contributing

This repository is designed for both human and AI-assisted development. See [AGENTS.md](AGENTS.md) for the conventions and workflow.

1. Fork the repository.
2. Create a feature branch.
3. Commit with clear messages.
4. Open a pull request.

Before submitting, ensure:
- The firmware builds without warnings.
- Existing tests pass (see [docs/harness/testing-and-validation.md](docs/harness/testing-and-validation.md)).
- Architecture-impacting decisions include an ADR update.

---

## License

Licensed under the Apache License, Version 2.0. See [LICENSE](LICENSE) for details.

---

## Acknowledgments

- [Waveshare](https://www.waveshare.com) for the ESP32-S3-RLCD-4.2 board and reference materials
- [Espressif](https://www.espressif.com) for the ESP-IDF framework
- [MiniMax](https://www.minimaxi.com) for the first supported provider API
