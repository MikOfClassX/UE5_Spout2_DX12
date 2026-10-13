<img width="268" height="269" alt="logo" src="https://github.com/user-attachments/assets/8afeb86f-cbe7-400b-8002-252ed8121854" />

# UE5_Spout2_DX12 v2.1.1

Spout2 plugin for Unreal Engine 5 using DirectX 12.

This plugin adds Spout sender and receiver components for Unreal Engine. It works in the editor, in PIE, and in packaged builds based on the startup policy you choose.

Discord Server:
**https://discord.gg/BxHRHDdkNy**

Tested on Windows with DX12:
- UE 5.2.1
- UE 5.4.4
- UE 5.6.1
- UE 5.7.2
- UE 5.8 Preview
- UE 5.8

[**You can donate to support the project here.**](https://www.paypal.com/donate/?hosted_button_id=G3JA94AZGB9T2)

---

## Included Features

- Spout sender component
- Spout receiver component
- Sender support for Render Target, Game Viewport, and Editor Viewport sources
- Receiver support for writing incoming Spout frames into Unreal render targets
- Automatic and manual sender / receiver startup options
- Editor, game, and preview startup policies for controlled auto-start behavior
- Optional sender and receiver double buffering
- Configurable sender and receiver frame rates
- Blueprint and C++ controls for starting, stopping, and changing active sources
- Packaged build DLL staging for the included Spout runtime files

---

## Requirements

- Windows only
- DirectX 12 only

---

## Installation

1. Download the correct build from [Releases](https://github.com/GPUbrainStorm/UE5_Spout2_DX12/releases)
2. Extract it into:

```text
YourProject/Plugins/
```

3. Open the project
4. Enable the plugin if needed

The required DLLs are handled by the plugin. No manual engine copy step is needed.

### Packaged Builds

Package the project normally. The plugin stages the required DLLs next to the packaged executable automatically.

### Building from the Command Line

Run `Build.bat` to compile the plugin standalone, without opening the Editor. `Deploy.bat` installs an existing package into the UE 5.8 Engine Marketplace directory with elevation, validation, backup, and rollback. `BuildAndDeploy.bat` performs the safe clean-package build and deployment in one command. See [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md) for prerequisites and usage.

---

## Sender Component

`SpoutSenderComponent` sends frames from Unreal to any Spout receiver.

### Sender Source Modes

- `RenderTarget`
  Sends the render target assigned to `CurrentRenderTarget`.
- `GameViewport`
  Sends the active game viewport.
- `EditorViewport`
  Sends the active editor viewport.

### Sender Properties

- `Auto_Start`
  Starts the sender automatically when the current world matches the selected startup policy.
- `CurrentSenderName`
  The published Spout sender name.
- `SourceType`
  Selects `RenderTarget`, `GameViewport`, or `EditorViewport`.
- `CurrentRenderTarget`
  Used only when `SourceType` is `RenderTarget`.
- `BroadcastFPS`
  Controls how often frames are sent.
- `bUseDoubleBuffer`
  Enables double buffering for smoother frame pacing with extra memory use.
- `StartupPolicy`
  Controls where the sender is allowed to start automatically.
- `TickAfterActor`
  Optional tick dependency if another actor must update first.

### Sender Startup Policies

- `Editor | Game`
  Allows start in editor and game worlds.
- `Game Only`
  Allows start only in game worlds.
- `Editor | Game | Single Preview`
  Allows start in editor, game, and a single preview world.

### Sender Blueprint / C++ Functions

- `StartBroadcast()`
  Starts using the current sender component settings exactly as configured in the editor.
- `StartBroadcastFromRenderTarget(RenderTarget, SenderName, FPS, bEnableDoubleBuffer)`
  Starts from a render target and forces runtime startup behavior to `Game Only`.
- `StartBroadcastGameViewport(SenderName, FPS, bEnableDoubleBuffer)`
  Starts from the game viewport and forces runtime startup behavior to `Game Only`.
- `StopBroadcast()`
  Stops the sender.
- `ChangeRenderTarget(NewRenderTarget)`
  Changes the render target source.
- `SetTickAfterActor(NewTickAfterActor)`
  Sets the optional tick dependency actor.

### Sender Notes

- `StartBroadcast()` uses the same configured settings path that auto-start uses.
- The runtime override functions are intended for manual runtime control.
- In v2.1.1, the sender opens Spout DX12 using Unreal's native D3D12 device instead of creating an independent DX12 path.
- Sender frame handoff now uses D3D11On12 acquire/release and fence tracking before reusing staging slots.
- Render-target sends expose `GetLastQueuedFrameTicket()`. Its thread-safe ticket sequence identifies the queued frame; `Wait(timeout)` completes after the D3D11 fence inserted after the Spout copy, and `WasSendSuccessful()` reports render-thread send/fence setup success.
- Packaged Game Viewport sender capture uses render-thread-safe stage-slot readiness while preserving D3D11On12 fence tracking.
- The sender bridge is opened only when broadcasting starts, reducing editor and PIE bridge churn.

### Sender Screenshots

![Sender Functions And Settings](SpoutSS/SenderFunctionsAndSettings.png)

![Sender Test In UE](SpoutSS/SenderTestSS_UE.png)

![Sender Test In OBS](SpoutSS/SenderTestSS_OBS.png)

---

## Receiver Component

`SpoutReceiverComponent` receives frames from any Spout sender and writes them into a render target.

### Receiver Properties

- `bAutoStart`
  Starts the receiver automatically when the current world matches the selected startup policy.
- `OutputRenderTarget`
  The render target that receives the incoming frame.
- `TargetFPS`
  Controls how often the receiver tries to pull frames. Set to `0` for one frame then stop.
- `SpoutSenderName`
  The specific sender name to connect to. Leave empty to use the first available sender.
- `bUseDoubleBuffer`
  Uses internal double buffering to keep the output render target stable while new frames are copied in.
- `StartupPolicy`
  Controls where the receiver is allowed to start automatically.

### Receiver Blueprint / C++ Functions

- `StartReceiving()`
  Starts the receiver.
- `StopReceiving()`
  Stops the receiver.
- `GetAvailableSenders()`
  Returns available Spout sender names.
- `IsConnected()`
  Returns the current connection state.

### Receiver Stats

- `CopiesPerSecond`
  Average copy operations per second.
- `Flush1PerSecond`
  Average first-stage flush operations per second.
- `FlushPerSecond`
  Average flush operations per second.
- `ReconnectCount`
  Number of reconnects during the current session.
- `MissedFrames`
  Number of missed frames during the current session.

### Opt-in Receiver Frame Trace

For intermittent repeated/skipped frames, enter this in the Unreal console while the receiver is running:

```text
Spout.RX.TraceSeconds 15
```

Each ticking receiver captures up to 65,536 records in preallocated memory (maximum duration 120 real seconds). It automatically writes a uniquely named CSV into the host project's `Saved/SpoutDiagnostics/` on expiry, capacity, receiver stop, or disable. The `LogSpoutRX` start/end messages identify the component, initial sender/settings, and file. There are no per-frame log messages or disk writes. Allocation/start logging and the final synchronous CSV dump can perturb timing at the capture boundaries; exclude those boundaries when reviewing video.

To capture again, set `Spout.RX.TraceSeconds 0`, allow a receiver tick, then set it positive again. The setting applies to all receivers, including supported editor worlds. It does not change reception rate, buffering, mutex behavior, or GPU synchronization.

CSV events include:

- `Tick`, `ThrottleSkip`, `ReceiveAttempt`, `ReceiveOK`, `ReceiveFailed`: receiver scheduling and overall attempt result. `ReceiveOK` means submission succeeded, not GPU completion.
- `NewFrame`, `NoNewFrame`: the existing `GetNewFrame()` check, followed by the SDK's cached `GetSenderFrame()` value. No extra frame-counter polling is introduced.
- `FirstFrameUncounted`: initial/reconnection copy bypassed that check. Count `-1` also means unavailable/disabled or not yet observed in this trace.
- `MutexAcquired`, `MutexMiss`, `SnapshotQueuedMutexReleased`: shared-source access and asynchronous snapshot submission.
- `CopySubmitted`: signaled copy fence (slot `-1` means direct output, `0`/`1` means internal double-buffer slot).
- `PublishEnqueued`: double-buffer output copy was queued; **not** proof that Unreal's output copy or camera sampling completed. Match its fence to the earlier `CopySubmitted` record; its sender-count column is only the latest observation, not the published slot's pixel identity.
- `SourceSyncReset`: source synchronization was reinitialized; compare counts only within one `source_epoch`.

Other columns record real elapsed time, Unreal `GFrameCounter`, observed D3D11 completed-fence value, tick delta and FPS accumulator. Count deltas greater than one show missed observed publications, not their cause. A new count followed by `MutexMiss` identifies an observed update that was not copied on that attempt. `ThrottleSkip` can be normal when Unreal ticks faster than the receiver's target rate.

The SDK count is not necessarily the video's burned-in counter or a host genlock ID. Keep the continuous visual counter and original recording alongside the CSV and Unreal log. This receiver-only trace does not read shared memory, identify camera consumption, or prove which pixels were copied; GPU-completion and camera-ordering investigations may require additional correlation. Frame counting must be supported/enabled by the Spout sender/SDK for meaningful counts.

### Capture-Driven Reception

A host can call `SetCaptureDrivenReceiving(true)` to suppress autonomous receiver reception, then call the exported game-thread `ReceiveForCapture(TimeoutMilliseconds)` immediately before capture. The default is autonomous reception. The capture-driven path requires a running receiver, enabled Spout frame counting and direct output (no double buffering). It bypasses FPS throttling, retries new-frame/access checks, retains observed updates across access failures, flushes D3D11 submission and waits for receiver-copy fence completion. The 1–100 ms budget bounds polling, not SDK calls/resource initialization. Exit requests break polling. `false` requires the host to skip sampling the destination, since a GPU write may remain pending; the next capture-driven call first waits for that pending fence. Mode/source/resource changes and teardown drain submitted work using a GPU event query (up to 1000 ms polling), including unfenced failed submissions. Failed drain retains resources instead of releasing/reusing them unsafely; destruction on a wedged GPU can retain native allocations. Polling backs off 1 ms; device-removal fence values are failure, not completion.

This does not establish producer GPU completion or exact image/camera matching, and does not redesign shared-source mutex/copy ordering. ClassX integrates this path through persistent **Project Settings → Plugins → ClassX Capture**, enabled by default with a 20 ms polling budget. `ClassX.SpoutCaptureSyncMs` defaults to -1 (follow project settings); 0 disables; positive values override the budget. Only authority Virtual1/AR1 captures with consumed requests wait. Its timeout skips capture but the existing output sender can still acknowledge prior output. Do not treat this as a new fresh-input protocol guarantee.

Source changes and stop/restart disconnect the frame counter with `CleanupFrameCount()`, not `DisableFrameCount()`: the latter latches SDK counting off and `EnableFrameCount()` does not undo it. The `Spout.Receiver.Sync.FrameCountLifecycle` test exercises counter retargeting/reopen and the Disable/Enable pitfall without changing registry settings.

Diagnostic CSV adds `CaptureWaitBegin`, `CaptureReady`, `CaptureNewFrameTimeout`, `CaptureGpuTimeout`, `CaptureReceiveError`, `CaptureFrameCountUnavailable`, and `CaptureDeviceRemoved`. Retry `NoNewFrame` checks within the same Unreal frame are expected in this mode. Capture-ready means receiver fence completion, not producer completion or pixel identity.

### Receiver Screenshots

![Receiver Functions And Settings](SpoutSS/ReceiverFunctionsAndSettings.png)

![Receiver Test](SpoutSS/ReceiverTestSS.png)


---

## Quick Start

### Send From Render Target

1. Add a `SpoutSenderComponent` to an actor
2. Set `SourceType` to `RenderTarget`
3. Assign `CurrentRenderTarget`
4. Set `CurrentSenderName`
5. Enable `Auto_Start` or call `StartBroadcast()`

### Send From Game Viewport

1. Add a `SpoutSenderComponent`
2. Set `SourceType` to `GameViewport`
3. Set `CurrentSenderName`
4. Enable `Auto_Start` or call `StartBroadcast()`

### Send From Editor Viewport

1. Add a `SpoutSenderComponent`
2. Set `SourceType` to `EditorViewport`
3. Set `CurrentSenderName`
4. Choose `Editor | Game` or `Editor | Game | Single Preview`
5. Enable `Auto_Start` or call `StartBroadcast()`

### Receive Into a Render Target

1. Add a `SpoutReceiverComponent`
2. Assign `OutputRenderTarget`
3. Set `SpoutSenderName` if needed
4. Enable `bAutoStart` or call `StartReceiving()`

---

## Version

Current release: `v2.1.1`

---

## Support

If you hit an issue, open a GitHub issue and include:

- Unreal Engine version
- whether the issue happens in editor, PIE, or packaged build
- whether you are using sender or receiver
- logs or screenshots if possible

Or join the Discord server mentioned in the beginning and ask there.
