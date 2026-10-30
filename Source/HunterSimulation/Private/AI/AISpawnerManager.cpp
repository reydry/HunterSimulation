// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/AISpawnerManager.h"
#include "NavigationSystem.h"

TArray<AActor*> UAISpawnerManager::SpawnVictims()
{
	TArray<AActor*> Actors;
	Actors.Reserve(NumOfVictims);

	for (int32 i = 0; i < NumOfVictims; ++i)
	{
		Actors.Emplace(SpawnActorFromClass(VictimClass));
	}

	return Actors;
}

TArray<AActor*> UAISpawnerManager::SpawnHunters()
{
	TArray<AActor*> Actors;
	Actors.Reserve(NumOfHunters);
	
	for (int32 i = 0; i < NumOfHunters; ++i)
	{
		Actors.Emplace(SpawnActorFromClass(HunterClass));
	}

	return Actors;
}

AActor* UAISpawnerManager::SpawnActorFromClass(TSubclassOf<AActor>& ActorClass)
{
	UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		return nullptr;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());

	if (!IsValid(NavSys))
	{
		return nullptr;
	}

	FNavLocation Location;
	NavSys->GetRandomPoint(Location);
	
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	Location.Location.Z += Offset;
	
	return World->SpawnActor<AActor>(ActorClass, Location.Location, FRotator::ZeroRotator, Params);
}
