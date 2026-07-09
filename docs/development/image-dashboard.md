# Development Spec: Image-Driven Dashboard (v2)

## Purpose

Replace the multi-domain LVGL dashboard (MiniMax / stocks / Baota / weather) with a
**server-rendered image** shown on the 400x300 RLCD panel. The on-device
firmware becomes a thin client:

1. Read Wi-Fi / NTP / sleep schedule from the SD card (unchanged from v1).
2. Poll an HTTP endpoint every N seconds for a pre-rendered 1-bit bitmap.
3. Push the bitmap to the panel and overlay white date/time and battery status
   text on top of the image.
4. When the refresh interval is longer than 2 minutes, drive the Wi-Fi station
   in a connect-fetch-disconnect cycle so the radio is only powered for a short
   active window.

This replaces PRD `multi-domain-dashboard.md` and the associated module tree
under `firmware/modules/`. The LLM-quota, A-share, Baota and weather modules
are removed in this revision; their snapshots and view-model fields disappear.

## Design Constraints

These are inherited from `AGENTS.md` and `docs/adr/ADR-0001-esp-idf-first.md` and
remain non-negotiable:

- **No desktop relay.** Image data must be pulled directly from the cloud.
- **Power first.** Wi-Fi must only be active when needed. RLCD panel must enter
  sleep during long idle periods.
- **All facts anchored in the source tree.** URLs, byte sizes, timings, and
  config keys all come from code or vendor docs in this repo.

## Configuration Schema (v3)

The TF card / NVS JSON schema is simplified. The `providers`, `weather`,
`stocks` and `baota` top-level keys are removed. A new
`display.image_refresh_seconds` key replaces the dashboard refresh keys.

```jsonc
{
  "version": 3,
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
    "image_url": "http://124.221.91.97:39080/render",
    "image_refresh_seconds": 300
  }
}
```

Validation rules in `app_config_validate` after this change:

- `display.image_refresh_seconds` is in `[30, 86400]`. Defaults to 300 if
  absent.
- `display.image_url` is a non-empty string up to `HTTP_URL_MAX_LEN`. Defaults
  to `http://124.221.91.97:39080/render` if absent.
- All previously-existing rules for `device`, `wifi`, and `sleep_schedule`
  are unchanged.
- All `providers`, `weather`, `stocks`, `baota` validation paths are deleted.
- Backwards-compat: if `version: 2` is read, the parser may still build the
  config but unknown keys must be ignored (do not reject). The runtime must
  not reference the deleted modules.

## Rendering Pipeline

### Display Geometry

| Property          | Value                         | Source                              |
|-------------------|-------------------------------|-------------------------------------|
| Panel native size  | 300 x 400 (portrait)         | `board.c` `native_width/height`     |
| Logical size       | 400 x 300 (landscape)        | `display_width/height` + `landscape` |
| Color depth        | 1 bit per pixel               | `rlcd_driver.c` (set_pixel / framebuffer) |
| Framebuffer size   | 15,000 bytes                  | `transfer_pixels >> 3`             |
| Refresh            | partial / LVGL flush         | `display_port.c` `display_port_flush_cb` |

### Server Image Contract

The HTTP server returns the **full 400 x 300 framebuffer** as raw 1-bit data:

- Total bytes: `400 * 300 / 8 = 15,000`
- Encoding: **MSB-first**, left-to-right, top-to-bottom. Each byte represents
  8 horizontal pixels: bit 7 is the leftmost pixel of that group.
- A set bit (`1`) means **white** (matching `rlcd_driver_set_pixel` /
  framebuffer convention in `rlcd_driver.c`).
- Optional HTTP headers accepted but not required by v1: `Content-Type:
  application/octet-stream`, `Cache-Control: no-store`.

The image must already include the visual layout the user wants, including
white panels reserved for the date/time and battery overlays. The firmware
does **not** request a transparent region; it draws the overlay directly on
top of the bitmap using LVGL after the bitmap is copied into the panel
framebuffer.

### HTTP Fetch Pipeline

```
app_runtime (image cycle)
  |
  +-> wifi_platform_ensure_ready          (only when refresh due)
  +-> http_client_get_binary              (raw bytes, no JSON parsing)
  +-> rlcd_driver_write_bitmap             (copy bytes into framebuffer)
  +-> ui_overlay_render_text              (draw white date/time/battery)
  +-> rlcd_driver_display                 (push via SPI to panel)
  +-> wifi_platform_stop                   (only if long-interval mode)
```

A new function `http_client_get_binary` is added to `platform/http/http_client.c`:

