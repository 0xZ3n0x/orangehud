# Building the payload

## Dependencies

| Requirement | Purpose |
|---|---|
| PS5 payload SDK (`PS5_PAYLOAD_SDK`) | Prospero clang toolchain and headers |
| `cmake`, `ninja` | Build |
| `clang` (host) | Host unit tests only |

## Configure and build

```bash
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
cmake --preset ps5-release
cmake --build --preset ps5-release
```

| Preset | Meaning |
|---|---|
| `ps5-release` | Payload, Release |
| `ps5-debug` | Payload, Debug |
| `debug` / `asan` / `tsan` | Host unit tests with the host compiler (see [testing.md](testing.md)) |

Every preset builds into its own directory (`build/ps5-release/`,
`build-host/debug/`, ...).
`cmake --build --preset ps5-release --clean-first -j 8` rebuilds from scratch
with 8 jobs. The root `CMakeLists.txt` picks the configuration (payload or host tests) and
holds the global settings; the targets are defined in `src/<folder>/CMakeLists.txt` (see
[arch.md](arch.md)) and the host tests in `test/CMakeLists.txt`.

## Version

The version is the single line in the root `VERSION` file. CMake reads it as the
project version and passes it to the code as `ORANGEHUD_VERSION`; the daemon
shows it in the klog start line (with logging on) and in the start notification.

To release: set `VERSION`, commit it on `main`, then push a tag `v<VERSION>`. The
Release workflow checks that the tag is on `main` and matches `VERSION`, builds
the payload and publishes `OrangeHUD.elf` with its sha256 as a GitHub release.

## Outputs

| File | Purpose |
|---|---|
| `build/ps5-release/bin/OrangeHUD.elf` | Final payload (daemon; embeds `shellui.elf`) |
| `build/ps5-release/bin/shellui.elf` | ShellUI module embedded in the payload |

Sending the payload again stops the previous daemon (`SIGTERM`, then
`SIGKILL` after 1.5 s) and reuses the HUD module already in SceShellUI.

## Logging

Logging is klog only and is compiled out by default so the shared klog stream
stays quiet; turn it on with `-DORANGEHUD_LOGGING=ON` (the `ps5-debug` preset
does). Notifications and the crash record (`/data/OrangeHUD/OrangeHUD_crash.log`)
do not depend on it. Every line looks like `[tag][subtag] message`.

```c
LOG("Inject", "ShellUI", "pid=%d ready", pid);   // [Inject][ShellUI] pid=1234 ready
LOGE("Inject", "ShellUI", "ptrace attach");      // [Inject][ShellUI] ERROR: ptrace attach: <strerror>
LOG_INFO("started");                             // [OrangeHUD][INFO] started
```

| Switch | Effect |
|---|---|
| `ORANGEHUD_LOGGING` (CMake option, default `OFF`) | Enables `LOG`, `LOGE`, and `LOG_ERROR` … `LOG_DEBUG` |
| off (default) | All logging is compiled out; arguments are still type-checked |

Read the stream from a PC with any klog reader (for example
`nc <ps5-ip> 3232` with `klogsrv`).

## Host tests

```bash
sh test/run.sh plain      # or: address, thread (sanitizers)
```

Details in [testing.md](testing.md).
