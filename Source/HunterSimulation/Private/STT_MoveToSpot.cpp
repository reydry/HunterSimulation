// Fill out your copyright notice in the Description page of Project Settings.


#include "STT_MoveToSpot.h"

#include "AIController.h"
#include "GraphAStar.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "Navigation/PathFollowingComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "StateTreeExecutionContext.h"
#include "NavigationPath.h"

DEFINE_LOG_CATEGORY_STATIC(LogMoveToSpot, Log, All);

EStateTreeRunStatus FSTT_MoveToSpot::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	InstanceData.CurrentSpot = FVector(FLT_MAX);
	InstanceData.bAtTarget = false;

	return Replan(InstanceData) ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

EStateTreeRunStatus FSTT_MoveToSpot::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AAIController* Controller = InstanceData.Controller;
	APawn* Owner = IsValid(Controller) ? Controller->GetPawn() : nullptr;

	if (!IsValid(Owner))
	{
		return EStateTreeRunStatus::Failed;
	}

	FVector ActorLocation = Owner->GetActorLocation();
	float Dist = FVector::Dist2D(ActorLocation, InstanceData.Spot);

	bool bTargetReached = Dist <= InstanceData.AcceptanceRadius;

	float DistToCurrentSpot = FVector::Dist2D(ActorLocation, InstanceData.CurrentSpot);
	bool bCurrentSpotReached = DistToCurrentSpot <= InstanceData.AcceptanceRadius;

	if (bTargetReached)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	if (!InstanceData.bAtTarget && bCurrentSpotReached)
	{
		AddVisited(InstanceData.CurrentSpot, InstanceData);
		return Replan(InstanceData) ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
	}

	return EStateTreeRunStatus::Running;
}

bool FSTT_MoveToSpot::IsPathExposed(UWorld* World, AActor* Pawn, FVector& Start, FVector& End, TArray<AActor*>& Hunters) const
{
	UNavigationSystemV1* NavigationSystem = UNavigationSystemV1::GetCurrent(World);
	
	if (!IsValid(NavigationSystem))
	{
		UE_LOG(LogMoveToSpot, Warning, TEXT("[%s] IsPathExposed: navigation system is invalid, treating path as hidden"), *GetNameSafe(Pawn));
		return false;
	}

	UNavigationPath* Path = NavigationSystem->FindPathToLocationSynchronously(World, Start, End, Pawn);
	
	if (!IsValid(Path))
	{
		UE_LOG(LogMoveToSpot, Warning, TEXT("[%s] IsPathExposed: no navigation path %s -> %s, treating path as exposed"), *GetNameSafe(Pawn),
			*Start.ToCompactString(), *End.ToCompactString());
		return true;
	}

	TArray<FVector> PathPoints = Path->PathPoints;

	UE_LOG(LogMoveToSpot, Verbose, TEXT("[%s] IsPathExposed: path %s -> %s, points=%d, valid=%s, partial=%s, hunters=%d"), *GetNameSafe(Pawn),
		*Start.ToCompactString(), *End.ToCompactString(), PathPoints.Num(),
		Path->IsValid() ? TEXT("yes") : TEXT("no"), Path->IsPartial() ? TEXT("yes") : TEXT("no"), Hunters.Num());

	for (AActor* Hunter : Hunters)
	{
		for(int32 i = 0; i < PathPoints.Num() - 1; ++i)
		{
			if (IsPathSegmentExposed(PathPoints[i], PathPoints[i + 1], Hunter, 60))
			{
				UE_LOG(LogMoveToSpot, Verbose, TEXT("[%s] IsPathExposed: segment %d/%d (%s -> %s) is seen by hunter %s -> EXPOSED"), *GetNameSafe(Pawn),
					i, PathPoints.Num() - 1,
					*PathPoints[i].ToCompactString(), *PathPoints[i + 1].ToCompactString(),
					*GetNameSafe(Hunter));
				return true;
			}
		}
	}
	
	UE_LOG(LogMoveToSpot, Verbose, TEXT("[%s] IsPathExposed: path %s -> %s is hidden from all hunters"), *GetNameSafe(Pawn),
		*Start.ToCompactString(), *End.ToCompactString());

	return false;
}

bool FSTT_MoveToSpot::IsPathSegmentExposed(FVector& Start, FVector& End, AActor* Hunter, float HalfFOV) const
{
	if (!IsValid(Hunter))
	{
		UE_LOG(LogMoveToSpot, Verbose, TEXT("[%s] IsPathSegmentExposed: hunter is invalid, skipping"), *GetNameSafe(Hunter));
		return false;
	}

	FVector EyeLocation = FVector::ZeroVector;
	FRotator EyeRotation = FRotator::ZeroRotator;

	Hunter->GetActorEyesViewPoint(EyeLocation, EyeRotation);

	const bool bExposed = IsPathSegmentExposed2D(Start, End, EyeLocation, EyeRotation.Vector(), HalfFOV);

	UE_LOG(LogMoveToSpot, Verbose, TEXT("[%s] IsPathSegmentExposed: eye=%s, dir=%s, halfFOV=%.1f, segment %s -> %s, inFOV=%s"), *GetNameSafe(Hunter),
		*EyeLocation.ToCompactString(), *EyeRotation.Vector().ToCompactString(), HalfFOV,
		*Start.ToCompactString(), *End.ToCompactString(), bExposed ? TEXT("yes") : TEXT("no"));

	return bExposed;
}

