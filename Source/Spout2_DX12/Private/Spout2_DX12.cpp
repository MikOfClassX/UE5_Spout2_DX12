#include "Spout2_DX12.h"
#include "Interfaces/IPluginManager.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RHI.h"
#include "Misc/MessageDialog.h"
#include "Spout2BlueprintLibrary.h"
#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include "RenderingThread.h"
#include "Modules/ModuleManager.h"
#include "CoreMinimal.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "TextureResource.h"

#if PLATFORM_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Windows/WindowsHWrapper.h"

THIRD_PARTY_INCLUDES_START
#include "Windows/AllowWindowsPlatformTypes.h"

#include <d3d11.h>
#include <d3d12.h>
#include <d3d11on12.h>
#include <dxgi.h>

// include Spout only *here*, never above
#include "SpoutDX12.h"

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
#ifdef IsMinimized
#undef IsMinimized
#endif
#ifdef GetNextSibling
#undef GetNextSibling
#endif

#include "Windows/HideWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_END
#endif

#define LOCTEXT_NAMESPACE "FSpout2_DX12Module"
DEFINE_LOG_CATEGORY(LogSpoutRX);
DEFINE_LOG_CATEGORY(LogSpoutSender);

// Spout2_DX12 module implementation
void FSpout2_DX12Module::StartupModule()
{
#if PLATFORM_WINDOWS
    TArray<FString> Dirs;
    if (const TSharedPtr<IPlugin> P = IPluginManager::Get().FindPlugin(TEXT("Spout2_DX12")))
        Dirs.Add(FPaths::Combine(P->GetBaseDir(), TEXT("Binaries/Win64")));                // plugin binaries (Editor)
    Dirs.Add(FPaths::Combine(FPaths::ProjectDir(), TEXT("Binaries/Win64")));               // project binaries (Editor)
    Dirs.Add(FPlatformProcess::BaseDir());

    // Try known folders one by one.
    // Always pop after push so we do not leave extra DLL dirs on the process stack.
    for (const FString& Dir : Dirs) {
        FPlatformProcess::PushDllDirectory(*Dir);
        void* TryHandle = FPlatformProcess::GetDllHandle(TEXT("SpoutDX12.dll"));
        FPlatformProcess::PopDllDirectory(*Dir);

        if (TryHandle)
        {
            MyDllHandle = TryHandle;
            break;
        }
    }
    if (MyDllHandle == nullptr)
    {
        // Handle error: DLL failed to load
        UE_LOG(LogTemp, Error, TEXT("Failed to load SpoutDX12.dll"));
    }
    else
    {
        // Success: DLL is loaded and ready for use
        UE_LOG(LogTemp, Log, TEXT("Successfully loaded SpoutDX12.dll"));
    }

    // ------------------------------------------------------------------
    // Graceful-stop channel for windowless sessions (-RenderOffScreen).
    // See the full rationale in Spout2_DX12.h next to CheckStopFile().
    //
    // The path is "<TEMP>\<ProjectName>_SpoutStop":
    //  - UserTempDir() is the same %TEMP% the stop .bat writes into with
    //    'type nul > "%TEMP%\..."', so the two sides always agree;
    //  - FApp::GetProjectName() keeps the name project-specific (here:
    //    "VirtualStudio_SpoutStop"), so the plugin works unchanged in any
    //    other project and two different games can coexist.
    StopFilePath = FPaths::Combine(
        FString(FPlatformProcess::UserTempDir()),
        FString::Printf(TEXT("%s_SpoutStop"), FApp::GetProjectName()));

    // Delete any STALE stop file left over from a previous session (e.g.
    // the stop .bat was run while the game was not running). Without this,
    // a new run would see the old file and shut itself down ~1 s after
    // startup. Args: (path, RequireExists=false, EvenReadOnly=false,
    // Quiet=true) -> silent no-op when the file is not there.
    IFileManager::Get().Delete(*StopFilePath, false, false, true);

    // Poll for the stop file once per second on the engine's core ticker.
    // The core ticker runs in the main loop even with no viewport/window,
    // so it works in headless sessions. CreateRaw is safe here because the
    // ticker is removed in ShutdownModule before this object goes away.
    StopFileTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateRaw(this, &FSpout2_DX12Module::CheckStopFile),
        1.0f);
#endif
}

// Called by the core ticker once per second (see StartupModule). This is
// the consumer side of the file-based stop channel used by
// Ferma_Headless.bat to close a windowless session cleanly.
bool FSpout2_DX12Module::CheckStopFile(float DeltaTime)
{
#if PLATFORM_WINDOWS
    // Two guards:
    //  - GIsEditor: never react inside the editor. A file dropped in %TEMP%
    //    must not be able to close the editor while you are working; this
    //    channel only makes sense for packaged/game sessions.
    //  - IsEngineExitRequested(): the exit is already in progress (from us
    //    or anyone else), do not request it again.
    if (GIsEditor || IsEngineExitRequested())
    {
        return true;
    }

    if (!StopFilePath.IsEmpty() && FPaths::FileExists(StopFilePath))
    {
        // Consume the file first (one-shot command): if it stayed on disk
        // it would kill the NEXT session too, right after startup.
        IFileManager::Get().Delete(*StopFilePath, false, false, true);
        UE_LOG(LogSpoutSender, Display, TEXT("Stop file detected at '%s'. Requesting clean engine exit so Spout senders unregister properly."), *StopFilePath);

        // RequestEngineExit is Unreal's ORDERLY exit: it lets the frame
        // finish, tears the world down (EndPlay -> StopBroadcastInternal),
        // and releases the Spout sender from shared memory. This is what a
        // taskkill /F cannot guarantee, and it is why no ghost sender is
        // left behind for LiveBoard/OBS. Takes ~1.5 s in practice.
        RequestEngineExit(TEXT("Spout2_DX12 stop file"));
    }
#endif
    // Always keep the ticker alive; returning false would unregister it.
    return true;
}

// Spout2_DX12 module shutdown
void FSpout2_DX12Module::ShutdownModule()
{
#if PLATFORM_WINDOWS
    // Unregister the stop-file poller BEFORE this module object dies: the
    // delegate was bound with CreateRaw (no lifetime tracking), so a tick
    // fired after shutdown would call into freed memory.
    if (StopFileTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(StopFileTickerHandle);
        StopFileTickerHandle.Reset();
    }

    // Intentionally do NOT free the SpoutDX12.dll handle here (the original
    // plugin did: FPlatformProcess::FreeDllHandle(MyDllHandle)). In
    // monolithic Shipping builds, late static destructors can still call
    // into the DLL after module shutdown; unloading it here caused an
    // access violation during process teardown. Keeping it loaded is
    // harmless: the OS reclaims the DLL when the process exits anyway.
    MyDllHandle = nullptr;
#endif
}

#undef LOCTEXT_NAMESPACE
IMPLEMENT_MODULE(FSpout2_DX12Module, Spout2_DX12)
