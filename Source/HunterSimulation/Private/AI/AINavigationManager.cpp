// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/AINavigationManager.h"
#include "Kismet/GameplayStatics.h"
#include "EnvironmentQuery/EnvQueryManager.h"

static TAutoConsoleVariable<int32> CVarDrawDebugSpots(
	TEXT("ai.debug.spots"), 0,
	TEXT("0 = off, 1 = hunter FOV/close range + reason per cover spot, 2 = also the ranking of spots per victim and why one won"),
	ECVF_Default);

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
	DebugDrawHunters();
	DebugDrawSpotReasons();
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

	float DistToSpot = FVector::Dist2D(Location, Spot);
	
	if (!(DistToSpot <= CloseRangeRadius))
	{
		FVector DirectionToSpot = (Spot - Location).GetSafeNormal();

		if (IsOutOfLOS(Direction, DirectionToSpot))
		{
			return false;
		}
	}

	FCollisionQueryParams Params;
	Params.bTraceComplex = false;
	Params.AddIgnoredActor(Enemy);

	FHitResult HitResult;
	
	bool bVisible = World->LineTraceTestByChannel(Location, Spot, ECC_Visibility, Params);

	return !bVisible;
}

bool UAINavigationManager::IsSpotCouldBeVisible(FVector& Spot)
{
	UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		return false;
	}

	for (AActor* Enemy : Hunters)
	{
		FCollisionQueryParams Params;
		Params.bTraceComplex = false;
		Params.AddIgnoredActor(Enemy);
		FHitResult HitResult;

		bool bVisible = World->LineTraceTestByChannel(Enemy->GetActorLocation(), Spot, ECC_Visibility, Params);

		if (!bVisible)
		{
			return true;
		}
	}

	return false;
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

		for (FVector Spot : SafeSpots)
		{
			if (bHidden && Spot.Equals(CurrentSpot, 1.f))
			{
				continue;
			}

			bool bTakenByOthers = false;

			for (TPair<AActor*, FVector>& Taken : TakenSpots)
			{
				if (Taken.Key != Victim && Taken.Value.Equals(Spot, 1.f))
				{
					bTakenByOthers = true;
					break;
				}
			}

			if (bTakenByOthers)
			{
				continue;
			}

			if (IsSpotCouldBeVisible(Spot))
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
			OnSpotChangedDelegateHandle.Broadcast(this);

			DebugDrawSpotDecision(Victim, BestSpot,
				FString::Printf(TEXT("new spot: picked %d of the %d nearest safe spots, previous=%s"),
					SafeSpots.IndexOfByKey(BestSpot) + 1, SafeSpots.Num(), bHidden ? *CurrentSpot.ToCompactString() : TEXT("none")));
		}
	}
}

bool UAINavigationManager::IsSpotStillNotSeen(FVector& Spot)
{
	return SafeSpots.Contains(Spot);
}

