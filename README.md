<p align="center">
  <img src="assets/icon.jpeg" width="140" alt="OrangeHUD icon"/>
</p>

<h1 align="center">OrangeHUD</h1>

<p align="center">
  An in-game performance HUD for jailbroken PlayStation 5 consoles.<br/>
  One payload, injected over the network, drawn over any game.
</p>

<p align="center">
  <a href="#features">Features</a> ·
  <a href="#how-it-works">How it works</a> ·
  <a href="#quick-start">Quick start</a> ·
  <a href="#build">Build</a> ·
  <a href="#configuration">Configuration</a> ·
  <a href="#documentation">Docs</a> ·
  <a href="#credits">Credits</a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/platform-PlayStation%205-003791?logo=playstation" alt="platform"/>
  <img src="https://img.shields.io/badge/language-C%20%2F%20C%2B%2B-00599C" alt="languages"/>
  <img src="https://img.shields.io/badge/build-CMake%20%2B%20Ninja-064F8C?logo=cmake" alt="build"/>
  <img src="https://img.shields.io/badge/license-GPL--3.0%2B-blue" alt="license"/>
</p>

<br/>

```text
 ┌──────────────────────────────────────┐
 │ FPS 59.9  1% 58  0.1% 52             │
 │ CPU 48C  12%                         │
 │ SoC 61C  84W                         │
 │ MEM 8.3/12.2 GB                      │
 │ Frametime               min 16 max 18│
 │  ▂▂▂▁▂▂▃▂▂▂▂▂▂▁▂▂▂▂▂▅▂▂▂▂▂▂▂▂▂▂▂▂▂  │
 └──────────────────────────────────────┘
```

