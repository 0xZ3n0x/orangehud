# Troubleshooting

Read the klog stream from a PC to see what the daemon and the module say
(see [build.md](build.md) → Logging).

| Symptom | Check |
|---|---|
| "Failed to inject the HUD into SceShellUI" notification | `kstuff` is not loaded, or SceShellUI was restarting; reload `kstuff`, then resend |
| No HUD, no notification | Read the klog stream |
| HUD shows `--` for FPS | No game running, or the game is not flipping frames yet (loading screens); the HUD also shows `--` when no flip was seen for a second |
| `1% --  0.1% --` next to the FPS | The lows need a 15 s warm-up after the game starts and 1000 frames for the 0.1% low; they also reset when the game changes |
| Config change has no effect | Check `/data/OrangeHUD/config.ini` (the daemon applies it within a second); an invalid file keeps the previous settings |
| "OrangeHUD was updated. Restart the console..." notification | The ShellUI module of an older build is still loaded; restart the console, load `kstuff`, resend |
| A HUD change does not show after resending the payload | The daemon only replaces the ShellUI module when `ORANGE_HUD_ABI` differs; with the same ABI it logs `HUD already active` and keeps the loaded module. Restart the console (or whatever restarts SceShellUI) to load the new one |
| Crash notification | Attach `/data/OrangeHUD/OrangeHUD_crash.log` to an issue |
| `MEM --` / `SoC` shows `--W` | No game is running, or the kernel call failed; check the klog stream |
