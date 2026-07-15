# Changelog

## [Unreleased] - 2026-07-07

Changes ported from the working branch in `VirtualStudio 5.8 5.8 - 2` (based on `37e714e`).

### Fixed
- **GPU freeze in headless sessions (`-RenderOffScreen`)**: when running with no window, opening the Spout bridge too early (while the engine was still rendering its first frames or compiling shaders) could freeze the GPU (`DXGI_ERROR_DEVICE_HUNG`). This happened because with no window, nothing was pacing the GPU queue. Fix: both the sender and the receiver now wait before opening the bridge, until both are true:
  1. At least `StartDelaySeconds` of game time has passed (default 2.0s, set to 0 to disable this wait);
  2. The engine has finished compiling pending shaders.
  A safety limit, `MaxStartDelaySeconds` (default 15.0s), makes sure the wait never lasts forever.
  - Files: `SpoutSenderComponent.cpp/h`, `SpoutReceiverComponent.cpp/h`.

- **Crash on shutdown in Shipping builds**: `ShutdownModule` no longer unloads `SpoutDX12.dll` by hand. In Shipping builds, some code could still call into the DLL right as the app was closing, after the DLL was already unloaded — causing a crash. Now the DLL is simply left loaded; the OS cleans it up when the process exits anyway.
  - File: `Spout2_DX12.cpp`.

- **Tearing and duplicate frames on the receiver side**: the receiver could sometimes read a frame while the sender was still writing it, causing visual glitches. 
Fix: added proper synchronization (a lock plus a frame counter) so the receiver waits for a complete frame instead of reading a half-written one. 
This synchronization state resets whenever the sender name changes, and is cleared when the receiver stops.
  - File: `SpoutReceiverComponent.cpp/h`.

### Added
- **A safe way to close headless sessions from outside**: a windowless process has no normal way to be closed cleanly (there's no window to send a "close" message to). Force-killing it (`taskkill /F`) skipped Spout's cleanup, leaving a "ghost" sender behind — 
which then forced the next run to use a renamed sender (`_1`, `_2`, ...), breaking any receiver expecting the original name. Fix: added a simple file-based signal. An external tool (e.g. `Ferma_Headless.bat`) creates a marker file; the plugin checks for it once per second, and when found, shuts the app down cleanly (including releasing the Spout sender). 
This only runs outside the Editor.
  - File: `Spout2_DX12.cpp/h`.

- **`Build.bat`**: a script to compile the plugin from the command line, without opening the Unreal Editor. Tested successfully against UE 5.8 (build succeeds in about 2 minutes).
- **`BUILD_INSTRUCTIONS.md`**: a guide covering requirements, how to run `Build.bat`, what the compiled output looks like, and how to install the plugin — either into one project's `Plugins/` folder, or engine-wide so every project using that Engine install can see it.
