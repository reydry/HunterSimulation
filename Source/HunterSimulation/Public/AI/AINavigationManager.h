// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "Templates/SharedPointer.h"
#include "AINavigationManager.generated.h"

class UEnvQueryManager;
class UEnvQueryInstanceBlueprintWrapper;

UCLASS(Blueprintable)
class HUNTERSIMULATION_API UAINavigationManager : public UObject
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable)
	void Init();
	
	UFUNCTION(BlueprintCallable)
	void Deinit();

	UFUNCTION()
	void OnQueryFinished(UEnvQueryInstanceBlueprintWrapper* QueryInstance, EEnvQueryStatus::Type QueryStatus);
	
	UFUNCTION(BlueprintPure)
	FVector CalculateEQSGenerationOrigin();

protected:
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<AActor> AICharacterClass;

	UPROPERTY()
	TArray<AActor*> AICharacters;

	UPROPERTY(EditDefaultsOnly)
	UEnvQuery* Query;

	UPROPERTY(EditDefaultsOnly)
	UEnvQueryContext* Querier;

	TArray<FVector> CachedCoverPoints;

private:
	void GetAllCoverPoints();
	void GetAllAICharacters();
	
	FTimerHandle RefreshSpotsTimerHandle;

	TArray<FVector> SafeSpots;

	void StartTomAndJerryLogic();

	UFUNCTION()
	void RefreshSpots();

	void FindSafeSpots(TArray<FVector>& OutArray);
	bool IsSpotVisible(FVector& Location, FVector& Direction, FVector& Spot, AActor* Enemy);
};
