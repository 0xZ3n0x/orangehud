# OrangeHUD architecture

OrangeHUD is one payload (`OrangeHUD.elf`) that draws an in-game performance HUD.
It runs in two places: a daemon process and a module injected into SceShellUI. The
build produces two ELFs; only `OrangeHUD.elf` is shipped, it embeds the other one.
Nothing runs inside the game.

## Runtime flow

| Step | Component | Action |
|------|-----------|--------|
| 1 | external `elfldr` :9021 | Runs `OrangeHUD.elf` |
| 2 | daemon | Stops a previous instance, creates `/data/OrangeHUD` |
| 3 | daemon | Loads `config.ini`, starts the FPS sampler, the HUD publisher and the power-state watcher |
| 4 | daemon | Injects the embedded `shellui.elf` into SceShellUI (`orange_remote`) |
| 5 | shellui | Hooks `Sce.PlayStation.PUI.Application.Update`, builds labels, publishes readiness |
| 6 | daemon | Re-injects when SceShellUI gets a new PID or the console resumes from Rest Mode |

Injection runs in a worker with up to five attempts (1 s, 2 s, 4 s, 8 s between them). A failure notification is shown only after the last attempt.

`kstuff` is a prerequisite. It is loaded separately and is not part of this payload.

## Processes

| Process | Image | Responsibilities |
|---------|-------|------------------|
| Daemon | `OrangeHUD.elf` (thread name `orange_daemon.elf`) | Settings reload, FPS sampling, injection, recovery |
| SceShellUI | injected `shellui.elf` | HUD rendering only (Mono widgets); no sampling, no config parsing |

## Data flow

| Data | Producer | Consumer | Channel |
|------|----------|----------|---------|
| Settings | `config.ini` | daemon | `settings` library file read; overlay part is republished in the HUD frame |
| HUD frame | daemon HUD publisher (`hud/publisher.cpp`) | shellui | `/system_tmp/orangehud/hud_frame` seqlock record: overlay config + ready-made text for FPS/CPU/SoC/memory + per-frame times (`ft10`, from the display flip times) (`contract/hud_frame.h`) |
| ShellUI readiness | shellui | daemon | PID marker under `/system_tmp/orangehud/ready/` |
| Notifications | daemon, shellui | system toast | `platform` notify |
| Log lines | daemon, shellui | PC klog reader | `klog_printf` via `LOG` / `LOG_*` (no log file) |
| Crash record | daemon signal handler | user | klog + `/data/OrangeHUD/OrangeHUD_crash.log` |

## Modules

`src/` has one folder per ELF (`daemon/`, `shellui/`) and `shared/` for
what more than one ELF uses. A library only one ELF uses lives in that ELF's folder
(`daemon/fps`, `daemon/settings`). Where a folder holds both console-facing glue and
decision logic, the logic sits in its own files with no Sony or Mono includes
("pure" below), so the host tests can cover it.

Includes are module-qualified, e.g. `<platform/log.h>`, `<contract/hud_frame.h>`;
`src/shared/` is the include root for the shared modules (`src/daemon/` also for its
`fps` and `settings` libraries) and `third_party/` for the vendored code
(`<hde64/hde64.h>`). Which module may include which is written down in `CLAUDE.md`.

The root `CMakeLists.txt` is the only file that includes modules (from `cmake/`,
by name). It holds the settings both configurations share (language standards,
logging option, include roots) and includes `orange_targets` (the
`orange_library` / `orange_elf` helpers); for the payload build it also includes
`payload_sdk` (SDK checks, SDK include paths and flags). Each folder under
`src/` defines its own targets: `shared/`, `daemon/` (with the settings and fps
libraries), and `shellui/`. The libraries are always defined; the
payload build adds their console-only files and the two ELFs. The host tests
(`test/CMakeLists.txt`) link the same libraries, built with the host compiler and
without those console-only files. Sanitizers are plain compiler flags set by the
`asan` / `tsan` presets (and by `test/run.sh` and CI).

