// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EQT_PossibleCovers.generated.h"

/**
 * 
 */
UCLASS()
class HUNTERSIMULATION_API UEQT_PossibleCovers : public UEnvQueryTest
{
	GENERATED_BODY()
	
	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;
	
public:
	UEQT_PossibleCovers(const FObjectInitializer& ObjectInitializer);

private:
	UPROPERTY(EditDefaultsOnly)
	int32 NumRays = 4;

	UPROPERTY(EditDefaultsOnly)
	float DistanceToWall = 200.f;

	UPROPERTY(EditDefaultsOnly)
	float TraceHeight = 90.f;

	UPROPERTY(EditDefaultsOnly)
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;
};
