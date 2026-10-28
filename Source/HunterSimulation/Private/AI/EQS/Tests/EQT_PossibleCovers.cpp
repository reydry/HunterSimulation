// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/EQS/Tests/EQT_PossibleCovers.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"

UEQT_PossibleCovers::UEQT_PossibleCovers(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Cost = EEnvTestCost::High;
	ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
	SetWorkOnFloatValues(false);
}

void UEQT_PossibleCovers::RunTest(FEnvQueryInstance& QueryInstance) const
{
	UObject* QueryOwner = QueryInstance.Owner.Get();

	if (!IsValid(QueryOwner))
	{
		return;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(QueryOwner, EGetWorldErrorMode::ReturnNull);

	if (!IsValid(World))
	{
		return;
	}

	BoolValue.BindData(QueryOwner, QueryInstance.QueryID);

	for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
	{
		FVector Origin = GetItemLocation(QueryInstance, It.GetIndex()) + FVector(0, 0, TraceHeight);
		bool bNearCover = false;

		for (int32 i = 0; i < NumRays && !bNearCover; ++i)
		{
			FVector Direction = FRotator(0.f, 360.f / NumRays * i, 0.f).RotateVector(FVector::ForwardVector);
			bNearCover = World->LineTraceTestByChannel(Origin, Origin + Direction * DistanceToWall, TraceChannel);
		}
		
		It.SetScore(TestPurpose, FilterType, bNearCover, BoolValue.GetValue());
	}
}
