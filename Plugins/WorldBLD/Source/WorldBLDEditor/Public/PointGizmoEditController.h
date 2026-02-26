#pragma once

#include "WorldBLDEditController.h"
#include "PointGizmoEditController.generated.h"

USTRUCT(BlueprintType)
struct FPointGizmoMetadata
{
	GENERATED_BODY()

	// Display name or identifier for this point
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Point")
	FName Label { NAME_None };

	// Optional visual color for the point
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Point")
	FLinearColor Color { FLinearColor::Yellow };
};

/**
 * Edit controller that manages 5 draggable points. Points are rendered via PDI and constrained to XY.
 */
UCLASS(BlueprintType, Blueprintable)
class WORLDBLDEDITOR_API UPointGizmoEditController : public UWorldBLDEditController
{
	GENERATED_BODY()

public:
	// Initial locations for the five points (Z is preserved during drag)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PointGizmo")
	TArray<FVector> InitialPointLocations{FVector(100, 200, 0),
		FVector(200, 300, 0),
		FVector(500, 300, 0),
		FVector(500, 700, 0),
		FVector(900, 700, 0)};

	// Metadata per point
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PointGizmo")
	TArray<FPointGizmoMetadata> PointMetadata;

protected:
	// Actors representing each draggable point
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AActor>> PointActors;

	// Cached Z per point to enforce XY-only constraint
	UPROPERTY(Transient)
	TArray<float> LockedZPerPoint;

protected:
	virtual bool AllowStandardSelectionControls_Implementation() const override { return true; }
	virtual void ToolBegin_Implementation() override;
	virtual void ToolActivate_Implementation() override;
	virtual void ToolResign_Implementation() override;
	virtual void ToolExit_Implementation(bool bForceEnd) override;
	virtual void ToolRender_Implementation(FPrimitiveDrawWrapper Renderer) override;
	virtual bool ToolMouseMove_Implementation(int32 X, int32 Y) override;
	virtual void ToolHitProxyClick_Implementation(FElementHitProxyContext Element, const FKey& MouseKey) override;
	virtual void ToolActorSelectionChanged_Implementation(const TArray<AActor*>& NewlySelected, const TArray<AActor*>& Deselected, const TArray<AActor*>& CurrentSelections) override;

    virtual bool WantsCustomTransformWidget_Implementation() const override { return true; }
    virtual EAxisList::Type GetWidgetAxisMask_Implementation() const override { return EAxisList::XY; }
    virtual FVector GetDesiredWidgetLocation_Implementation() const override;

private:
	void EnsurePointsSpawned();
	void DestroyPoints();
	void UpdateActiveFromSelection(const TArray<AActor*>& CurrentSelections);
	int32 FindPointIndexByActor(const AActor* Actor) const;
};


