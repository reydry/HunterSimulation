// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "STT_MoveToSpot.generated.h"

class AAIController;

struct FEnemyView
{
	FVector EyeLocation;
	FVector EyeDirection;
	float CosHalfFOV = 60.f;
	AActor* Hunter;
	AActor* Victim;
	ECollisionChannel Channel = ECollisionChannel::ECC_Visibility;
};

USTRUCT()
struct FMoveToSpotTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	AAIController* Controller;

	UPROPERTY(EditAnywhere, Category = Parameter)
	TArray<AActor*> Hunters;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FVector Spot;

	UPROPERTY(EditAnywhere, Category = Parameter)
	TArray<FVector> SafeSpots;

	UPROPERTY(EditAnywhere, Category = Parameter)
	float AcceptanceRadius;

	bool bAtTarget;

	TArray<FVector> VisitedSpots;

	FVector CurrentSpot;
};

USTRUCT()
struct HUNTERSIMULATION_API FSTT_MoveToSpot : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()
	
	using FInstanceDataType = FMoveToSpotTaskInstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
public:

protected:
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

private:
	bool IsPathExposed(UWorld* World, AActor* Pawn, FVector& Start, FVector& End, TArray<AActor*>& Hunters) const;
	bool IsPathSegmentExposed(FVector& Start, FVector& End, AActor* Hunter, float HalfFOV) const;
	bool IsPathSegmentExposed2D(FVector& Start, FVector& End, FVector& EyeLocation, FVector EyeDirection, float HalfFOV) const;

	void AddVisited(FVector& Spot, FInstanceDataType& InstanceData) const;

	bool Replan(FInstanceDataType& InstanceData) const;
	void PickSpots(FVector& Start, FInstanceDataType& InstanceData, TArray<FVector>& OutSpots) const;
};
