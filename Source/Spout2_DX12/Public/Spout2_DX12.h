#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSpoutRX, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogSpoutSender, Log, All);

class SPOUT2_DX12_API FSpout2_DX12Module : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static inline FSpout2_DX12Module& Get()
	{
		return FModuleManager::LoadModuleChecked<FSpout2_DX12Module>("Spout2_DX12");
	}

private:
	// ------------------------------------------------------------------
	// CLEAN STOP FOR HEADLESS SESSIONS (-RenderOffScreen)
	//
	// Problem: a process with no window has no easy way to be closed
	// from the outside (no window means no "close" message). Killing
	// it by force (taskkill /F) does not remove the Spout senders: if
	// a receiver (LiveBoard, OBS...) is still open, it keeps a "ghost"
	// sender alive. Then, next time the app starts, the new sender
	// gets a different name ("Sender Component_1", "_2"...), and
	// receivers looking for the original name lose the stream.
	//
	// Fix: a simple file-based signal. An outside tool
	// (Ferma_Headless.bat) creates the file "<TEMP>\<ProjectName>_SpoutStop".
	// CheckStopFile runs once per second and, when it finds the file,
	// deletes it and calls RequestEngineExit(): this closes the engine
	// the normal, clean way (EndPlay -> StopBroadcast -> Spout sender
	// removed), so no ghost sender is left. Always returns true, which
	// means "keep checking every second".
	// ------------------------------------------------------------------
	bool CheckStopFile(float DeltaTime);

	// Handle for SpoutDX12.dll, loaded in StartupModule. Note: we no
	// longer unload it in ShutdownModule (see the comment there). We
	// only keep this pointer as proof that loading it worked.
	void* MyDllHandle = nullptr;

	// Full path of the stop file (for example
	// "%TEMP%\VirtualStudio_SpoutStop"). Set once in StartupModule.
	FString StopFilePath;

	// Ticker handle. We need this to stop CheckStopFile in
	// ShutdownModule — otherwise the engine could call a function on a
	// module that no longer exists, which would crash on exit.
	FTSTicker::FDelegateHandle StopFileTickerHandle;
};
