// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/AINavigationManager.h"
#include "Kismet/GameplayStatics.h"
#include "EnvironmentQuery/EnvQueryManager.h"

void UAINavigationManager::Init()
{
	CalculateEQSGenerationOrigin();
	GetAllCoverPoints();
}

void UAINavigationManager::Deinit()
{
	GetWorld()->GetTimerManager().ClearTimer(RefreshSpotsTimerHandle);
}

void UAINavigationManager::OnQueryFinished(UEnvQueryInstanceBlueprintWrapper* QueryInstance, EEnvQueryStatus::Type QueryStatus)
{
	if (QueryStatus != EEnvQueryStatus::Type::Success)
	{
		return;
	}

	CachedSpots = QueryInstance->GetResultsAsLocations();

	StartTomAndJerryLogic();
}

void UAINavigationManager::StartTomAndJerryLogic()
{
	SafeSpots.Reset();
	SafeSpots.Reserve(CachedSpots.Num());
	
	FTimerManagerTimerParameters Params;
	Params.bLoop = true;
	Params.bMaxOncePerFrame = true;

	GetWorld()->GetTimerManager().SetTimer(RefreshSpotsTimerHandle, this, &UAINavigationManager::RefreshSpots, 0.5, Params);
}

void UAINavigationManager::RefreshSpots()
{
	FindSafeSpots(SafeSpots);
	DebugDrawSpots();
	OnSpotsRefreshed();
	DebugDrawTakenSpots();
}

void UAINavigationManager::FindSafeSpots(TArray<FVector>& OutArray)
{
	SafeSpots.Reset();

	for (AActor* Enemy : Hunters)
	{
		FVector EnemyLocation = FVector::ZeroVector;
		FRotator EnemyRotation = FRotator::ZeroRotator;

		Enemy->GetActorEyesViewPoint(EnemyLocation, EnemyRotation);

		FVector EnemyViewDirection = EnemyRotation.Vector();

		for (FVector Spot : CachedSpots)
		{
			bool bVisible = IsSpotVisible(EnemyLocation, EnemyViewDirection, Spot, Enemy);

			if (!bVisible)
			{
				if (!OutArray.Contains(Spot))
				{
					OutArray.Emplace(Spot);
				}
			}
			else
			{
				if (OutArray.Contains(Spot))
				{
					OutArray.Remove(Spot);
				}
			}
		}
	}
}

bool UAINavigationManager::IsSpotVisible(FVector& Location, FVector& Direction, FVector& Spot, AActor* Enemy)
{
	UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		return false;
	}

	FVector DirectionToSpot = (Spot - Location).GetSafeNormal();
	
	if (IsOutOfLOS(Direction, DirectionToSpot))
	{
		return false;
	}

	FCollisionQueryParams Params;
	Params.bTraceComplex = false;
	Params.AddIgnoredActor(Enemy);

	FHitResult HitResult;
	
	bool bVisible = World->LineTraceTestByChannel(Location, Spot, ECC_Visibility, Params);

	return !bVisible;
}

bool UAINavigationManager::IsOutOfLOS(FVector& Direction, FVector& DirectionToSpot)
{
	FVector HorizontalView = Direction;
	HorizontalView.Z = 0.0f;
	HorizontalView.Normalize();

	FVector HorizontalTarget = DirectionToSpot;
	HorizontalTarget.Z = 0.0f;
	HorizontalTarget.Normalize();

	float Dot = FVector::DotProduct(HorizontalView, HorizontalTarget);
	float CosHalfFOV = FMath::Cos(FMath::DegreesToRadians(60));
	
	return Dot < CosHalfFOV;
}

void UAINavigationManager::OnSpotsRefreshed()
{
	for (AActor* Victim : Victims)
	{
		FVector CurrentSpot = FVector::ZeroVector;
		FVector BestSpot = FVector::ZeroVector;
		FVector *TakenSpot = TakenSpots.Find(Victim);
	
		bool bHidden = TakenSpot != nullptr;
		
		if (bHidden)
		{
			CurrentSpot = *TakenSpot;
		}

		if (bHidden && IsSpotStillNotSeen(CurrentSpot))
		{
			continue;
		}

		CurrentSpot = Victim->GetActorLocation();
		
		float ShortDist = MAX_FLT;
		bool bFound = false;

		for (FVector Spot : CachedSpots)
		{
			if (bHidden && Spot.Equals(CurrentSpot, 1.f))
			{
				continue;
			}

			float Dist = FVector::DistSquared(CurrentSpot, Spot);
			
			if (Dist < ShortDist)
			{
				ShortDist = Dist;
				BestSpot = Spot;
				bFound = true;
			}
		}

		if (bFound)
		{
			TakenSpots.Remove(Victim);
			TakenSpots.Emplace(Victim, BestSpot);
			OnSpotChangedDelegateHandle.Broadcast(Victim, BestSpot);
		}
	}
}

bool UAINavigationManager::IsSpotStillNotSeen(FVector& Spot)
{
	return SafeSpots.Contains(Spot);
}

void UAINavigationManager::DebugDrawSpots()
{
	bool bDrawSafeSpots = CVarDrawDebugSpots.GetValueOnGameThread();

	if (!bDrawSafeSpots)
	{
		return;
	}

	for (FVector Spot : CachedSpots)
	{
		if (!SafeSpots.Contains(Spot))
		{
			DrawDebugSphere(GetWorld(), Spot, 32, 1, FColor::Red, false, 0.5f, 0U, 5.f);
		}
		else
		{
			DrawDebugSphere(GetWorld(), Spot, 32, 1, FColor::Green, false, 0.5f, 0U, 5.f);
		}
	}
}

void UAINavigationManager::DebugDrawTakenSpots()
{
	//DrawDebugDirectionalArrow(GetWorld())
	for (auto& Spot : TakenSpots)
	{


			DrawDebugSphere(GetWorld(), Spot.Value, 64, 1, FColor::Blue, false, 0.5f, 0U, 5.f);

	}
}


void UAINavigationManager::GetAllCoverPoints()
{
	UEnvQueryManager* QueryManager = UEnvQueryManager::GetCurrent(GetWorld());
	UEnvQueryInstanceBlueprintWrapper* ActiveWrapper = QueryManager->RunEQSQuery(this, Query, GetWorld(), EEnvQueryRunMode::AllMatching, nullptr);

	if (IsValid(ActiveWrapper))
	{
		ActiveWrapper->GetOnQueryFinishedEvent().AddDynamic(this, &UAINavigationManager::OnQueryFinished);
	}
}

FVector UAINavigationManager::CalculateEQSGenerationOrigin()
{
	FVector Origin = FVector::ZeroVector;

	for (AActor* Actor : Hunters)
	{
		Origin += Actor->GetActorLocation();
	}

	for (AActor* Actor : Victims)
	{
		Origin += Actor->GetActorLocation();
	}

	return Origin / (Hunters.Num() + Victims.Num());
}

void UAINavigationManager::RegisterHunters(TArray<AActor*> Actors)
{
	Hunters = Actors;
}

void UAINavigationManager::RegisterVictims(TArray<AActor*> Actors)
{
	Victims = Actors;
}