```c
bool http_client_get_binary(const http_request_t *request,
                            uint8_t *response_buffer,
                            size_t response_buffer_len,
                            http_response_meta_t *meta);
```

It is functionally identical to `http_client_get_json` but documents that the
buffer is treated as raw bytes. The existing JSON path remains for any future
HTTP needs.

A new function is added to `bsp/display/rlcd_driver.h`:

```c
bool rlcd_driver_write_bitmap(rlcd_driver_t *driver,
                             const uint8_t *bitmap,
                             size_t bitmap_len);
```

It validates that `bitmap_len == driver->framebuffer_len`, then
`memcpy(driver->framebuffer, bitmap, bitmap_len)`. The bitmap must be in the
panel's native byte order. Today the existing `rlcd_compute_index` produces
**MSB-first, left-to-right, top-to-bottom**, which matches the server
contract. **Do not change the bit ordering without rewriting both the server
producer and the renderer simultaneously.**

### Text Overlay

After the bitmap is pushed, the firmware draws white overlay text using LVGL
on top of the same framebuffer. Layout (all text in `lv_color_white()` on
`lv_color_black()`):

| Anchor              | Text                       | Font               | Notes |
|---------------------|----------------------------|--------------------|-------|
| top-left (x=8, y=4) | `MM-DD HH:MM`              | `montserrat_14`    | Updated every minute |
| top-right (x≈340, y=4) | `▮▮▮▮ 85%`             | `montserrat_14` + rect | Battery icon + percent |

The overlay coordinates intentionally avoid the area reserved by the server
image for the same data. The server is expected to leave a black or neutral
band along the top 26 px of the image so the white overlay is legible.

A new component `firmware/ui/ui_image_overlay.c` owns this. It reuses the
existing `ui_app.c` machinery to invalidate the LVGL dirty area. LVGL is
**not** the source of the image; it only owns the small overlay.

### Power Management Strategy (≥ 2 minute refresh)

When `image_refresh_seconds >= 120`, the runtime uses a **Wi-Fi duty-cycle**:

```
loop:
    if refresh_due():
        wifi_platform_ensure_ready(timeout=10s)
        fetch_image(timeout=8s)
        wifi_platform_stop()
        rlcd_driver_set_sleep(false) if sleeping
        push framebuffer to panel
        (rlcd_driver_set_sleep(true) does NOT happen between refreshes; the
        panel refresh is fast and the user-visible win is the radio being off)
    wait(image_refresh_seconds)
```

For `< 120s`, the Wi-Fi station stays connected to amortize DHCP / association
cost across refreshes. A future ADR may revisit this threshold.

`wifi_platform_ensure_ready` and `wifi_platform_stop` already exist in
`platform/wifi/`. The runtime just calls them around the fetch.

### NTP Sync

NTP continues to run on every boot when Wi-Fi is up, and once per 24h
afterwards (unchanged). RTC write-back remains in `app_bootstrap_run`.

### Sleep Schedule

The `device.sleep_schedule` block is unchanged. When the runtime is in
`SCHEDULED_SLEEP` mode, the RLCD panel is put to sleep via
`rlcd_driver_set_sleep(true)`. Wake is via the existing BOOT/KEY GPIO
ext1 wakeup (`app_bootstrap_configure_light_sleep_wakeup`).

## File-Level Changes

### Removed

```
firmware/modules/stocks/*                       (entire directory)
firmware/modules/baota/*                        (entire directory)
firmware/modules/weather/*                      (entire directory)
firmware/modules/encoding/gbk_utf8.*            (no longer used)
firmware/ui/ui_home_screen.c                    (replaced by overlay-only)
firmware/ui/ui_home_view_model.c                (no provider/state to format)
```

`firmware/modules/module_interface.h`,
`firmware/modules/module_registry/`, and the registration calls in
`app_bootstrap.c` are removed. `firmware/CMakeLists.txt` no longer lists
`modules/*`.

### Added

```
firmware/ui/ui_image_overlay.c
firmware/ui/ui_image_overlay.h
firmware/platform/http/http_client_binary.c      (or extend http_client.c)
```

### Modified

- `firmware/platform/http/http_client.{h,c}` — add `http_client_get_binary`.
- `firmware/bsp/display/rlcd_driver.{h,c}` — add `rlcd_driver_write_bitmap`.
- `firmware/domain/app_config.{h,c}` — remove `weather_config_t`,
  `stocks_config_t`, `baota_config_t`, `provider_config_t` arrays; add
  `image_url`, `image_refresh_seconds`.
- `firmware/platform/storage/config_store.c` — drop parser branches for
  deleted keys; parse new `display.image_url` and
  `display.image_refresh_seconds`. Keep v1/v2 parser as no-op for unknown keys.
