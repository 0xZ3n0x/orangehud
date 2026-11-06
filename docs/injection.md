# ShellUI injection contract

This document defines the active ShellUI injection path, its synchronization
rules, and its failure behavior.

## Runtime flow

```text
daemon: ShellUiInjector::inject_once() [daemon/inject/shellui/injector.cpp]
  |- live_shellui_pid()               find SceShellUI
  `- inject_elf(pid, shellui.elf)  [daemon/inject/remote/injector.c]
            |- get_proc_by_pid(): the process must exist
            |- set_ucred_to_ptrace() once for the inject window
            |- pt_attach(SceShellUI)
            |- pt_resolve(pthread_create)
            |- elfldr_load()
            |- elfldr_payload_args()
            |- map and copy the bootstrap stager
            |- map and copy the SCEFunctions block
            |- pt_call2(bootstrap) -> remote pthread_create(elf_main)
            `- pt_detach() and restore the caller authid
```

The remote stager terminates with `int3`. `pt_call2` waits for that stop before
restoring the interrupted ShellUI registers.

## Readiness identity

After every required hook is ready, `shellui.elf` writes its PID to:

```text
/system_tmp/orangehud/ready/hud
```

The daemon waits up to 45 seconds for the expected PID. The marker is an
instance identity, not a boolean flag:

- matching PID means the current ShellUI instance is initialized;
- missing or different PID requires injection into the current process;
- a PID change during the handshake invalidates the acknowledgement.

`ShellUiInjector::inject_once()` holds one mutex across the identity check,
injection, and readiness wait. Cold start, resume recovery, and restart
recovery therefore cannot ptrace the same ShellUI process concurrently.

`ShellUiInjector` has two entry points:

- **Cold start:** `inject_now()` claims the live PID first (so the watcher does
  not start a second worker) and starts a recovery worker.
- **Watching:** `watch()` polls the power state every 500 ms.
  - **Rest Mode resume:** on the `WORKING` transition it waits one second and
    reconciles the live ShellUI (re-inject unless the marker already matches).
  - **ShellUI restart:** it compares the live SceShellUI PID with the last PID
    it scheduled; a new PID starts a recovery worker.

The recovery worker waits for `libSceNpTrophy.sprx` and `libSceNpTrophy2.sprx`,
applies `rest_mode.resume_reinject_delay_seconds`, and then runs the same
serialized injection flow. The extra delay applies to the first attempt only.

## Injection invariants

| Area | Contract |
|------|----------|
| Authid | `inject_elf` enters one `PTRACE_AUTHID` window and restores the caller's authid on every exit path |
| ptrace | Calls execute inside that credential window without per-call authid changes |
| Remote calls | `pt_call`, `pt_call2`, and `pt_syscall` use an ABI-aligned private stack below the interrupted frame and outside the 128-byte red zone |
| Register restore | `pt_call2` waits for the stager stop before restoring saved registers |
| Mapping | Null, `MAP_FAILED`, mprotect, and copyin failures abort injection |
| Attach state | A successful detach clears the attached state; cleanup detaches whenever an attach completed |
| Return value | Any failed stage returns failure to the daemon and cannot publish readiness |

Single-step return detection compares against the private entry stack pointer.
The invariant is `rsp % 16 == 8` at the remote call boundary.

## Hook installation safety

ShellUI remains active while the payload thread installs hooks. Two mechanisms
keep callbacks coherent during that window:

- `TrampolineArena` uses non-fixed mmap hints and page-sized bump allocation.
  Every trampoline has a unique, non-overlapping address within rel32 reach of
  its target. Allocation fails closed when no valid mapping is available.
- Hook callbacks remain pass-through while lifecycle state is `Installing`.
  The payload publishes `Ready` only after all detours and shared dependencies
  are initialized.

The render hook (`PUI.Application.Update`) calls the original function until
the lifecycle barrier reaches `Ready`.

## Failure behavior

An injection failure has these observable results:

1. The daemon receives a failed injection result.
2. The target is detached when attachment succeeded.
3. The daemon authid is restored.
4. The expected PID is not published as ready.
5. A later serialized request may retry the current ShellUI instance.

The injector does not kill or respawn ShellUI as part of this operation.

## Source map

| Responsibility | Source |
|----------------|--------|
| Request serialization and PID readiness | `src/daemon/inject/shellui/injector.cpp` |
| The decisions (pure): marker verdict, power-state watch, resume / restart | `src/daemon/inject/shellui/policy.hpp` |
| Injection orchestration | `src/daemon/inject/remote/injector.c` |
| ELF mapping and stager launch | `src/daemon/inject/remote/injector.c` |
| Remote ptrace calls | `third_party/NineS/pt.c` |
| Trampoline allocation | `src/shared/detour/trampoline_arena.cpp` |
| Hook lifecycle barrier | `src/shellui/boot/hook_lifecycle.cpp` |

## The remote injector

ptrace-based ELF injector used by the daemon to place a module into a process
(SceShellUI for the HUD). It is the static
library `orange_remote`:

| File | Role |
|------|------|
| `src/daemon/inject/remote/injector.c` / `.h` | `inject_elf()`: attach, load, bootstrap stager, detach |
| `third_party/NineS/pt.c` / `pt.h` | ptrace primitives (`pt_*`); attach and stager waits have timeouts |
| `third_party/NineS/elfldr.c` / `elfldr.h` | ELF loader into the target (`elfldr_load`, `elfldr_payload_args`) |
| `third_party/NineS/elfldr_log.h` | logging helpers for the loader |

`inject_elf()` holds process-wide state while it runs (the attach and
the elevated authid), so it is called under `inject/lock.hpp`.

### The stager

The bootstrap `stager` is compiled code that is copied into the target: it
calls the target's `pthread_create(elf_main)` and stops on `int3`. Its bytes
depend on the flags of the `orange_remote` target and on the order of the
`.stager_shellcode$1` / `$2` sections, and the stager reads the argument block
by offset. After touching `injector.c` or those flags, compare the stager bytes
(`llvm-objcopy --dump-section '.stager_shellcode$1'=out.bin injector.c.o`)
against a build from before the change.

### Authid policy

**Do not** flip ucred authid around every `ptrace` syscall. Elevate **once** for
the inject / attach window with `set_ucred_to_ptrace()` (`PTRACE_AUTHID
0x4800000000010003`, Sony's SceTracer-style id). **`DEBUG_AUTHID` (`...0006`) is
not accepted for `PT_*`**: using it causes attach / `waitpid` failures (errno
`ECHILD`). Restore the previous authid after the inject window (see
`inject_elf`).

