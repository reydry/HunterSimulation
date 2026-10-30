// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "Templates/SharedPointer.h"
#include "AINavigationManager.generated.h"

class UEnvQueryManager;
class UEnvQueryInstanceBlueprintWrapper;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSpotChangedDelegate, AActor*, Victim, FVector, Spot);

static TAutoConsoleVariable<bool> CVarDrawDebugSpots(
	TEXT("debug.drawspots"),
	false,
	TEXT("draw safe spots as spheres"),
	ECVF_Default
);

UCLASS(Blueprintable)
class HUNTERSIMULATION_API UAINavigationManager : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	void Init();

	UFUNCTION(BlueprintCallable)
	void Deinit();

	UFUNCTION(BlueprintPure)
	FVector CalculateEQSGenerationOrigin();

	UFUNCTION(BlueprintCallable)
	void RegisterHunters(TArray<AActor*> Actors);

	UFUNCTION(BlueprintCallable)
	void RegisterVictims(TArray<AActor*> Actors);

	UPROPERTY(BlueprintAssignable)
	FOnSpotChangedDelegate OnSpotChangedDelegateHandle;

protected:
	UPROPERTY()
	TArray<AActor*> Hunters;

	UPROPERTY()
	TArray<AActor*> Victims;

	UPROPERTY(EditDefaultsOnly)
	UEnvQuery* Query;

	TArray<FVector> CachedSpots;

private:
	UFUNCTION()
	void OnQueryFinished(UEnvQueryInstanceBlueprintWrapper* QueryInstance, EEnvQueryStatus::Type QueryStatus);
	UFUNCTION()
	void RefreshSpots();

	void GetAllCoverPoints();
	
	void StartTomAndJerryLogic();
	void FindSafeSpots(TArray<FVector>& OutArray);
	bool IsSpotVisible(FVector& Location, FVector& Direction, FVector& Spot, AActor* Enemy);
	bool IsOutOfLOS(FVector& Direction, FVector& DirectionToSpot);

	void OnSpotsRefreshed();
	bool IsSpotStillNotSeen(FVector& Spot);

	void DebugDrawSpots();
	void DebugDrawTakenSpots();

	FTimerHandle RefreshSpotsTimerHandle;
	TArray<FVector> SafeSpots;

	UPROPERTY()
	TMap<AActor*, FVector> TakenSpots;
};
