#pragma once

#include "UObject/ObjectMacros.h"

UENUM(BlueprintType)
enum EACFCustomMovementMode : uint8
{
	Climbing	UMETA(DisplayName = "Climbing"),
	Max			UMETA(Hidden),
};