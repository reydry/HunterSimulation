// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AISpawnerManager.generated.h"

UCLASS(Blueprintable)
class HUNTERSIMULATION_API UAISpawnerManager : public UObject
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintPure)
	TArray<AActor*> SpawnVictims();
	UFUNCTION(BlueprintPure)
	TArray<AActor*> SpawnHunters();

protected:
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<AActor> VictimClass;
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<AActor> HunterClass;

	UPROPERTY(EditDefaultsOnly)
	int32 NumOfVictims;
	UPROPERTY(EditDefaultsOnly)
	int32 NumOfHunters;
	
	UPROPERTY(EditDefaultsOnly)
	float Offset = 90.f;

private:	
	AActor* SpawnActorFromClass(TSubclassOf<AActor>& ActorClass);
};
