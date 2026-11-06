# Host unit tests

Host-side unit tests for the shared libraries and the HUD logic. They build with
the host compiler (no PS5 SDK) as the `orange-host-tests` target under `test/`.

## Run

```bash
sh test/run.sh plain      # or: address, thread (sanitizers)
# or, by hand:
cmake --preset debug      # asan / tsan also exist
cmake --build --preset debug
ctest --preset debug
```

The tests link the libraries from `src/` (`platform`, `settings`, `detour`, `fps`, built
with the host compiler without their console-only files), so a source file is
listed in one place only. Requirements: host C11 / C++20 compiler (clang), CMake 3.21+, Ninja. The tests
are a separate configuration because they need the host compiler, which cannot
share a configure with the PS5 cross toolchain.

```bash
ORANGE_TEST_VERBOSE=1 ctest --preset debug   # extra klog-style noise on stderr
build-host/debug/test/orange-host-tests      # run the binary directly
```

Build tree: `build-host/<preset>/` (the payload builds into `build/ps5-release/`).

## Layout

`test/<module>/` mirrors the source modules: the libraries (`platform`, `contract`,
`settings`, `detour` with the vendored `hde64`, `fps`) and the two processes (`daemon`
and `shellui`: their pure logic). `stubs/` holds the host replacements for PS5-only
symbols. A suite is registered in `test_main.cpp` and its source in
`test/CMakeLists.txt`.

## Coverage

| Suite | What it locks |
|-------|----------------|
| `test_settings` | schema serialize/round-trip, defaults, rest-mode clamp, overlay order |
| `test_ready` | ready markers, PID-bound process instances, path builder, name rejection, runtime flags |
| `test_platform_notify` | notification prefix, truncation, no-watermark, send path |
| `test_frame_stats` | flip-time statistics: MangoHud percentile definition, stall cut-off, window, flip rate |
| `test_flip_times` | flip frame statistics (warm-up, silence, lows cadence), display flip extraction from the /dev/dce answer |
| `test_hud_frame` | HUD frame record layout and seqlock |
| `test_overlay_layout` | the corner-panel box, stacked rows, hidden and reordered metrics, per-core CPU keeping its row, everything parked without a screen |
| `test_overlay_text_metrics` | panel text width estimation (glyph advances, minimum width, font scale) |
| `test_frame_driver` | HUD hook bookkeeping: settle delay and update interval, build / repaint / retry on config generation and failed builds, panel removal after a second without frames |
| `test_inject_policy` | ShellUI injector rules: power-state watch ticks (resume once after standby), resume / restart decisions, when recovery stops retrying |
| `test_hud_config` | settings to HUD frame configuration (fields, per-core forces CPU on, order normalized) and the config_gen bump rule |
| `test_hud_text` | HUD texts from measured numbers (FPS and lows, CPU, memory, SoC temperature / power), per-core usage maths, VM-map device memory sum |
| `test_panel_scene` | panel arrangement (groups, order, font scale, rows at their origins) and the update plan (park once, place / set text only on change, unapplied commands re-planned) |
| `test_hde64` | x86_64 length decoder |
| `test_hotpatch` | aligned atomic entry-patch image, absolute jump encoding |
| `test_trampoline_arena` | unique, non-overlapping, near allocations |
| `test_x64_relocator` | RIP-relative, rel8/rel32 call/jmp/jcc relocation and safe rejection |
| `test_hook_lifecycle` | Installing/Ready/Failed callback barrier |

## Intentionally not host-tested

| Area | Why |
|------|-----|
| `proc` library: proc / ucred | kernel_copyout |
| Mono invocation, `OnRender_Hook` | needs SceShellUI (the layout, diff and per-frame decisions behind it are covered by `test_panel_scene` and `test_frame_driver`) |
| `daemon/inject/remote` (the remote injector) | ptrace |
| Daemon FPS sampler | `/dev/dce` |
