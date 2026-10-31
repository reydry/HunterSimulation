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

EStateTreeRunStatus FSTT_MoveToSpot::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
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

	bool bTargetReached = FVector::Dist2D(Owner->GetActorLocation(), InstanceData.Spot) <= InstanceData.AcceptanceRadius;

	if (bTargetReached)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	if (!InstanceData.bAtTarget && bTargetReached)
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
		return false;
	}

	UNavigationPath* Path = NavigationSystem->FindPathToLocationSynchronously(World, Pawn->GetActorLocation(), End, Pawn);
	TArray<FVector> PathPoints = Path->PathPoints;

	for (AActor* Hunter : Hunters)
	{
		for(int32 i = 0; i < PathPoints.Num() - 1; ++i)
		{
			if (IsPathSegmentExposed(PathPoints[i], PathPoints[i + 1], Hunter, 60))
			{
				return true;
			}
		}
	}
	return false;
}

bool FSTT_MoveToSpot::IsPathSegmentExposed(FVector& Start, FVector& End, AActor* Hunter, float HalfFOV) const
{
	if (!IsValid(Hunter))
	{
		return false;
	}

	FVector EyeLocation = FVector::ZeroVector;
	FRotator EyeRotation = FRotator::ZeroRotator;

	Hunter->GetActorEyesViewPoint(EyeLocation, EyeRotation);

	return IsPathSegmentExposed2D(Start, End, EyeLocation, EyeRotation.Vector(), HalfFOV);
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
}

bool FSTT_MoveToSpot::Replan(FInstanceDataType& InstanceData) const
{
	AAIController* Controller = InstanceData.Controller;
	APawn* Owner = Controller ? Controller->GetPawn() : nullptr;
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;

	if (!IsValid(World))
	{
		return false;
	}

	FVector Target = InstanceData.Spot;
	FVector Start = Owner->GetActorLocation();
	
	if (IsPathExposed(World, Owner, Start, Target, InstanceData.Hunters))
	{
		TArray<FVector> PossibleSpots;
		PickSpots(Start, InstanceData, PossibleSpots);

		for (FVector& Spot : PossibleSpots)
		{
			if (!IsPathExposed(World, Owner, Start, Spot, InstanceData.Hunters))
			{
				Target = Spot;
				break;
			}
		}
	}

	bool bIsTarget = Target.Equals(InstanceData.Spot, 1.f);

	if (Target.Equals(InstanceData.CurrentSpot, 1.f) && Controller->GetMoveStatus() == EPathFollowingStatus::Moving)
	{
		InstanceData.bAtTarget = bIsTarget;
		return true;
	}

	InstanceData.CurrentSpot = Target;
	InstanceData.bAtTarget = bIsTarget;

	EPathFollowingRequestResult::Type Result = Controller->MoveToLocation(Target, InstanceData.AcceptanceRadius, true, true, true);

	if (Result != EPathFollowingRequestResult::Type::Failed)
	{
		return true;
	}

	if (bIsTarget)
	{
		return false;
	}

	AddVisited(Target, InstanceData);

	return false;
}

void FSTT_MoveToSpot::PickSpots(FVector& Start, FInstanceDataType& InstanceData, TArray<FVector>& OutSpots) const
{
	OutSpots.Reset();

	struct FSpot
	{
		FVector Location;
		float Detour;
	};

	TArray<FSpot> Best;
	Best.Reserve(5);

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
			continue;
		}
		
		if (DetourLength >= Best.Last().Detour)
		{
			continue;
		}

		int32 InsertIndex = Best.Num();
		while (InsertIndex > 0 && Best[InsertIndex - 1].Detour > DetourLength)
		{
			--InsertIndex;
		}
		
		Best.Insert(FSpot(Spot, DetourLength), InsertIndex);
		
		for (FSpot& BestSpot : Best)
		{
			OutSpots.Emplace(BestSpot.Location);
		}
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
