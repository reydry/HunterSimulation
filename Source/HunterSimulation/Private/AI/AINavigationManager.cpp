// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/AINavigationManager.h"
#include "Kismet/GameplayStatics.h"
#include "EnvironmentQuery/EnvQueryManager.h"

void UAINavigationManager::Init()
{
	GetAllAICharacters();
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

	CachedCoverPoints = QueryInstance->GetResultsAsLocations();

	StartTomAndJerryLogic();
}

void UAINavigationManager::StartTomAndJerryLogic()
{
	SafeSpots.Reset();
	SafeSpots.Reserve(CachedCoverPoints.Num());
	
	FTimerManagerTimerParameters Params;
	Params.bLoop = true;
	Params.bMaxOncePerFrame = true;

	GetWorld()->GetTimerManager().SetTimer(RefreshSpotsTimerHandle, this, &UAINavigationManager::RefreshSpots, 0.5, Params);
}

void UAINavigationManager::RefreshSpots()
{
	SafeSpots.Reset();
	FindSafeSpots(SafeSpots);

	for (FVector Point : CachedCoverPoints)
	{
		if (!SafeSpots.Contains(Point))
		{
			DrawDebugPoint(GetWorld(), Point, 25, FColor::Red, false, 0.5f);
		}
		else
		{
			DrawDebugPoint(GetWorld(), Point, 25, FColor::Green, false, 0.5f);
		}
	}
}

void UAINavigationManager::FindSafeSpots(TArray<FVector>& OutArray)
{
	AActor* Enemy = AICharacters[0];

	if (!IsValid(Enemy))
	{
		return;
	}

	FVector EnemyLocation = FVector::ZeroVector;
	FRotator EnemyRotation = FRotator::ZeroRotator;

	Enemy->GetActorEyesViewPoint(EnemyLocation, EnemyRotation);

	FVector EnemyViewDirection = EnemyRotation.Vector();	

	for (FVector Spot : CachedCoverPoints)
	{
		bool bVisible = IsSpotVisible(EnemyLocation, EnemyViewDirection, Spot, Enemy);

		if (!bVisible)
		{
			OutArray.Emplace(Spot);
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
	
	FVector HorizontalView = Direction;
	HorizontalView.Z = 0.0f;
	HorizontalView.Normalize();

	FVector HorizontalTarget = DirectionToSpot;
	HorizontalTarget.Z = 0.0f;
	HorizontalTarget.Normalize();

	float Dot = FVector::DotProduct(HorizontalView, HorizontalTarget);
	float CosHalfFOV = FMath::Cos(FMath::DegreesToRadians(60));

	if (Dot < CosHalfFOV)
	{
		return false;
	}

	FCollisionQueryParams Params;
	Params.bTraceComplex = false;
	Params.AddIgnoredActor(AICharacters[0]);
	FHitResult HitResult;
	bool bVisible = World->LineTraceTestByChannel(Location, Spot, ECC_Visibility, Params);

	return !bVisible;
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

void UAINavigationManager::GetAllAICharacters()
{
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AICharacterClass, AICharacters);
}

FVector UAINavigationManager::CalculateEQSGenerationOrigin()
{
	FVector Origin = FVector::ZeroVector;

	for (AActor* Actor : AICharacters)
	{
		Origin += Actor->GetActorLocation();
	}

	return Origin / AICharacters.Num();
}