- `firmware/app/app_runtime.c` — replace provider / stocks / baota / weather
  scheduling with `image_due()` channel; fetch raw bytes; trigger overlay
  render; wrap Wi-Fi around the fetch for long-interval mode.
- `firmware/app/app_bootstrap.c` — drop module registry init and module
  seeding; seed the initial image snapshot instead.
- `firmware/app/app_scheduler.{h,c}` — replace `stocks_*_due` and `baota_due`
  channels with a single `image_due` channel driven by
  `display.image_refresh_seconds`.
- `firmware/examples/sdcard-config.example.json` — bump to `version: 3`,
  remove `providers`, `weather`, `stocks`, `baota`; add `display.image_url`
  and `display.image_refresh_seconds`.
- `README.md` and `README_ZH.md` — replace "Optional Modules" sections with
  a short note on the new image endpoint and the `display.image_refresh_seconds`
  config key.

## Domain Refactor

The v2 model is gone. `ui_boot_model_t` keeps:
- `rtc_time`, `power`, `environment`, `wifi`, `config_*`
- new: `image_loaded_epoch` (int64, last successful image fetch wall time)

Removed: `provider_snapshot`, `weather_snapshot`, `stocks_market_snapshot`,
`stocks_watchlist_snapshot`, `baota_snapshot`, and every `has_*` flag
associated with them.

`ui_home_view_model_t` is removed entirely. `ui_image_overlay` is a
struct holding only the two text strings (date/time and battery percent)
and a "stale" flag.

## Test Plan

- **Unit (host)** — none of the deleted module parsers and no new HTTP code can
  be unit-tested without a mock server. The overlay LVGL widget is exercised
  via `test_ui_image_overlay.c` (to be added) which feeds sample view-models
  and asserts the rendered labels.

- **Integration (device)**:
  1. Standalone server endpoint reachable on the LAN.
  2. Verify first image push: panel shows the bitmap with the overlay.
  3. Toggle `image_refresh_seconds` between 60 and 600 in `config.json`;
     confirm Wi-Fi cycles only when ≥120.
  4. Force NTP failure: RTC time should still update from the last sync.
  5. Inject a 5xx from the server: overlay stays visible with the previous
     bitmap, a "STALE" badge appears in the bottom-right corner.

- **Manual regression** — when the device is in deep sleep, press BOOT:
  image refresh must resume within the configured interval without
  re-entering provisioning.

## Open Questions

These are tracked in `docs/questions/` once implementation starts:

1. Should `display.image_url` accept `https://` too, or only `http://`? The
   current `http_client` defaults to port 80 with no TLS hook for GET, so
   the contract for now is `http://` only. A future ADR may add a
   `skip_tls_verify` flag modeled on the old `http_request_t`.
2. When the server response size differs from 15,000 bytes, the renderer must
   reject it. We should log the mismatch and retain the previous bitmap.
3. How to surface "stale" state — a corner badge, a small banner, or a
   `display_port_render()` overlay widget. Initial implementation will use
   a small `STALE` text in the bottom-right corner.

## Risks

- **Render order**: any regression that pushes the overlay before the bitmap
  will be visible as white text over white image regions. Mitigation: the
  overlay widget must read `image_loaded_epoch` and refuse to draw before
  the first successful push.
- **Wi-Fi reconnect cost**: at the 2-minute boundary the radio toggles on
  for ~1 s and off again. If association takes >10 s the refresh misses
  its slot. The runtime must schedule the Wi-Fi session at
  `image_refresh_seconds / 2` before the next due time, not exactly at due.
- **Bit ordering drift**: any change to `rlcd_compute_index` (e.g.
  landscape vs portrait flip) silently breaks both producers. Guard with
  a compile-time assert that `framebuffer_len == 15000`.

## Definition Of Done

A change that delivers this spec is done when:

1. `idf.py build` succeeds with `-Werror` and no unused-function warnings.
2. The deleted module directories and headers are gone from the tree; the
   `firmware/modules/` directory either disappears or contains only the
   `encoding/` helper if any future text encoding is needed (likely
   delete).
3. A device powered only by SD-config can pull a 1-bit image from the
   configured URL and render it with the overlay within the configured
   refresh interval.
4. README quick-start references the new `display` config keys and the
   image endpoint URL.
5. `docs/architecture/multi-domain-dashboard.md` is moved to
   `docs/archive/` or deleted, and a one-line pointer is added at the top
   of `docs/architecture/README.md` (or equivalent index) saying "this PRD
   is superseded by `docs/development/image-dashboard.md`".