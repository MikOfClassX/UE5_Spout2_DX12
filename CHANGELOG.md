# Changelog

## [Unreleased] - 2026-07-07

Changes ported from the working branch in `VirtualStudio 5.8 5.8 - 2` (based on `37e714e`).

### Fixed
- **GPU hang in headless sessions (`-RenderOffScreen`)**: opening the D3D11On12 bridge while the engine is still rendering its first frames or compiling PSOs could deadlock the D3D12 queue (`DXGI_ERROR_DEVICE_HUNG` at Frame 0), since without a swapchain no `Present` paces the queue. Added a "deferred start" to both the sender and the receiver: the actual bridge open is held back until both conditions are true:
  1. `StartDelaySeconds` of world time have elapsed (default 2.0s, 0 = disable);
  2. PSO precaching is idle (`PipelineStateCache` + `FShaderPipelineCache` have no pending requests).
  `MaxStartDelaySeconds` (default 15.0s) acts as a safety valve so the start can never be held back indefinitely.
  - Files: `SpoutSenderComponent.cpp/h`, `SpoutReceiverComponent.cpp/h`.

- **Shipping shutdown crash**: `ShutdownModule` no longer frees `SpoutDX12.dll` with `FreeDllHandle`. In monolithic Shipping builds, late static destructors could still call into the DLL after it was unloaded, causing an access violation on exit. The DLL stays loaded (the OS reclaims it anyway when the process exits).
  - File: `Spout2_DX12.cpp`.

- **Link/runtime crash from `Spout.lib`/`Spout.dll`**: `SpoutDX12.dll` already statically embeds the `spoutDX`/`spoutFrameCount` code, so also linking `Spout.lib` made the linker resolve `spoutFrameCount` methods to `Spout.dll` (delay-loaded), which is never actually loaded at runtime by `SpoutDX12.dll` → crash. Removed the link to `Spout.lib` and the delay-load of `Spout.dll`; only `SpoutDX12.lib`/`SpoutDX12.dll` remain.
  - File: `Spout2_DX12.Build.cs`.

- **Tearing/duplicate frames in the receiver**: added synchronization via Spout's access mutex and frame count (`CreateAccessMutex`/`EnableFrameCount`, `GetNewFrame`, `CheckAccess`/`AllowAccess`) before copying the sender's shared texture, so frames not yet ready are skipped instead of read mid-write. Sync state is reinitialized when the sender name changes and cleared in `StopReceivingInternal`/`ReleaseSpoutDevices`.
  - File: `SpoutReceiverComponent.cpp/h`.

### Added
- **Graceful stop channel for headless sessions**: a windowless process has no clean way to be closed from the outside (no `WM_CLOSE`); a `taskkill /F` does not unregister the Spout sender, leaving a "ghost sender" in shared memory that forces the next sender to be renamed (`Sender Component_1`, `_2`, ...), breaking receivers bound to the fixed name. Added a file-based channel: an external tool (e.g. `Ferma_Headless.bat`) creates the file `%TEMP%\<ProjectName>_SpoutStop`; a ticker registered on the core ticker (1s) detects it, deletes it, and calls `RequestEngineExit()` for an orderly shutdown (EndPlay → StopBroadcast → Spout sender release). Active only outside the editor (`GIsEditor` excludes the editor).
  - File: `Spout2_DX12.cpp/h`.

### Added
- **`Build.bat`**: command-line build script wrapping `RunUAT.bat BuildPlugin`, so the plugin can be compiled standalone (Editor, Game Development, Game Shipping, Win64) without opening the Editor. Verified end-to-end against UE 5.8 (`BUILD SUCCESSFUL`, ~2 min).
- **`BUILD_INSTRUCTIONS.md`**: documents prerequisites, how to run `Build.bat`, the layout of the compiled output folder, and how to install the plugin either into a single project's `Plugins/` folder or engine-wide (`Engine/Plugins/Marketplace/`) so it's available to every project using that Engine install.

### Notes
- The prebuilt binaries (`Binaries/Win64/*.dll`, `.pdb`) were **not** updated along with the source: they need to be rebuilt in this environment before the next commit/use (`Build.bat` handles this).
