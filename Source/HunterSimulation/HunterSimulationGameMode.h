// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "HunterSimulationGameMode.generated.h"

class UAISpawnerManager;
class UAINavigationManager;

UCLASS(abstract)
class AHunterSimulationGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AHunterSimulationGameMode();

};



