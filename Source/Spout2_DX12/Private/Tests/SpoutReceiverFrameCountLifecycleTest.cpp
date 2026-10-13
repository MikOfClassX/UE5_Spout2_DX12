#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"

#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
THIRD_PARTY_INCLUDES_START
#include "Windows/AllowWindowsPlatformTypes.h"
#include "SpoutFrameCount.h"
#include "Windows/HideWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_END

namespace
{
	// Isolate the test from the registry preference without modifying that preference.
	class FTestFrameCount : public spoutFrameCount
	{
	public:
		FTestFrameCount() { m_bFrameCount = true; }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpoutFrameCountLifecycleTest,
	"Spout.Receiver.Sync.FrameCountLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpoutFrameCountLifecycleTest::RunTest(const FString& Parameters)
{
	const FString Prefix = TEXT("ClassXFrameCountTest_") + FGuid::NewGuid().ToString();
	const FString NameA = Prefix + TEXT("_A");
	const FString NameB = Prefix + TEXT("_B");
	FTestFrameCount SenderA;
	FTestFrameCount SenderB;
	FTestFrameCount Receiver;
	SenderA.EnableFrameCount(TCHAR_TO_ANSI(*NameA));
	SenderB.EnableFrameCount(TCHAR_TO_ANSI(*NameB));

	// Retarget twice and reopen the same sender, as source switching and restart do.
	for (int32 Step = 0; Step < 4; ++Step)
	{
		const bool bUseA = Step == 0 || Step >= 2;
		const FString& Name = bUseA ? NameA : NameB;
		FTestFrameCount& Sender = bUseA ? SenderA : SenderB;
		Receiver.CleanupFrameCount();
		Receiver.EnableFrameCount(TCHAR_TO_ANSI(*Name));
		TestTrue(FString::Printf(TEXT("Counting stays enabled at step %d"), Step), Receiver.IsFrameCountEnabled());
		Receiver.GetNewFrame(); // Prime this connection's observation before publishing.
		const long Before = Receiver.GetSenderFrame();
		Sender.SetNewFrame();
		TestTrue(FString::Printf(TEXT("New publication detected at step %d"), Step), Receiver.GetNewFrame());
		TestEqual(FString::Printf(TEXT("Correct source counter advances at step %d"), Step), Receiver.GetSenderFrame(), Before + 1);
	}

	// Document the original SDK pitfall using a separate, disposable instance.
	FTestFrameCount DisabledReceiver;
	DisabledReceiver.EnableFrameCount(TCHAR_TO_ANSI(*NameA));
	DisabledReceiver.DisableFrameCount();
	DisabledReceiver.EnableFrameCount(TCHAR_TO_ANSI(*NameB));
	TestFalse(TEXT("Enable does not undo application-level DisableFrameCount"), DisabledReceiver.IsFrameCountEnabled());
	return true;
}
#endif