void UAINavigationManager::DebugDrawSpots()
{
	if (CVarDrawDebugSpots.GetValueOnGameThread() < 2)
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
	if (CVarDrawDebugSpots.GetValueOnGameThread() < 2)
	{
		return;
	}

	for (auto& Spot : TakenSpots)
	{
		DrawDebugSphere(GetWorld(), Spot.Value, 64, 1, FColor::Blue, false, 0.5f, 0U, 5.f);
		
		if (IsValid(Spot.Key))
		{
			DrawDebugDirectionalArrow(GetWorld(), Spot.Key->GetActorLocation() + FVector(0, 0, 50.f), Spot.Value + FVector(0, 0, 50.f),
				80.f, FColor::Blue, false, 0.5f, 0, 3.f);

			const FString* Reason = TakenSpotReasons.Find(Spot.Key);
			DrawDebugString(GetWorld(), Spot.Value + FVector(0, 0, 120.f),
				FString::Printf(TEXT("%s: %s"), *GetNameSafe(Spot.Key), Reason ? **Reason : TEXT("(no reason recorded)")),
				nullptr, FColor::Blue, 0.5f, true);
		}
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

TArray<AActor*> UAINavigationManager::GetHunters()
{
	return Hunters;
}

TArray<FVector> UAINavigationManager::GetSafeSpots()
{
	return SafeSpots;
}

FVector UAINavigationManager::GetTakenSpot(AActor* Character)
{
	FVector* Location = TakenSpots.Find(Character);

	if (Location != nullptr)
	{
		return *Location;
	}
	return FVector();
}

void UAINavigationManager::DebugDrawHunters()
{
	if (CVarDrawDebugSpots.GetValueOnGameThread() < 1)
	{
		return;
	}

	for (AActor* Hunter : Hunters)
	{
		if (!IsValid(Hunter))
		{
			continue;
		}

		FVector Eye = FVector::ZeroVector;
		FRotator Rotation = FRotator::ZeroRotator;
		Hunter->GetActorEyesViewPoint(Eye, Rotation);

		const FVector Forward = FRotator(0.f, Rotation.Yaw, 0.f).Vector();
		const float HalfFOV = 60.f;
		const float ConeLength = 1500.f;
		const FVector Left = Forward.RotateAngleAxis(-HalfFOV, FVector::UpVector);
		const FVector Right = Forward.RotateAngleAxis(HalfFOV, FVector::UpVector);

		DrawDebugLine(GetWorld(), Eye, Eye + Left * ConeLength, FColor::Yellow, false, 0.5f, 0, 2.f);
		DrawDebugLine(GetWorld(), Eye, Eye + Right * ConeLength, FColor::Yellow, false, 0.5f, 0, 2.f);
		DrawDebugLine(GetWorld(), Eye, Eye + Forward * ConeLength, FColor(180, 180, 0), false, 0.5f, 0, 1.f);

		DrawDebugCircle(GetWorld(), Hunter->GetActorLocation() + FVector(0, 0, 10.f), CloseRangeRadius, 64, FColor::Orange,
			false, 0.5f, 0, 2.f, FVector(1, 0, 0), FVector(0, 1, 0), false);

		DrawDebugString(GetWorld(), Eye + FVector(0, 0, 80.f),
			FString::Printf(TEXT("%s | FOV +-%.0f | close range %.0f"), *GetNameSafe(Hunter), HalfFOV, CloseRangeRadius),
			nullptr, FColor::Yellow, 0.5f, true);
	}
}

void UAINavigationManager::DebugDrawSpotReasons()
{
	if (CVarDrawDebugSpots.GetValueOnGameThread() < 1)
	{
		return;
	}

	const float LabelRadius = 1500.f;

	for (const TPair<FVector, FString>& Entry : SpotDebugReasons)
	{
		bool bNearVictim = false;
		for (AActor* Victim : Victims)
		{
			if (IsValid(Victim) && FVector::Dist2D(Victim->GetActorLocation(), Entry.Key) <= LabelRadius)
			{
				bNearVictim = true;
				break;
			}
		}

		if (!bNearVictim)
		{
			continue;
		}

		DrawDebugString(GetWorld(), Entry.Key + FVector(0, 0, 90.f), Entry.Value, nullptr,
			Entry.Value.StartsWith(TEXT("SAFE")) ? FColor::Green : FColor::Red, 0.5f, false, 0.8f);
	}
}

void UAINavigationManager::DebugDrawSpotDecision(AActor* Victim, const FVector& Chosen, const FString& Verdict)
{
	TakenSpotReasons.Add(Victim, Verdict);

	if (CVarDrawDebugSpots.GetValueOnGameThread() < 2)
	{
		return;
	}

	const FVector VictimLocation = Victim->GetActorLocation();
	const float ChosenDistance = FVector::Dist2D(VictimLocation, Chosen);
	const FVector Lift(0.f, 0.f, 20.f);

	struct FRanked { FVector Location; float Distance; FString TakenBy; };
	TArray<FRanked> Ranking;

	for (const FVector& Spot : SafeSpots)
	{
		FRanked Entry{ Spot, FVector::Dist2D(VictimLocation, Spot), FString() };

		for (const TPair<AActor*, FVector>& Taken : TakenSpots)
		{
			if (Taken.Key != Victim && Taken.Value.Equals(Spot, 1.f))
			{
				Entry.TakenBy = GetNameSafe(Taken.Key);
				break;
			}
		}
		Ranking.Add(Entry);
	}

	Ranking.Sort([](const FRanked& A, const FRanked& B) { return A.Distance < B.Distance; });

	for (int32 i = 0; i < FMath::Min(6, Ranking.Num()); ++i)
	{
		const FRanked& R = Ranking[i];
		FString Text;
		FColor Color;

		if (R.Location.Equals(Chosen, 1.f))
		{
			Text = FString::Printf(TEXT("#%d d=%.0f CHOSEN"), i + 1, R.Distance);
			Color = FColor::Blue;
		}
		else if (!R.TakenBy.IsEmpty())
		{
			Text = FString::Printf(TEXT("#%d d=%.0f skipped: taken by %s"), i + 1, R.Distance, *R.TakenBy);
			Color = FColor::Orange;
		}
		else if (R.Distance <= ChosenDistance)
		{
			Text = FString::Printf(TEXT("#%d d=%.0f closer, but not picked (see verdict)"), i + 1, R.Distance);
			Color = FColor::Cyan;
		}
		else
		{
			Text = FString::Printf(TEXT("#%d d=%.0f farther than chosen by %.0f"), i + 1, R.Distance, R.Distance - ChosenDistance);
			Color = FColor(150, 150, 150);
		}

		DrawDebugSphere(GetWorld(), R.Location + Lift, 28.f, 10, Color, false, 0.5f, 0, 2.f);
		DrawDebugString(GetWorld(), R.Location + FVector(0, 0, 150.f), FString::Printf(TEXT("%s: %s"), *GetNameSafe(Victim), *Text),
			nullptr, Color, 0.5f, true);
	}

	TArray<TPair<float, FVector>> Unsafe;
	for (const FVector& Spot : CachedSpots)
	{
		if (!SafeSpots.Contains(Spot))
		{
			Unsafe.Emplace(FVector::Dist2D(VictimLocation, Spot), Spot);
		}
	}
	Unsafe.Sort([](const TPair<float, FVector>& A, const TPair<float, FVector>& B) { return A.Key < B.Key; });

	for (int32 i = 0, Shown = 0; i < Unsafe.Num() && Shown < 3 && Unsafe[i].Key < ChosenDistance; ++i, ++Shown)
	{
		const FString* Reason = SpotDebugReasons.Find(Unsafe[i].Value);
		DrawDebugString(GetWorld(), Unsafe[i].Value + FVector(0, 0, 150.f),
			FString::Printf(TEXT("%s: nearer (d=%.0f) but UNSAFE - %s"), *GetNameSafe(Victim), Unsafe[i].Key, Reason ? **Reason : TEXT("?")),
			nullptr, FColor::Red, 0.5f, true);
	}
}