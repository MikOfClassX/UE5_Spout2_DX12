#pragma once
#include "CoreMinimal.h"

// D3D11 fence UINT64_MAX denotes device removal, never successful completion.
enum class ESpoutReceiverFenceStatus : uint8 { Pending, Ready, DeviceRemoved };
inline ESpoutReceiverFenceStatus GetSpoutReceiverFenceStatus(uint64 Completed, uint64 Required)
{
	if (Completed == MAX_uint64)
	{
		return ESpoutReceiverFenceStatus::DeviceRemoved;
	}
	return Completed >= Required ? ESpoutReceiverFenceStatus::Ready : ESpoutReceiverFenceStatus::Pending;
}