A panel in the top-left corner of the screen: one row per metric plus a live
frametime graph, drawn by a small module inside the system UI while you play.
OrangeHUD is a nod to [MangoHud](https://github.com/flightlessmango/MangoHud):
the panel layout, the frametime graph and the 1% / 0.1% low definition follow
it.

<br/>

# Features

| Metric | What you get |
|---|---|
| **FPS** | Frame rate from the display flip times the kernel reports through `/dev/dce`, polled at 240 Hz from outside the game; `--` while the flip status cannot be read. `1%` / `0.1%` lows follow MangoHud's definition over the last 10 000 frames |
| **Frametime graph** | The last 96 frame times (about 1.6 s at 60 FPS) on a fixed 0–50 ms scale, with the window min/max |
| **CPU** | Temperature and idle-thread usage, averaged or per core (8 cores) |
| **SoC** | Hottest sensor temperature and package power in W — the PS5's CPU and GPU share one chip |
| **MEM** | Direct memory the game has mapped against its budget, e.g. `8.3/12.2 GB` |

Everything is read on the console itself, not by an external tool. How far to
trust each value, and what it actually measures, is in the
[accuracy table](#accuracy).

- **Rest Mode aware** — re-injects after standby and after a ShellUI restart
- **Clean reloads** — sending the payload again stops the old daemon and
  reuses the module already in memory
- **No leftovers** — on daemon exit the HUD is removed at once; after a crash
  it disappears within seconds
- **One file** — `OrangeHUD.elf` embeds the injected module
- **Host-tested** — the decision logic (layout, scene diff, FPS statistics,
  injection policy) is pure C++ covered by unit tests on a PC

<br/>

# How it works

```text
OrangeHUD.elf (daemon, sent to elfldr :9021)
 ├── injects shellui.elf into SceShellUI   → hooks PUI.Application.Update → draws the HUD
 ├── polls /dev/dce at 240 Hz              → display flip times → FPS, lows, frametime graph
 ├── samples CPU / SoC / memory            → ready-made HUD text
 └── publishes everything in a seqlock record under /system_tmp/orangehud/
```

The HUD module never samples and never parses config: it reads one record
and renders it. A config change bumps a generation counter and the panel
rebuilds itself — no restart needed.

The daemon and the module agree on a hand-bumped ABI; on a mismatch the
daemon asks for a console restart instead of replacing a module it cannot
replace. Details in [docs/arch.md](docs/arch.md).

<br/>

# Quick start

| Requirement | Notes |
|---|---|
| Jailbroken PS5 | Firmware supported by your exploit chain |
| [kstuff](https://github.com/EchoStretch/kstuff-lite) | Load before OrangeHUD; not included |
| [elfldr](https://github.com/ps5-payload-dev/elfldr) | Listening on port 9021 |

```bash
nc <ps5-ip> 9021 < build/ps5-release/bin/OrangeHUD.elf
```

1. Load `kstuff` with your usual loader.
2. Send `OrangeHUD.elf` to port 9021.
3. A "Welcome to OrangeHUD <version>" notification confirms the daemon started; the
   ShellUI module is injected automatically.
4. Start a game — the HUD shows the frame rate as soon as the game flips frames.

Sending the payload again is safe: it stops the previous instance first.

<br/>

# Build

Needs the [PS5 payload SDK](https://github.com/ps5-payload-dev/sdk),
`cmake` and `ninja`:

```bash
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
cmake --preset ps5-release
cmake --build --preset ps5-release
```

`build/ps5-release/bin/OrangeHUD.elf` is the payload. Presets `ps5-debug`,
`debug`, `asan` and `tsan` exist; the host tests run with
`sh test/run.sh plain`. See [docs/build.md](docs/build.md).

<br/>

# Configuration

`/data/OrangeHUD/config.ini`, created with defaults on first start, re-read
every second — changes apply without a restart:

```ini
[overlay]
enabled=true
background=true
show_fps=true
show_fps_lows=true
show_cpu=true
cpu_usage_mode=per_core
show_soc=true
show_memory=true
order=fps,cpu,gpu,memory
```

Full schema: [docs/config.md](docs/config.md).

<br/>

# Repository layout

```text
src/daemon/     the payload: settings, FPS samplers, injectors, HUD publisher
src/shellui/    the module inside SceShellUI: boot, HUD panel, Mono bridge
src/shared/     used by more than one ELF: contract/ (cross-process ABI), platform/, proc/, detour/
                (the fps/ and settings/ libraries sit inside src/daemon/: only the daemon uses them)
third_party/    NineS (ptrace and ELF loader code), hde64 (x86-64 disassembler)
test/           host unit tests, one folder per module
cmake/          the helpers that define the libraries and ELFs
config/         the default config.ini
docs/           architecture, build, config, testing, troubleshooting
assets/         the icon
.github/        CI (host tests with sanitizers, console build) and the release workflow
```

<br/>

# Documentation

| Document | Contents |
|---|---|
| [docs/arch.md](docs/arch.md) | Architecture, data flow, module map, FPS sampling, limits |
| [docs/build.md](docs/build.md) | Build presets, outputs, klog logging |
| [docs/config.md](docs/config.md) | Every `config.ini` key, runtime data paths |
| [docs/testing.md](docs/testing.md) | Host unit tests |
| [docs/injection.md](docs/injection.md) | The ShellUI injection contract |
| [docs/troubleshooting.md](docs/troubleshooting.md) | Symptoms and fixes |

<br/>

# Accuracy

Nothing here was cross-checked against an external reference (wall power
meter, second monitor). What each value is, and how far to trust it:

| Value | What it is | Trust |
|---|---|---|
| `FPS` (flip times) | Rate of the display flips over the last second, as stamped by the kernel when the display flips; polled at 240 Hz, so a game above 240 FPS is not resolved | High |
| `1%` / `0.1%` low (flip times) | MangoHud definition over real display frame times; shown after a 15 s warm-up and 1000 frames, the full 10 000-frame window fills in about 3 minutes at 60 FPS | High |
| `CPU` temperature / usage | One kernel sensor (measured point undocumented); usage from the idle threads `SceIdleCpu0..7` | Medium |
| `SoC` temperature / power | Hottest of up to 32 sensors (treat as an upper bound); power tracks load but is not verified against a meter | Medium |
| `MEM` used / budget | Device (direct memory) mappings, i.e. mapped, not touched; budget from `sceKernelGetDirectMemorySize` | High / Low-medium |

Not available on this hardware: GPU load, a separate VRAM figure (one memory
pool), real fan speed, live clock frequencies.

<br/>

# License

OrangeHUD is licensed under the [GNU General Public License v3 or later](LICENSE)
(`GPL-3.0-or-later`), copyright 0xZ3n0x. The terms are forced by the vendored
code and the SDK this payload is built against; they are not a choice.

- `third_party/NineS/` — elfldr/pt by John Törnblom, GPLv3-or-later (taken
  over from NineS); the injector built on them is this project's own code
- `third_party/hde64/` — Vyacheslav Patkov's Hacker Disassembler Engine 64,
  BSD-style per-file notice
- `src/shellui/mono/mono.hpp` mirrors struct layouts of the Mono runtime (MIT
  license)
- The PS5 payload SDK headers the payload links against are GPLv3+

Each third-party file keeps its own license acknowledgement in its header.

<br/>

# Credits

OrangeHUD owes a lot to the PS5 homebrew and reverse-engineering community.

### Inspired by

- [MangoHud](https://github.com/flightlessmango/MangoHud) — flightlessmango and
  contributors; OrangeHUD is a nod to it. The panel look, the frametime graph
  and the 1% / 0.1% low FPS definition follow its approach
- [OnionHEN](https://github.com/aydencharles/onionHEN) — aydencharles and
  contributors; this project started under that name
- [etaHEN](https://github.com/LightningMods/etaHEN) — LightningMods and
  contributors
- [GoldHEN](https://github.com/GoldHEN/GoldHEN) — SiSTR0 and contributors
- [kstuff-lite](https://github.com/EchoStretch/kstuff-lite) — EchoStretch,
  sleirsgoevy, and contributors; Rest Mode recovery waits for the trophy
  SPRX modules the same way
- [PHU Games Tools](https://github.com/ArkSama) — ArkSama; the `/dev/dce`
  ioctl used for the display flip status follows its sampling
- [NineS](https://github.com/buzzer-re/NineS) — buzzer-re; the injector here
  started from its ptrace-based ShellUI injection

### Used or embedded

- [PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk) — Prospero
  toolchain and headers
- [elfldr](https://github.com/ps5-payload-dev/elfldr) — the loader on port
  9021; not shipped in the payload
- elfldr / pt — John Törnblom; the ptrace primitives and ELF loader vendored
  in `third_party/NineS/`
- hde64 — Vyacheslav Patkov's Hacker Disassembler Engine 64 (vendored, used
  by the detour library)
- [Mono](https://github.com/mono/mono) — MIT license; the struct layouts in
  `src/shellui/mono/mono.hpp` mirror its runtime

<br/>
