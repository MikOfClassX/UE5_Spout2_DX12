# Build Instructions

How to compile the `Spout2_DX12` plugin from the command line and where to
copy the result so it works in **any** Unreal Engine 5 project (not just
the one it was built against).

---

## 1. Prerequisites

- Windows, with **Unreal Engine 5.8** installed (default path
  `C:\Program Files\Epic Games\UE_5.8`). A different install path can be
  passed as an argument (see below).
- Visual Studio 2022 with the "Game development with C++" workload
  (the same toolchain the Engine itself uses to build).
- DirectX 12 capable GPU/driver (runtime requirement of the plugin, not
  of the build itself).

No CMake, no external build system: Unreal plugins are compiled by
**UnrealBuildTool (UBT)**, invoked here through `RunUAT.bat BuildPlugin` —
the same command Epic uses to produce the binaries distributed on the
Marketplace.

---

## 2. Compiling

From the repository root, run:

```bat
Build.bat [EngineDir] [OutputDir]
```

Both arguments are optional:

| Argument    | Default                                  | Purpose                          |
|-------------|-------------------------------------------|-----------------------------------|
| `EngineDir` | `C:\Program Files\Epic Games\UE_5.8`      | Root of the Unreal Engine install to build against |
| `OutputDir` | `.\Build\Spout2_DX12`                     | Where the compiled, packaged plugin is written |

Example, using the defaults:

```bat
Build.bat
```

Example, keeping the default engine but writing the output elsewhere:

```bat
Build.bat "" "E:\PluginOutput\Spout2_DX12"
```

Example, targeting a different engine install (only if UE is **not**
installed at the default path above — check first, e.g. with
`dir "C:\Program Files\Epic Games"`, before passing a custom one):

```bat
Build.bat "D:\Epic Games\UE_5.8" "E:\PluginOutput\Spout2_DX12"
```

The script compiles three configurations for Win64 (`Editor`,
`Game Development`, `Game Shipping`) via `RunUAT.bat BuildPlugin`. A clean
run takes roughly 2 minutes. On success it prints:

```
BUILD SUCCESSFUL
[OK] Plugin built successfully. Output in "...".
```

---

## 3. Where the compiled plugin ends up

`BuildPlugin` produces a **self-contained, redistributable plugin folder**
at `OutputDir` (default: `Build\Spout2_DX12\`), laid out exactly like a
plugin that goes inside a project's `Plugins/` directory:

```
Build\Spout2_DX12\
├── Spout2_DX12.uplugin        <- plugin descriptor
├── Binaries\
│   └── Win64\
│       ├── UnrealEditor-Spout2_DX12.dll   <- compiled module (Editor)
│       ├── UnrealEditor-Spout2_DX12.pdb
│       ├── UnrealEditor.modules
│       ├── Spout.dll                       <- Spout runtime (staged)
│       └── SpoutDX12.dll                   <- Spout runtime (staged)
├── Source\                     <- plugin source (kept so the Editor
│                                   can rebuild for other engine versions)
├── Resources\
│   └── Icon128.png
└── Intermediate\                <- build cache, NOT needed for distribution
```

Only `Intermediate\` can be discarded; everything else is part of the
plugin.

> Note: `BuildPlugin` only compiles the **Editor** binaries into this
> folder's `Binaries\Win64\` (used when the plugin is loaded inside the
> Unreal Editor / in PIE). The `Game Development` and `Game Shipping`
> configurations are compiled and verified as part of the build (to catch
> compile errors in all configurations) but their binaries are consumed
> automatically by the Engine's cooker at packaging time — a game project
> that ships a packaged build recompiles/relinks the plugin for that
> target itself, it does not need those intermediate binaries copied
> anywhere.

---

## 4. Installing the plugin into a project

To make the plugin available to **one specific project**, copy the whole
`Build\Spout2_DX12\` folder (minus `Intermediate\`, optional) into that
project's `Plugins\` directory:

```
YourProject\Plugins\Spout2_DX12\      <- copy of Build\Spout2_DX12\
```

Then open the project and enable the plugin from
**Edit → Plugins → Spout2_DX12** if it isn't enabled automatically.

### Making it available to ALL projects (engine-wide install)

To avoid copying it into every project individually, install it once at
the **Engine level** instead, so every project using that Engine install
picks it up automatically:

```
C:\Program Files\Epic Games\UE_5.8\Engine\Plugins\Marketplace\Spout2_DX12\
```

(the `Marketplace` subfolder is a convention for third-party/engine
plugins; any subfolder under `Engine\Plugins\` works). Copy the same
`Build\Spout2_DX12\` contents there. Every `.uproject` opened with that
Engine install will then list `Spout2_DX12` under **Edit → Plugins**
without needing it inside the project's own `Plugins\` folder.

> Requires write access to the Engine install directory — if UE was
> installed via the Epic Games Launcher into `Program Files`, you may
> need administrator rights to copy files there.
