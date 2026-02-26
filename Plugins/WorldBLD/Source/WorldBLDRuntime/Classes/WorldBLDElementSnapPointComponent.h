// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "Components/BillboardComponent.h"
#include "IWorldBLDKitElementInterface.h"
#include "WorldBLDElementSnapPointComponent.generated.h"

UCLASS(BlueprintType, Blueprintable, ClassGroup = (WorldBLD), meta = (BlueprintSpawnableComponent))
class WORLDBLDRUNTIME_API UWorldBLDElementSnapPointComponent : public UBillboardComponent
{
	GENERATED_BODY()
public:
	UWorldBLDElementSnapPointComponent();

	///////////////////////////////////////////////////////////////////////////////////////////////

	// The unique identifier of this point in Owner. If set to NAME_None, will auto-generate a name based on component hierarchy in the owner.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SnapPoint")
	FName PointId {NAME_None};

	// Tags for this point.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SnapPoint", Meta=(Categories="WorldBLD.Detail"))
	FGameplayTagContainer PointTags;

	///////////////////////////////////////////////////////////////////////////////////////////////

	UFUNCTION(BlueprintPure, Category="SnapPoint")
	FWorldBLDKitElementPoint GetPoint() const;
};
