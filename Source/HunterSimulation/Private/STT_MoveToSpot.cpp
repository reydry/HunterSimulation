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

static TAutoConsoleVariable<int32> CVarDrawDebugRoutes(
	TEXT("ai.debug.routes"), 
	0,
	TEXT("0 = off, 1 = active route (green = concealed, red = exposed), 2 = also the alternatives considered at the last replan, with verdicts"),
	ECVF_Default);

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

	if (CVarDrawDebugRoutes.GetValueOnGameThread() > 0 && InstanceData.CurrentSpot.X < FLT_MAX)
	{
		UWorld* World = Owner->GetWorld();

		if (FMath::Fmod(World->GetTimeSeconds(), 0.25f) < DeltaTime)
		{
			DebugDrawRoute(World, Owner, ActorLocation, InstanceData.CurrentSpot, InstanceData.Hunters, 4.f, 0.3f,
				FString::Printf(TEXT("%s ACTIVE -> %s"), *GetNameSafe(Owner),
					InstanceData.bAtTarget ? TEXT("final spot") : TEXT("intermediate spot")));

			if (!InstanceData.bAtTarget)
			{
				DrawDebugLine(World, InstanceData.CurrentSpot + FVector(0, 0, 25.f), InstanceData.Spot + FVector(0, 0, 25.f),
					FColor(150, 150, 150), false, 0.3f, 0, 1.f);
			}
		}
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

	UNavigationPath* Path = NavigationSystem->FindPathToLocationSynchronously(World, Start, End, Pawn);
	
	if (!IsValid(Path))
	{
		return true;
	}

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

	const bool bExposed = IsPathSegmentExposed2D(Start, End, EyeLocation, EyeRotation.Vector(), HalfFOV);

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

		bool bFoundSafeSpot = false;

		for (FVector& Spot : PossibleSpots)
		{
			const bool bExposed = IsPathExposed(World, Owner, Start, Spot, InstanceData.Hunters);

			if (!bExposed)
			{
				Target = Spot;
				bFoundSafeSpot = true;
				break;
			}
		}

		DebugDrawDecision(World, Owner, Start, InstanceData.Spot, InstanceData.Hunters, PossibleSpots, Target, true);
	}

	bool bIsTarget = Target.Equals(InstanceData.Spot, 1.f);

	if (Target.Equals(InstanceData.CurrentSpot, 1.f) && Controller->GetMoveStatus() == EPathFollowingStatus::Moving)
	{
		InstanceData.bAtTarget = bIsTarget;
		return true;
	}

	InstanceData.CurrentSpot = Target;
	InstanceData.bAtTarget = bIsTarget;

	EPathFollowingRequestResult::Type Result = Controller->MoveToLocation(Target, InstanceData.AcceptanceRadius, false, true, true);

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
	const APawn* Pawn = IsValid(InstanceData.Controller) ? InstanceData.Controller->GetPawn() : nullptr;

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
			continue;
		}
		
		if (Best.Num() >= MaxCandidates && DetourLength >= Best.Last().Detour)
		{
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

FRouteDebugInfo FSTT_MoveToSpot::DebugDrawRoute(UWorld* World, AActor* Pawn, const FVector& Start, const FVector& End,
	TArray<AActor*>& Hunters, float Thickness, float Duration, const FString& Label) const
{
	FRouteDebugInfo Info;

	const FVector Lift(0.f, 0.f, 25.f);

	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(World);
	UNavigationPath* Path = NavSys ? NavSys->FindPathToLocationSynchronously(World, Start, End, Pawn) : nullptr;

	if (!Path || Path->PathPoints.Num() < 2)
	{
		DrawDebugLine(World, Start + Lift, End + Lift, FColor::Magenta, false, Duration, 0, 1.f);
		DrawDebugString(World, End + Lift * 6.f, FString::Printf(TEXT("%s | NO NAV PATH"), *Label), nullptr, FColor::Magenta, Duration, true);
		return Info;
	}

	Info.bPathFound = true;
	const TArray<FVector>& Points = Path->PathPoints;
	Info.SegmentCount = Points.Num() - 1;

	for (int32 i = 0; i < Info.SegmentCount; ++i)
	{
		FVector A = Points[i];
		FVector B = Points[i + 1];
		const float Length = FVector::Dist(A, B);

		FString SeenBy;
		for (AActor* Hunter : Hunters)
		{
			if (IsPathSegmentExposed(A, B, Hunter, 60))
			{
				SeenBy = GetNameSafe(Hunter);
				break;
			}
		}

		const bool bExposed = !SeenBy.IsEmpty();
		const FColor Color = bExposed ? FColor::Red : FColor::Green;

		DrawDebugLine(World, A + Lift, B + Lift, Color, false, Duration, 0, Thickness);
		DrawDebugSphere(World, B + Lift, Thickness * 4.f, 8, Color, false, Duration);

		Info.TotalLength += Length;

		if (bExposed)
		{
			++Info.ExposedSegmentCount;
			Info.ExposedLength += Length;

			if (Info.FirstSeenBy.IsEmpty())
			{
				Info.FirstSeenBy = SeenBy;
			}

			DrawDebugString(World, (A + B) * 0.5f + Lift * 3.f, FString::Printf(TEXT("EXPOSED (seen by %s)"), *SeenBy),
				nullptr, FColor::Red, Duration, true);
		}
	}

	DrawDebugString(World, End + Lift * 6.f,
		FString::Printf(TEXT("%s | %s | exposed %d/%d segs, %.0f/%.0f uu"), *Label,
			Info.IsExposed() ? TEXT("EXPOSED") : TEXT("CONCEALED"),
			Info.ExposedSegmentCount, Info.SegmentCount, Info.ExposedLength, Info.TotalLength),
		nullptr, Info.IsExposed() ? FColor::Red : FColor::Green, Duration, true);

	return Info;
}

void FSTT_MoveToSpot::DebugDrawDecision(UWorld* World, AActor* Pawn, const FVector& Start, const FVector& FinalSpot,
	TArray<AActor*>& Hunters, const TArray<FVector>& Candidates, const FVector& Chosen, bool bDirectExposed) const
{
	if (CVarDrawDebugRoutes.GetValueOnGameThread() < 2)
	{
		return;
	}

	const float Duration = 5.f;
	const FVector Lift(0.f, 0.f, 25.f);

	const FRouteDebugInfo Direct = DebugDrawRoute(World, Pawn, Start, FinalSpot, Hunters, 1.5f, Duration, TEXT("DIRECT"));

	int32 ChosenIndex = INDEX_NONE;
	for (int32 i = 0; i < Candidates.Num(); ++i)
	{
		if (Candidates[i].Equals(Chosen, 1.f))
		{
			ChosenIndex = i;
			break;
		}
	}

	FString FinalText = bDirectExposed
		? FString::Printf(TEXT("direct route rejected: exposed (seen by %s, %d/%d segs)"), *Direct.FirstSeenBy, Direct.ExposedSegmentCount, Direct.SegmentCount)
		: FString(TEXT("direct route accepted: concealed from all hunters"));

	if (bDirectExposed && ChosenIndex == INDEX_NONE)
	{
		FinalText += FString::Printf(TEXT("\nFALLBACK: none of %d candidate(s) is concealed, going to the final spot anyway"), Candidates.Num());
	}

	DrawDebugString(World, FinalSpot + Lift * 14.f, FinalText, nullptr, bDirectExposed ? FColor::Red : FColor::Green, Duration, true);

	const FVector2D Start2D(Start);
	const FVector2D Final2D(FinalSpot);

	for (int32 i = 0; i < Candidates.Num(); ++i)
	{
		const FVector& Spot = Candidates[i];
		const bool bIsChosen = (i == ChosenIndex);

		const FRouteDebugInfo Info = DebugDrawRoute(World, Pawn, Start, Spot, Hunters, bIsChosen ? 3.f : 1.5f, Duration,
			FString::Printf(TEXT("ALT #%d"), i + 1));

		const float Detour = FVector2D::Distance(Start2D, FVector2D(Spot)) + FVector2D::Distance(FVector2D(Spot), Final2D);

		FString Verdict;
		FColor Color;

		if (bIsChosen)
		{
			Verdict = FString::Printf(TEXT("#%d CHOSEN: first candidate (smallest detour %.0f) whose route is concealed"), i + 1, Detour);
			Color = FColor::Cyan;
		}
		else if (ChosenIndex != INDEX_NONE && i > ChosenIndex)
		{
			Verdict = FString::Printf(TEXT("#%d not evaluated by the planner (#%d was accepted first); would be %s, detour %.0f"),
				i + 1, ChosenIndex + 1, Info.IsExposed() ? TEXT("EXPOSED") : TEXT("CONCEALED"), Detour);
			Color = FColor(150, 150, 150);
		}
		else
		{
			Verdict = FString::Printf(TEXT("#%d rejected: route exposed (seen by %s, %d/%d segs), detour %.0f"),
				i + 1, *Info.FirstSeenBy, Info.ExposedSegmentCount, Info.SegmentCount, Detour);
			Color = FColor::Orange;
		}

		DrawDebugSphere(World, Spot + Lift, bIsChosen ? 40.f : 24.f, 12, Color, false, Duration, 0, 2.f);
		DrawDebugString(World, Spot + Lift * 10.f, Verdict, nullptr, Color, Duration, true);
	}
}