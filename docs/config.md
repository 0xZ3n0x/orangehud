# Configuration

`config.ini` lives at `/data/OrangeHUD/config.ini`
(see [`config/config.ini.example`](../config/config.ini.example)). It is
created with defaults on first start. The daemon re-reads it every second and
republishes the overlay settings; the HUD rebuilds itself when they change, so
no restart is needed. An invalid file keeps the previous settings.

## Keys

| Key | Default | Values |
|---|---|---|
| `rest_mode.resume_reinject_delay_seconds` | `10` | `0`–`255`; extra wait before re-injecting after Rest Mode (not applied at first start or after a ShellUI restart) |
| `overlay.enabled` | `true` | `true`, `false` |
| `overlay.background` | `true` | `true`, `false`; the translucent box behind the panel |
| `overlay.show_fps` | `true` | `true`, `false` |
| `overlay.show_fps_lows` | `true` | `true`, `false`; adds `1% x  0.1% y` next to the FPS |
| `overlay.show_cpu` | `true` | `true`, `false`; ignored while `cpu_usage_mode` is `per_core`, which always shows the CPU row |
| `overlay.cpu_usage_mode` | `per_core` | `average`, `per_core` |
| `overlay.show_soc` | `true` | `true`, `false` |
| `overlay.show_memory` | `true` | `true`, `false` |
| `overlay.order` | `fps,cpu,gpu,memory` | Comma-separated metric names (the row order in the panel) |

The panel itself is drawn in the top-left corner with a fixed 24 pt font and
MangoHud's default colors, plus the frametime graph below the metric rows.

## Runtime data

| Path | Contents |
|---|---|
| `/data/OrangeHUD/config.ini` | Configuration |
| `/data/OrangeHUD/OrangeHUD_crash.log` | Crash backtrace (daemon) |
| `/system_tmp/orangehud/hud_frame` | The seqlock record the daemon publishes and the ShellUI module reads |
| `/system_tmp/orangehud/ready/` | ShellUI readiness markers |
