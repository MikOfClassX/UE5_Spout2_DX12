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
    // WAIT BEFORE START
    // This fixes a GPU freeze that happened in headless mode (no window).
    //
    // Problem: if we open the Spout bridge too early, while the engine
    // is still busy with its first frames, the GPU can freeze. This
    // only happens with no window (-RenderOffScreen). A normal window
    // does not have this problem.
    //
    // Fix: we wait to open the bridge until BOTH are true:
    //   1) StartDelaySeconds of game time has passed, AND
    //   2) the engine finished compiling its shaders.
    // MaxStartDelaySeconds is the longest we will ever wait.
    // The code for this is in StartBroadcastConfigured().
    // ------------------------------------------------------------------

    // Minimum time to wait. Set to 0 to start right away, like before
    // this fix.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spout", meta = (ClampMin = "0.0", ToolTip = "Minimum seconds of world time before opening the Spout bridge. The start is additionally held until engine PSO precaching is idle, so heavy scenes wait as long as they need. Opening the D3D11On12 interop during the engine's startup frames can deadlock the D3D12 queue in windowless sessions (-RenderOffScreen). Set to 0 to start immediately."))
    float StartDelaySeconds = 2.0f;

    // Longest time we will wait. If shaders are still compiling after
    // this time, we start anyway (and log a Warning). Set to 0 for no
    // limit.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spout", meta = (ClampMin = "0.0", ToolTip = "Safety cap: if PSO precaching is still busy after this many seconds of world time, start anyway so the sender can never be held back forever. Set to 0 to wait for PSO precaching without a cap."))
    float MaxStartDelaySeconds = 15.0f;

    UFUNCTION(BlueprintCallable, Category = "Spout")
    void SetTickAfterActor(AActor* NewTickAfterActor);

    // ------------------------------------------------------------------
    // GPU FRAME COMPLETION EVENT (genlock support)
    //
    // Problem: a caller that needs to tell an external process "the frame
    // is ready" has no way to know when the GPU actually finished writing
    // the shared texture. Everything on the game thread only proves the
    // work was *queued*, not done, so the external process reads a frame
    // that is still one behind.
    //
    // Fix: we already signal a D3D11 fence right after the copy (see
    // SignalSubmittedWork). Pass a Win32 event handle here and the GPU
    // itself signals it via ID3D11Fence::SetEventOnCompletion when it
    // reaches that fence value. Nothing blocks: no game thread stall, no
    // frame rate cost, and the event fires exactly when the frame is
    // really on the shared texture.
    //
    // The handle stays owned by the caller: we never close it. Pass
    // nullptr to detach (default), which restores the previous behaviour
    // exactly. If a frame is skipped (staging slots still busy) the event
    // is signaled right away instead, so a waiting process is never left
    // hanging for a frame that will never be sent.
    // ------------------------------------------------------------------
    void SetFrameCompletionEvent(void* EventHandle);

    // True once the bridge is open and frames are actually going out. Note the
    // start is deferred (see StartDelaySeconds), so this stays false for the
    // first seconds of a session: a caller relying on the completion event above
    // must signal by itself while this is false, or it would wait for a frame
    // the sender is not sending yet.
    UFUNCTION(BlueprintPure, Category = "Spout")
    bool IsBroadcasting() const { return bIsBroadcasting; }

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

    // Win32 event the GPU signals when it finishes writing a frame, or
    // null when nobody asked for it. Set through SetFrameCompletionEvent;
    // owned by the caller, never closed here.
    void* FrameCompletionEvent = nullptr;

    // Signals FrameCompletionEvent right now, if one is set. Used when a
    // frame is skipped, so a process waiting on it is not left hanging.
    void SignalFrameCompletionEventNow();
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

    // Timer used for the wait-before-start (see StartDelaySeconds).
    // We need this so StopBroadcastInternal can stop the timer. If we
    // don't, the timer could start the broadcast again by itself later.
    FTimerHandle StartDelayTimerHandle;
    // Makes sure we log the "waiting to start" message only one time,
    // not on every retry.
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
