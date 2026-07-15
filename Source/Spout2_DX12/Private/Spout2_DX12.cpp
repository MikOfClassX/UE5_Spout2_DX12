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

// Spout2_DX12 module implementation.
// Unreal calls this by itself when the plugin loads, right after the
// engine starts (LoadingPhase = "Default" in Spout2_DX12.uplugin). We
// never call this function ourselves.
//
// How the stop-file channel works (plugin side):
//   - StartupModule() adds CheckStopFile() to the core ticker. Once per
//     second, it checks for the file "%TEMP%\<ProjectName>_SpoutStop"
//     (for example "SunTemple_SpoutStop"). The core ticker runs in the
//     main loop even with no window, so this works in headless
//     sessions too, where there is no window to close.
//   - As soon as it finds the file, it CONSUMES it: it deletes the
//     file right away (this is a one-time command — if we did not
//     delete it, it would also close the next session, one second
//     after it starts) and then calls RequestEngineExit().
//   - RequestEngineExit() is Unreal's clean, normal way to close: it
//     finishes the current frame, shuts the world down, and runs
//     EndPlay -> StopBroadcastInternal -> ReleaseSender, which removes
//     the sender from shared memory. This takes about 1.5 seconds.
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
    // STOP FILE CHANNEL for sessions with no window (-RenderOffScreen).
    // Full explanation is in Spout2_DX12.h, next to CheckStopFile().
    //
    // The file path is "<TEMP>\<ProjectName>_SpoutStop":
    //  - UserTempDir() is the same %TEMP% folder the stop .bat writes
    //    to, so both sides always agree on the path;
    //  - FApp::GetProjectName() makes the name specific to this project
    //    (here: "VirtualStudio_SpoutStop"), so it works the same way in
    //    any project, and two different games will not clash.
    StopFilePath = FPaths::Combine(
        FString(FPlatformProcess::UserTempDir()),
        FString::Printf(TEXT("%s_SpoutStop"), FApp::GetProjectName()));

    // Delete any OLD stop file left from a previous session (for example,
    // if the stop .bat ran while the game was not running). If we don't
    // do this, a new run would see the old file and shut down by itself
    // about 1 second after start. The last argument (Quiet=true) means:
    // do nothing quietly if the file is not there.
    IFileManager::Get().Delete(*StopFilePath, false, false, true);

    // Check for the stop file once per second, using the engine's core
    // ticker. This ticker keeps running even with no window, so it also
    // works in headless mode. CreateRaw is safe here because we remove
    // this ticker in ShutdownModule before the module is destroyed.
    StopFileTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateRaw(this, &FSpout2_DX12Module::CheckStopFile),
        1.0f);
#endif
}

// Called once per second by the core ticker (see StartupModule). This
// function checks for the stop file created by Ferma_Headless.bat, to
// close a windowless session the clean way.
bool FSpout2_DX12Module::CheckStopFile(float DeltaTime)
{
#if PLATFORM_WINDOWS
    // Two checks before we do anything:
    //  - GIsEditor: never do this inside the editor. A file in %TEMP%
    //    must never be able to close the editor while you work. This
    //    feature is only for packaged/game sessions.
    //  - IsEngineExitRequested(): if we are already closing (started by
    //    us or something else), do not ask again.
    if (GIsEditor || IsEngineExitRequested())
    {
        return true;
    }

    if (!StopFilePath.IsEmpty() && FPaths::FileExists(StopFilePath))
    {
        // Delete the file first (it is a one-time command). If we leave
        // it on disk, it would also close the NEXT session, right after
        // it starts.
        IFileManager::Get().Delete(*StopFilePath, false, false, true);
        UE_LOG(LogSpoutSender, Display, TEXT("Stop file detected at '%s'. Requesting clean engine exit so Spout senders unregister properly."), *StopFilePath);

        // RequestEngineExit is Unreal's normal, clean way to close: it
        // finishes the current frame, shuts the world down properly
        // (EndPlay -> StopBroadcastInternal), and removes the Spout
        // sender from shared memory. A forced kill (taskkill /F) cannot
        // do this, and that is why it used to leave a "ghost" sender
        // behind. This clean exit takes about 1.5 seconds.
        RequestEngineExit(TEXT("Spout2_DX12 stop file"));
    }
#endif
    // Keep this ticker running. Returning false would stop it.
    return true;
}

// Spout2_DX12 module shutdown.
// Unreal calls this by itself when the plugin unloads — this happens
// when the engine closes, or when the plugin module is unloaded during
// a hot-reload in the editor. We never call this function ourselves.
void FSpout2_DX12Module::ShutdownModule()
{
#if PLATFORM_WINDOWS
    // Stop checking for the stop file BEFORE this module is destroyed.
    // We used CreateRaw to set it up, which does not track object
    // lifetime, so a tick after shutdown would touch freed memory.
    if (StopFileTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(StopFileTickerHandle);
        StopFileTickerHandle.Reset();
    }

    // We do NOT unload SpoutDX12.dll here on purpose (the old plugin
    // code did: FPlatformProcess::FreeDllHandle(MyDllHandle)). In
    // Shipping builds, some code can still call into the DLL late
    // during shutdown, after it would already be unloaded — this caused
    // a crash. Leaving the DLL loaded is safe: the OS frees it anyway
    // when the process closes.
    MyDllHandle = nullptr;
#endif
}

#undef LOCTEXT_NAMESPACE
IMPLEMENT_MODULE(FSpout2_DX12Module, Spout2_DX12)
