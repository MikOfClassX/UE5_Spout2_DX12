#include "SpoutReceiverSyncPolicy.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpoutReceiverFencePolicyTest,
	"Spout.Receiver.Sync.FencePolicy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSpoutReceiverFencePolicyTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Earlier fence remains pending"), GetSpoutReceiverFenceStatus(4, 5), ESpoutReceiverFenceStatus::Pending);
	TestEqual(TEXT("Exact completion ready"), GetSpoutReceiverFenceStatus(5, 5), ESpoutReceiverFenceStatus::Ready);
	TestEqual(TEXT("Later completion ready"), GetSpoutReceiverFenceStatus(6, 5), ESpoutReceiverFenceStatus::Ready);
	TestEqual(TEXT("Device removal is not successful completion"), GetSpoutReceiverFenceStatus(MAX_uint64, 5), ESpoutReceiverFenceStatus::DeviceRemoved);
	TestEqual(TEXT("Device removal wins even for zero fence"), GetSpoutReceiverFenceStatus(MAX_uint64, 0), ESpoutReceiverFenceStatus::DeviceRemoved);
	return true;
}
#endif
