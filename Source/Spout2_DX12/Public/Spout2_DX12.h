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
	// Problem: a windowless process has no "gentle" way to be closed from
	// the outside (no window, so no WM_CLOSE). Killing it with taskkill /F
	// does not unregister the Spout senders: if a receiver (LiveBoard,
	// OBS...) is still open, it keeps the "ghost" sender alive in Spout's
	// shared memory, so on the next launch the new sender gets renamed to
	// "Sender Component_1", "_2"... and receivers bound to the fixed name
	// lose the stream.
	//
	// Solution: a file-based command channel. An external tool
	// (Ferma_Headless.bat) creates the file "<TEMP>\<ProjectName>_SpoutStop";
	// CheckStopFile, registered on the core ticker with a 1 s period,
	// detects it, deletes it and calls RequestEngineExit(): the engine runs
	// its orderly shutdown (component EndPlay -> StopBroadcast -> Spout
	// sender release) and no ghost is left behind. Always returns true,
	// meaning "keep ticking me".
	// ------------------------------------------------------------------
	bool CheckStopFile(float DeltaTime);

	// Handle of SpoutDX12.dll loaded in StartupModule. NOTE: it is no
	// longer freed in ShutdownModule (see the comment there); the pointer
	// is only kept as a record that the load succeeded.
	void* MyDllHandle = nullptr;

	// Full path of the stop file ("%TEMP%\VirtualStudio_SpoutStop" for this
	// project). Computed once in StartupModule.
	FString StopFilePath;

	// Ticker handle: needed to unregister CheckStopFile in ShutdownModule,
	// otherwise the engine would call into a module that has already been
	// destroyed (crash on exit).
	FTSTicker::FDelegateHandle StopFileTickerHandle;
};
