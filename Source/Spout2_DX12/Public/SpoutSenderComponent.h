#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/TimerHandle.h"
#include "RHIResources.h"
#include "RenderCommandFence.h"
#include "SpoutSenderSource.h"
#include "SpoutWorldPolicy.h"
#include "SpoutSenderComponent.generated.h"

struct ID3D11Device5;
struct ID3D11DeviceContext4;
struct ID3D11Fence;
struct ID3D11On12Device;
struct ID3D11Resource;
struct ID3D12Device;

class spoutDX;
class spoutDX12;
class AActor;
class FViewport;
class SWindow;

#if WITH_EDITOR
struct FPropertyChangedEvent;
#endif

UCLASS(ClassGroup = (Spout), meta = (BlueprintSpawnableComponent))
class SPOUT2_DX12_API USpoutSenderComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    USpoutSenderComponent();

	void UpdateTexture();

    UFUNCTION(BlueprintCallable, Category = "Spout")
    void StartBroadcastFromRenderTarget(
        UTextureRenderTarget2D* RenderTarget,
        const FString& SenderName = "Sender Component",
        int32 FPS = 60,
        bool bEnableDoubleBuffer = false);

    UFUNCTION(BlueprintCallable, Category = "Spout")
    void StartBroadcastGameViewport(
        const FString& SenderName = "Sender Component",
        int32 FPS = 60,
        bool bEnableDoubleBuffer = false);

    UFUNCTION(BlueprintCallable, Category = "Spout")
    void StartBroadcast();

    UFUNCTION(BlueprintCallable, Category = "Spout")
    void StopBroadcast();

    UFUNCTION(BlueprintCallable, Category = "Spout")
    void ChangeRenderTarget(UTextureRenderTarget2D* NewRenderTarget);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spout", meta = (ToolTip = "If enabled, the sender starts automatically when the component becomes active in a supported world."))
    bool Auto_Start = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spout", meta = (ToolTip = "The published Spout sender name. Receivers use this name to find and connect to this sender."))
    FString CurrentSenderName = "Broadcast Component";
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spout", meta = (ToolTip = "Selects which Unreal source this component sends to Spout: a render target, the game viewport, or the editor viewport."))
    ESpoutSenderSourceType SourceType = ESpoutSenderSourceType::RenderTarget;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spout", meta = (EditCondition = "SourceType == ESpoutSenderSourceType::RenderTarget", EditConditionHides, ToolTip = "The render target to send when Source Type is set to Render Target."))
    UTextureRenderTarget2D* CurrentRenderTarget = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spout", meta = (ToolTip = "How often the sender pushes frames. Set to 0 to disable throttling and tick every frame."))
    int32 BroadcastFPS = 60;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spout", meta = (ToolTip = "Uses two staging buffers instead of one. This can improve frame pacing at the cost of more GPU memory and latency."))
    bool bUseDoubleBuffer = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spout", meta = (ToolTip = "Controls which world types are allowed to auto-start or editor-start this sender component. Runtime Blueprint start helpers can override this internally."))
    ESpoutWorldBootstrapPolicy StartupPolicy = ESpoutWorldBootstrapPolicy::GameOnly;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Spout", meta = (ToolTip = "Optional actor that this component should tick after. Use this when the source texture is updated by another actor earlier in the frame."))
    TObjectPtr<AActor> TickAfterActor = nullptr;

    // ------------------------------------------------------------------
    // DEFERRED START (fix for the headless GPU crash, 2026-07-03)
    //
    // Root cause: opening the Spout D3D11On12 interop bridge while the
    // engine is still inside its first frames (map load, PSO precompile
    // flood, no swapchain pacing the queue in -RenderOffScreen) raced the
    // D3D12 3D queue and hung the GPU (~3 crashes out of 4 runs,
    // DXGI_ERROR_DEVICE_HUNG, breadcrumbs always stuck at Frame 0). In a
    // windowed session the Present paces the queue, so it never showed up.
    //
    // Fix: the actual bridge open is HELD until BOTH conditions are true:
    //   1) world time >= StartDelaySeconds (minimum grace period), AND
    //   2) engine PSO precaching is idle (no pending precompiles),
    // with MaxStartDelaySeconds as a hard cap so nothing can hold the
    // sender back forever. Implemented in StartBroadcastConfigured().
    // Validated: 4/4 clean 75 s headless runs after the fix.
    // ------------------------------------------------------------------

    // Condition 1: minimum world-time grace period. 0 disables the whole
    // deferral mechanism (start immediately, pre-fix behaviour).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spout", meta = (ClampMin = "0.0", ToolTip = "Minimum seconds of world time before opening the Spout bridge. The start is additionally held until engine PSO precaching is idle, so heavy scenes wait as long as they need. Opening the D3D11On12 interop during the engine's startup frames can deadlock the D3D12 queue in windowless sessions (-RenderOffScreen). Set to 0 to start immediately."))
    float StartDelaySeconds = 2.0f;

    // Safety valve for condition 2: a scene that keeps PSOs compiling for
    // a long time must not delay the sender indefinitely. Past this world
    // time the sender starts even if precaching is still busy (logged as a
    // Warning). 0 = no cap, wait for PSO idle however long it takes.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spout", meta = (ClampMin = "0.0", ToolTip = "Safety cap: if PSO precaching is still busy after this many seconds of world time, start anyway so the sender can never be held back forever. Set to 0 to wait for PSO precaching without a cap."))
    float MaxStartDelaySeconds = 15.0f;

    UFUNCTION(BlueprintCallable, Category = "Spout")
    void SetTickAfterActor(AActor* NewTickAfterActor);