bool FSTT_MoveToSpot::IsPathSegmentExposed2D(FVector& Start, FVector& End, FVector& EyeLocation, FVector EyeDirection, float HalfFOV) const
{
	EyeDirection.Normalize();

	FVector2D Start2D(Start);
	FVector2D End2D(End);
	FVector2D EnemyLocation2D(EyeLocation);
	FVector2D SegmentDirection2D = End2D - Start2D;

	float Cos = FMath::Cos(FMath::DegreesToRadians(HalfFOV));
	float Sin = FMath::Sin(FMath::DegreesToRadians(HalfFOV));

	FVector2D Left(EyeDirection.X * Cos - EyeDirection.Y * Sin, EyeDirection.X * Sin + EyeDirection.Y * Cos);
	FVector2D Right(EyeDirection.X * Cos + EyeDirection.Y * Sin, -EyeDirection.X * Sin + EyeDirection.Y * Cos);

	FVector2D NormalLeft(Left.Y, -Left.X);
	FVector2D NormalRight(-Right.Y, Right.X);

	float EnterT = 0.f;
	float ExitT = 1.f;

	auto Clip = [&](FVector2D& Vector)
		{
			float StartSideDistance = FVector2D::DotProduct(Start2D - EnemyLocation2D, Vector);
			float MoveToward = FVector2D::DotProduct(SegmentDirection2D, Vector);

			if (FMath::Abs(MoveToward) < KINDA_SMALL_NUMBER)
			{
				return StartSideDistance >= 0.f;
			}

			float CrossT = -StartSideDistance / MoveToward;

			if (MoveToward > 0.f)
			{
				EnterT = FMath::Max(EnterT, CrossT);
			}
			else
			{
				ExitT = FMath::Min(ExitT, CrossT);
			}

			return EnterT < ExitT;
		};

	return Clip(Left) && Clip(Right);
}

void FSTT_MoveToSpot::AddVisited(FVector& Spot, FInstanceDataType& InstanceData) const
{
	InstanceData.VisitedSpots.Emplace(Spot);
	const APawn* Pawn = IsValid(InstanceData.Controller) ? InstanceData.Controller->GetPawn() : nullptr;
	UE_LOG(LogMoveToSpot, Log, TEXT("[%s] AddVisited: spot=%s, totalVisited=%d"), *GetNameSafe(Pawn), *Spot.ToCompactString(), InstanceData.VisitedSpots.Num());
}

