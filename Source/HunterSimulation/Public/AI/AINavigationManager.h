// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "Templates/SharedPointer.h"
#include "AINavigationManager.generated.h"

class UEnvQueryManager;
class UEnvQueryInstanceBlueprintWrapper;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSpotChangedDelegate, UAINavigationManager*, Manager);

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

	UFUNCTION(BlueprintPure)
	TArray<AActor*> GetHunters();

	UFUNCTION(BlueprintPure)
	TArray<FVector> GetSafeSpots();

	UFUNCTION(BlueprintPure)
	FVector GetTakenSpot(AActor* Character);

protected:
	UPROPERTY()
	TArray<AActor*> Hunters;

	UPROPERTY()
	TArray<AActor*> Victims;

	UPROPERTY(EditDefaultsOnly)
	UEnvQuery* Query;

	TArray<FVector> CachedSpots;

	UPROPERTY(EditDefaultsOnly)
	float CloseRangeRadius = 500.f;

private:
	UFUNCTION()
	void OnQueryFinished(UEnvQueryInstanceBlueprintWrapper* QueryInstance, EEnvQueryStatus::Type QueryStatus);
	UFUNCTION()
	void RefreshSpots();

	void GetAllCoverPoints();

	void StartTomAndJerryLogic();
	void FindSafeSpots(TArray<FVector>& OutArray);
	bool IsSpotVisible(FVector& Location, FVector& Direction, FVector& Spot, AActor* Enemy);
	bool IsSpotCouldBeVisible(FVector& Spot);
	bool IsOutOfLOS(FVector& Direction, FVector& DirectionToSpot);

	void OnSpotsRefreshed();
	bool IsSpotStillNotSeen(FVector& Spot);

	void DebugDrawSpots();
	void DebugDrawTakenSpots();

	FTimerHandle RefreshSpotsTimerHandle;
	TArray<FVector> SafeSpots;

	UPROPERTY()
	TMap<AActor*, FVector> TakenSpots;

	void DebugDrawHunters();
	void DebugDrawSpotReasons();
	void DebugDrawSpotDecision(AActor* Victim, const FVector& Chosen, const FString& Verdict);

	TMap<FVector, FString> SpotDebugReasons;
	TMap<AActor*, FString> TakenSpotReasons;
};