protected:
    virtual void OnRegister() override;
    virtual void OnUnregister() override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
    struct FSpoutStageSlot
    {
        FTextureRHIRef Texture;
        ID3D11Resource* Wrapped11 = nullptr;
        int32 Width = 0;
        int32 Height = 0;
        EPixelFormat Format = PF_Unknown;
        FRenderCommandFence Fence;
        uint64 D3D11FenceValue = 0;
        bool bD3D11FencePending = false;
    };

    bool IsEditorWorld() const;
    bool IsPreviewWorld() const;
    bool IsSupportedWorld() const;
    bool IsD3D12Active() const;
    bool IsUsingEditorViewportSource() const;
    bool IsUsingGameViewportSource() const;
    bool ShouldUseSlateBackBufferGameViewportPath() const;
    bool HasValidConfiguredSource() const;
    bool ResolveCurrentSource(FTextureRHIRef& OutTexture, int32& OutWidth, int32& OutHeight, EPixelFormat& OutFormat) const;
    void ResetGameViewportDebugState();
    void LogGameViewportFailure(const TCHAR* Context, const FString& Reason) const;
    void LogGameViewportReady(const FViewport* Viewport, const FTextureRHIRef& ViewportTexture, int32 Width, int32 Height, EPixelFormat Format) const;
    bool RegisterGameViewportBackBufferCallback();
    void UnregisterGameViewportBackBufferCallback();
    void OnGameViewportBackBufferReady_RenderThread(SWindow& SlateWindow, const FTextureRHIRef& FrameBuffer);

    void EnsureBridge();
    void ShutdownBridge();
    bool CacheDX11FenceObjects();
    void ReleaseFenceObjects();
    bool SignalSubmittedWork(int32 SlotIndex);
    bool IsStageSlotReady_GameThread(int32 SlotIndex) const;
    bool IsStageSlotReady_RenderThread(int32 SlotIndex) const;
    static ID3D12Device* GetUE_D3D12Device();
    static ID3D11On12Device* GetD3D11On12(spoutDX12* InDX12);
    void RefreshEditorState();
    void StopBroadcastInternal(bool bClearConfiguration, bool bClearDesiredState);
    void StartBroadcastConfigured(UTextureRenderTarget2D* RenderTarget, const FString& SenderName, int32 FPS);
    void InitializeDesiredState();
    bool AcquireEditorOwnership(const FString& SenderName);
    void ReleaseEditorOwnership();

    void ApplyTickPrerequisite();
    void ClearTickPrerequisite();

    TWeakObjectPtr<AActor> AppliedTickAfterActor;

    spoutDX12* SpoutBridge = nullptr;

    FSpoutStageSlot StageSlots[2];
    int32 NextStageSlot = 0;

#if PLATFORM_WINDOWS
    ID3D11Device5* CachedDev11_5 = nullptr;
    ID3D11DeviceContext4* CachedCtx11_4 = nullptr;
    ID3D11Fence* CopyFence11 = nullptr;
    ID3D11On12Device* CachedD3D11On12 = nullptr;
    uint64 NextFenceValue = 1;
#endif

    bool bIsBroadcasting = false;
    bool bWantsBroadcasting = false;
    bool bBroadcastIntentInitialized = false;
    mutable FString LastGameViewportFailureKey;
    mutable int32 LastGameViewportFailureRepeatCount = 0;
    mutable bool bHasLoggedGameViewportReady = false;
    mutable const void* LastLoggedGameViewportAddress = nullptr;
    mutable const void* LastLoggedGameViewportTextureAddress = nullptr;
    mutable int32 LastLoggedGameViewportWidth = 0;
    mutable int32 LastLoggedGameViewportHeight = 0;
    mutable EPixelFormat LastLoggedGameViewportFormat = PF_Unknown;
    int32 GameViewportQueuedFrameCount = 0;

    void ResetStageSlots();
    bool SendFrame_RenderThread(
        FRHICommandListImmediate& RHICmdList,
        const FTextureRHIRef& SrcRHI,
        int32 W,
        int32 H,
        EPixelFormat PF,
        int32 SlotIndex,
        ERHIAccess SourceBeforeAccess,
        ERHIAccess SourceAfterAccess,
        bool bRestoreSourceState,
        bool bLogGameViewport,
        const FString& SenderContext);
    void QueueSendFrame_RenderThread(FTextureRHIRef SrcRHI, int32 W, int32 H, EPixelFormat PF, int32 SlotIndex);

    // Retry timer for the deferred start (see StartDelaySeconds above).
    // Keeping the handle lets StopBroadcastInternal cancel a pending retry:
    // without that, stopping the sender during the deferral window would
    // leave a "zombie" timer that restarts the broadcast on its own.
    FTimerHandle StartDelayTimerHandle;
    // Log-throttle flag: the "holding sender start" message is emitted only
    // once per deferral cycle instead of every 0.5 s retry.
    bool bLoggedStartDeferral = false;

    FDelegateHandle GameViewportBackBufferReadyDelegateHandle;
    const SWindow* GameViewportWindow = nullptr;
    FString GameViewportRenderThreadContext;
    double GameViewportMinSendIntervalSeconds = 0.0;
    double GameViewportLastSendTimeSeconds = 0.0;
    bool bGameViewportBackBufferCallbackRegistered = false;
    bool bHasLoggedGameViewportBackBufferCallback = false;
    bool bHasLoggedGameViewportWrongWindowSkip = false;
    bool bHasLoggedGameViewportThrottle = false;
};