bool FSTT_MoveToSpot::Replan(FInstanceDataType& InstanceData) const
{
	AAIController* Controller = InstanceData.Controller;
	APawn* Owner = Controller ? Controller->GetPawn() : nullptr;
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;

	if (!IsValid(World))
	{
		UE_LOG(LogMoveToSpot, Warning, TEXT("[%s] Replan: pawn or world is invalid -> false"), *GetNameSafe(Controller));
		return false;
	}

	FVector Target = InstanceData.Spot;
	FVector Start = Owner->GetActorLocation();
	
	UE_LOG(LogMoveToSpot, Log, TEXT("[%s] Replan: start=%s, finalSpot=%s, currentSpot=%s, visited=%d, safeSpots=%d"), *GetNameSafe(Owner),
		*Start.ToCompactString(), *InstanceData.Spot.ToCompactString(),
		*InstanceData.CurrentSpot.ToCompactString(), InstanceData.VisitedSpots.Num(), InstanceData.SafeSpots.Num());

	if (IsPathExposed(World, Owner, Start, Target, InstanceData.Hunters))
	{
		UE_LOG(LogMoveToSpot, Log, TEXT("[%s] Replan: direct path to final spot is EXPOSED, looking for safe spots"), *GetNameSafe(Owner));

		TArray<FVector> PossibleSpots;
		PickSpots(Start, InstanceData, PossibleSpots);

		UE_LOG(LogMoveToSpot, Log, TEXT("[%s] Replan: %d candidate spot(s) picked"), *GetNameSafe(Owner), PossibleSpots.Num());

		bool bFoundSafeSpot = false;

		for (FVector& Spot : PossibleSpots)
		{
			const bool bExposed = IsPathExposed(World, Owner, Start, Spot, InstanceData.Hunters);

			if (!bExposed)
			{
				UE_LOG(LogMoveToSpot, Log, TEXT("[%s] Replan: candidate %s -> %s"), *GetNameSafe(Owner),
					*Spot.ToCompactString(), bExposed ? TEXT("exposed") : TEXT("hidden"));

				Target = Spot;
				bFoundSafeSpot = true;
				break;
			}
		}

		if (!bFoundSafeSpot)
		{
			UE_LOG(LogMoveToSpot, Warning, TEXT("[%s] Replan: no hidden candidate found, falling back to final spot"), *GetNameSafe(Owner));
		}
	}
	else
	{
		UE_LOG(LogMoveToSpot, Log, TEXT("[%s] Replan: direct path to final spot is hidden, going directly"), *GetNameSafe(Owner));
	}

	bool bIsTarget = Target.Equals(InstanceData.Spot, 1.f);

	if (Target.Equals(InstanceData.CurrentSpot, 1.f) && Controller->GetMoveStatus() == EPathFollowingStatus::Moving)
	{
		UE_LOG(LogMoveToSpot, Log, TEXT("[%s] Replan: target %s unchanged and already moving, not restarting movement"), *GetNameSafe(Owner),
			*Target.ToCompactString());

		InstanceData.bAtTarget = bIsTarget;
		return true;
	}

	InstanceData.CurrentSpot = Target;
	InstanceData.bAtTarget = bIsTarget;

	EPathFollowingRequestResult::Type Result = Controller->MoveToLocation(Target, InstanceData.AcceptanceRadius, false, true, true);

	UE_LOG(LogMoveToSpot, Log, TEXT("[%s] Replan: MoveToLocation(%s, radius %.1f) [%s] -> result=%d, moveStatus=%d"), *GetNameSafe(Owner),
		*Target.ToCompactString(), InstanceData.AcceptanceRadius,
		bIsTarget ? TEXT("final spot") : TEXT("intermediate spot"),
		static_cast<int32>(Result), static_cast<int32>(Controller->GetMoveStatus()));

	if (Result != EPathFollowingRequestResult::Type::Failed)
	{
		return true;
	}

	if (bIsTarget)
	{
		UE_LOG(LogMoveToSpot, Warning, TEXT("[%s] Replan: MoveToLocation to final spot failed -> false"), *GetNameSafe(Owner));

		return false;
	}

	UE_LOG(LogMoveToSpot, Warning, TEXT("[%s] Replan: MoveToLocation to intermediate spot %s failed, marking visited"), *GetNameSafe(Owner),
		*Target.ToCompactString());

	AddVisited(Target, InstanceData);

	return false;
}

void FSTT_MoveToSpot::PickSpots(FVector& Start, FInstanceDataType& InstanceData, TArray<FVector>& OutSpots) const
{
	OutSpots.Reset();
	const APawn* Pawn = IsValid(InstanceData.Controller) ? InstanceData.Controller->GetPawn() : nullptr;
	struct FSpot
	{
		FVector Location;
		float Detour;
	};
	const int32 MaxCandidates = 5;
	TArray<FSpot> Best;
	Best.Reserve(MaxCandidates + 1);

	int32 SkippedVisited = 0;
	int32 SkippedWorse = 0;

	for (FVector& Spot : InstanceData.SafeSpots)
	{
		FVector2D Spot2D(Spot);

		float DetourLength = FVector2D::Distance(FVector2D(Start), Spot2D) + FVector2D::Distance(Spot2D, FVector2D(InstanceData.Spot));

		bool bVisited = false;

		for (FVector& VisitedSpot : InstanceData.VisitedSpots)
		{
			if (FVector::DistSquared2D(VisitedSpot, Spot) < FMath::Square(5.f))
			{
				bVisited = true;
				break;
			}
		}

		if (bVisited)
		{
			++SkippedVisited;
			continue;
		}
		
		if (Best.Num() >= MaxCandidates && DetourLength >= Best.Last().Detour)
		{
			++SkippedWorse;
			UE_LOG(LogMoveToSpot, Verbose, TEXT("[%s] PickSpots: spot %s skipped (detour %.1f >= worst kept %.1f)"), *GetNameSafe(Pawn),
				*Spot.ToCompactString(), DetourLength, Best.Last().Detour);
			continue;
		}

		if (!Best.IsEmpty() && DetourLength >= Best.Last().Detour)
		{
			continue;
		}

		int32 InsertIndex = Best.Num();
		while (InsertIndex > 0 && Best[InsertIndex - 1].Detour > DetourLength)
		{
			--InsertIndex;
		}
		
		Best.Insert(FSpot(Spot, DetourLength), InsertIndex);

		if (Best.Num() > MaxCandidates)
		{
			Best.Pop();
		}
	}

	for (FSpot& BestSpot : Best)
	{
		OutSpots.Emplace(BestSpot.Location);
	}
}

void FSTT_MoveToSpot::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	if (IsValid(InstanceData.Controller) && InstanceData.Controller->GetMoveStatus() != EPathFollowingStatus::Idle)
	{
		InstanceData.Controller->StopMovement();
	}
}
