# OrangeHUD: rules for Claude Code

`docs/` explains how the program is built and what was learned about the
console; `docs/README.md` is the index and `docs/arch.md` the architecture
overview. Read the part you are touching before changing it, and keep it true:
when code changes, change the text that describes it in the same change.

Everything in the repo is in English: code, identifiers, comments, tests,
docs, log messages, commit messages.

## Layout and layers

- One payload, two ELFs: the daemon (`src/daemon/`, the shipped
  `OrangeHUD.elf`, which embeds the other one) and the module injected into
  SceShellUI (`src/shellui/`). They talk only through `src/shared/contract/`
  (the seqlock frame, `/system_tmp` paths).
- `contract/` is an ABI: both sides are built from the same header in the same
  payload, so both may change together — but `test_hud_frame` locks the frame
  layout, and `ORANGE_HUD_ABI` decides whether a running module must be
  replaced (console restart).
- `shellui` must not include daemon headers, and the daemon must not include
  `shellui` headers. Code more than one ELF uses goes to a library in
  `src/shared/` (`contract`, `platform`, `proc`, `detour`); a library only one
  ELF uses lives in that ELF's folder (`daemon/fps`, `daemon/settings`).
- Who may include whom (a module always may include itself): `contract` and
  `platform` each only the other; `proc` and `settings` only `platform` and
  `contract` respectively; `detour` `platform` and `hde64`; `fps` only
  `platform`; the daemon all of those except `detour`; `shellui` `contract`,
  `platform`, `proc` and `detour`. The two ELFs never include each
  other. Nothing checks this: keep to it by hand.
- Pure vs console-facing: every module keeps its testable half free of Sony
  APIs and Mono (`panel_scene.hpp`, `text.hpp`, `frame_stats.hpp`, ...).
  The host tests cover only that half.
- Never cache Mono widget pointers across frames: PUI disposes widgets on
  scene changes, so every per-frame access re-resolves by name
  (`FindWidgetByName`). A stale pointer is a crash (it happened).
- Only send the elf to the console
  (`nc --send-only <console-ip> 9021 < build/ps5-release/bin/OrangeHUD.elf`)
  when asked. After a payload change the console usually asks for a restart
  ("OrangeHUD was updated"); the user handles it.

## Includes

Groups in this order, alphabetical inside each, one blank line between:
standard library, SDK and system headers (`<ps5/...>`, `<sys/...>`,
`<unistd.h>`), third party, this project. Everything of this project uses
quotes (`"platform/log.h"`), the rest uses angle brackets. A file's own header
stays first. Each group has a comment above it (`// Standard library`,
`// SDK`, `// Third party`, `// Project`). `.clang-format` sorts inside a group;
put a new include in the right group yourself, since it does not move across
the comments.

## Keep it simple

- An interface exists only where a test fakes something it cannot run (the
  console, Mono, the game). Otherwise use a plain function or a concrete
  class.
- Prefer early returns. Do not add options, wrappers or helpers for a single
  caller, and do not add error handling for cases that cannot happen.
- No personal data in code, comments, tests, docs, logs or commit messages:
  no real names, e-mail addresses, home directory paths, user names or the
  addresses of someone's own network. Use made-up values
  (`192.168.1.20`).
- Comments follow the surrounding code: short, plain English, explaining why
  or what the bytes mean, not narrating the code.

## License

The repo's code is GPL-3.0-or-later (`LICENSE` at the root); do not add
per-file license headers to it. `third_party/` is not ours and keeps its own
notices; do not reformat or "clean up" those files.

## Build and test

- Host tests: `sh test/run.sh plain` (also `address`, `thread`); presets
  `cmake --preset debug|asan|tsan`, build `cmake --build --preset ...`,
  run `ctest --preset ...`. Host tests compile no PS5 SDK code; the console
  build and the console itself are the only check for the rest — say so when a
  change touches it and has not been run on the console.
- Payload build: `export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk &&
  ninja -C build/ps5-release` (presets `ps5-release` / `ps5-debug`), output
  `build/ps5-release/bin/OrangeHUD.elf` (the daemon; it embeds the module).

## Git

- Commit messages are in English: an imperative subject line without a
  trailing period, then a short body saying what and why when it is not
  obvious.
- No `Co-Authored-By` line and no "Generated with Claude Code" line in commit
  messages.
- Work lands on `main`. Use short-lived branches for experiments; the user
  decides when a body of work becomes a commit and whether it lands as one
  commit or several.

## Reporting

- Say what was verified (host tests, PS5 build, run on the console) and what
  was not. Do not claim a console behaviour that was not run.