```text
src/
  daemon/            main.cpp (entry, crash handlers)
    core/            settings store, shutdown/lifecycle, fault_frame.h
    console/         console wrappers (power state, running game, Sony API prototypes)
    hud/             HUD publisher (+ pure: texts, configuration)
    sampling/        FPS sampler thread
    inject/          the injection lock, the remote injector, the embedded module
      shellui/       ShellUI injector (+ pure policy)
    fps/             the FPS record and its holder (only the daemon uses it)
      sources/       /dev/dce flip status
      stats/         pure: flip extraction, frame statistics
    settings/        config.ini schema (only the daemon uses it)
  shellui/           module-wide state and header
    boot/            init phases, hook installation, lifecycle barrier
    hud/             Mono adapter and the per-frame hook
      core/          pure layout, scene diff, frame bookkeeping
    mono/            Mono runtime helpers
  shared/            what more than one ELF uses
    contract/        cross-process ABI: frame, /system_tmp paths
    platform/        log, notify, readiness markers, clock, PS5 ABI types
    proc/            process / kernel helpers (authid, lookup)
    detour/          function hooking
```

| Path | Role |
|------|------|
| `src/daemon/` | `main.cpp` (entry, crash handlers), `core/fault_frame.h` |
| `src/daemon/core/` | `settings_store` (`SettingsStore`), `lifecycle` (shutdown request, "last HUD frame published") |
| `src/daemon/console/` | Wrappers around the console: `running_game` (title id / app id / pid, shared), `power_state`, `sce_api.h` (shared Sony API prototypes) |
| `src/daemon/hud/` | `publisher` (HUD publisher, CPU / SoC / memory sampling), `text.hpp` (pure: measured numbers to HUD texts, VM-map sum), `frame_config.hpp` (pure: settings to frame configuration, config_gen rule) |
| `src/daemon/sampling/` | `fps_sampler` (`FlipSampler`) |
| `src/daemon/inject/` | `lock.hpp` (the injection lock), `remote/` (ptrace injector: `inject_elf`), `payloads.c` (`.incbin` of the ShellUI module) |
| `src/daemon/inject/shellui/` | `injector` (`ShellUiInjector`), `policy.hpp` (pure: ready-marker / ABI verdict, power-state watch, resume and restart decisions) |
| `src/daemon/fps/` | `sample.h` (the published FPS record), `publish` (latest-sample store, `LatestSample`) |
| `src/daemon/fps/sources/` | `dce`: `/dev/dce` flip status (counter, flip time) |
| `src/daemon/fps/stats/` | Pure: `display_flips.hpp` (new flips out of the raw `/dev/dce` answer), `frame_stats.hpp` (`FrameWindow` lows, `FlipRate`, `FlipFrameStats`) |
| `src/daemon/settings/` | `Settings` schema and INI load/save |
| `src/shellui/` | `shellui.hpp` (module-wide surface), `state.cpp` (HUD frame copy, panel layout, notify) |
| `src/shellui/boot/` | `main.cpp` (init phases, hook installation), `hook_lifecycle` (Installing / Ready / Failed barrier) |
| `src/shellui/hud/` | The adapter: `panel` (metric table, the frametime graph, applies the commands to Mono widgets), `frame_hook.cpp` (the per-frame hook: reads the frame, drives the panel) |
| `src/shellui/hud/core/` | Pure, no Mono: `panel_scene.hpp` (arranges the metric rows and diffs against what is on screen), `frame_driver.hpp` (`FramePacer`, `HudTracker`), `layout.hpp` (the corner-panel box), `text_metrics.hpp` |
| `src/shellui/mono/` | `mono.hpp`, `mono_runtime.cpp` (Mono helpers, version banner), `external_symbols.cpp` (dlsym'd Mono / kernel pointers) |
| `src/shared/contract/` | Cross-process contracts: `hud_frame.h`, `overlay_defs.hpp`, `system_tmp.h` |
| `src/shared/platform/` | klog macros (`log.h`), notify, `ReadyMarker`, `clock.h`, PS5 ABI types |
| `src/shared/proc/` | Process / kernel helpers: `proc` (module handles, `struct proc`), `ucred` + `authid_guard` (authid), `query` (process lookup) |
| `src/shared/detour/` | Detours, trampoline arena, hotpatch builder |
| `third_party/NineS/` | ptrace primitives (`pt.c`) and ELF loader (`elfldr.c`) by John Törnblom (vendored, locally modified); linked into `orange_remote` |
| `third_party/hde64/` | x86-64 length disassembler used by the detour library |

## FPS sampling

One thread (`fps_sampler_thread`, `FlipSampler`) polls the `/dev/dce` flip status at 240 Hz while a game runs
(PS5 native and PS4 BC alike) and sleeps one second otherwise.

### Display flip times (`/dev/dce`)

The ioctl `0x80308217` on `/dev/dce` is a generic dispatcher; selector `0xA` answers with the display's flip status
(`DceSource::sample`, 0x60 bytes). Each answer describes only the LAST flip: qword 1 is a counter that grows by one per
flip and qword 3 is the time of that flip in microseconds (an unknown clock base, so only differences are used). The
sampler reads it every 4.17 ms (240 Hz, a fixed grid, twice the fastest game rate) so each flip is seen before the
next one replaces it; the game is never touched.

| Step | Detail |
|---|---|
| Extraction | `DisplayFlips::feed` (`fps/stats/display_flips.hpp`) reports a flip whenever the counter changed |
| Statistics | `FlipFrameStats` (`fps/stats/frame_stats.hpp`) turns flip times into FPS (last second), 1% / 0.1% low (MangoHud definition over 10 000 frames; stalls over 500 ms and the first 15 s excluded) and the 96-frame graph (`ft10`) |
| Publishing | The collected flips go out every 33 ms as one `OrangeFpsSample` (source `FLIP`) |
| No frames | No flip in the last second: the HUD shows `--`. A read that fails (`DceSource::sample` false) also shows `--` and is retried every 33 ms; there is no other source |

## Build outputs

| ELF | Built from | Links | Role |
|---|---|---|---|
| `shellui.elf` | `src/shellui/` | `detour`, `proc`, `platform` | intermediate: embedded in the daemon, injected into SceShellUI |
| `OrangeHUD.elf` | `src/daemon/` | `settings`, `fps`, `proc`, `platform`, `orange_remote` | the payload; `inject/payloads.c` embeds the one above with `.incbin` |

The libraries are static archives (`liborange_*.a`, `libhde64.a`),
not ELFs. The ELFs land in `build/ps5-release/bin/`.

## Limits

| Limit | Reason |
|-------|--------|
| ShellUI follows `config.ini` changes | The daemon reloads the file and bumps `config_gen`; ShellUI rebuilds the widgets when it changes |
| Frame times are display flip times | They show when the display flipped, not when the game submitted or the GPU finished the frame. A game that repeats a frame (missed deadline) shows a longer interval; the submit-to-flip latency is not measured |
| 240 Hz polling | The flip status holds only the last flip, so a game above 240 FPS loses flips; a poll that is late by more than a flip interval (a busy daemon thread) loses one too, which shows as one longer frame |
| Module and daemon must agree on `ORANGE_HUD_ABI` | The HUD module cannot be replaced in a running SceShellUI (hooking the same method twice fails). The ABI (`contract/hud_frame.h`) is bumped by hand when the daemon/module protocol changes; on a mismatch the daemon asks for a console restart. With the same ABI a new daemon reuses the running module, so a change to the module's own code only shows after SceShellUI restarts |
| `kstuff` must already be loaded | The payload no longer loads it |
| Rest Mode with a game running | One entry into Rest Mode with a title open ended in a kernel panic (`a53mm_is_rings_suspended`, a GPU memory manager assertion); with no game running it worked. Not shown to come from OrangeHUD |
| Few titles tried | The flip reader has run on a PS5 native game at 30, 60 and 120 FPS and a PS4 BC game at 60 FPS. At 120 FPS about one flip in 3600 frames was missed in a 30 s run. Variable refresh rate and dropped frames are untested. The display id in the request is fixed to 1 |
| Config writes are not atomic | The file is written in place and `st_mtime` has second resolution, so a second change within the same second can be missed |
| The crash handler is not signal-safe | It catches signals 1..11 and is not async-signal-safe; it should catch only real fault signals (SEGV, BUS, ILL, FPE, ABRT) and write signal-safely |
