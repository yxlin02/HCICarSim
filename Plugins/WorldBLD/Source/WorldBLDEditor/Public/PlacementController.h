// Copyright WorldBLD. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorldBLDEditController.h"
#include "WorldBLDPrefab.h"
#include "PlacementController.generated.h"

class UMaterialInterface;
struct FCollisionQueryParams;

/**
 * Controller for placing WorldBLDPrefab actors in the world.
 */
UCLASS(BlueprintType, Blueprintable)
class WORLDBLDEDITOR_API UPlacementController : public UWorldBLDEditController
{
	GENERATED_BODY()

public:
	/** The prefab class to spawn */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Placement", meta=(BlueprintSetter="SetPrefabClass"))
	TSubclassOf<AWorldBLDPrefab> PrefabClass;

	/** Applied to the preview brush when there are no overlaps (purely visual feedback). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Placement|Validation")
	TObjectPtr<UMaterialInterface> ValidMaterial = nullptr;

	/** Applied to the preview brush when there are overlaps (purely visual feedback). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Placement|Validation")
	TObjectPtr<UMaterialInterface> InvalidMaterial = nullptr;

	/**
	 * Vertical clearance (cm) applied to the overlap-check bounds so the preview resting on the ground doesn't
	 * count as "overlapping" solely due to the floor collision.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Placement|Validation", meta=(ClampMin="0.0"))
	float OverlapCheckBottomClearance = 100.0f;

	/** Updates the prefab class and refreshes the preview actor if the tool is active */
	UFUNCTION(BlueprintCallable, Category="Placement")
	void SetPrefabClass(TSubclassOf<AWorldBLDPrefab> InPrefabClass);

	/** Whether to exit the tool after placing a single actor */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Placement")
	bool bExitToolOnPlacement;

	/**
	 * Creates a new Placement Controller with the specified settings.
	 * @param InPrefabClass The class of the prefab to place
	 * @param bInExitToolOnPlacement Whether to exit the tool after placing one actor
	 * @return The new Placement Controller
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Placement")
	static UPlacementController* CreatePlacementController(TSubclassOf<AWorldBLDPrefab> InPrefabClass, bool bInExitToolOnPlacement = false);

protected:
	virtual void ToolBegin_Implementation() override;
	virtual void ToolExit_Implementation(bool bForceEnd) override;
	virtual bool ToolMouseMove_Implementation(int32 X, int32 Y) override;
	virtual void ToolRender_Implementation(FPrimitiveDrawWrapper Renderer) override;
	virtual bool ConsumesMouseClick_Implementation(int32 Which, bool& bIsPassive) const override;
	virtual void ToolMouseClick_Implementation(int32 WhichButton, bool bPressed) override;
	virtual bool ConsumesMouseScroll_Implementation() const override { return true; }
	virtual void ToolMouseScroll_Implementation(float ScrollDelta, bool bControl, bool bAlt, bool bShift) override;
	virtual bool PressingEscapeTriggersExit_Implementation() const override { return true; }
	virtual bool AllowStandardSelectionControls_Implementation() const override { return false; }
	virtual void AddIgnoredActorsForMouseTrace(FCollisionQueryParams& TraceParams) const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	/** Spawns a new preview actor */
	void SpawnPreviewActor();

	/** Refreshes (destroys + respawns) the preview actor for the current PrefabClass */
	void RefreshPreviewActorForPrefabClass();

	/** Current Z offset applied to the placement */
	float PlacementZOffset = 0.0f;

	/** The rotation of the last placed actor, used for continuous placement */
	FRotator LastPlacementRotation = FRotator::ZeroRotator;

#if WITH_EDITOR
	/** Updates preview validity feedback (overlap -> invalid material, else valid material). */
	void UpdatePerFramePreviewValidity();

	/** Tracks whether we last considered the preview to be overlapping. */
	bool bHasCachedOverlapState = false;
	bool bCachedHadOverlap = false;

	/** Original materials for preview brush components so we can restore before final placement. */
	TMap<TWeakObjectPtr<class UMeshComponent>, TArray<TObjectPtr<UMaterialInterface>>> PreviewOriginalMaterials;

	void ResetPreviewValidityCache();
	void ClearPreviewOriginalMaterials();
	void CacheOriginalMaterialsForActorHierarchy(AActor* RootActor);
	void ApplyOverrideMaterialToActorHierarchy(AActor* RootActor, UMaterialInterface* OverrideMaterial);
	void RestoreOriginalMaterialsForCachedComponents();

	bool PreviewHasBlockingOverlap() const;
	static void GetActorHierarchy(AActor* RootActor, TArray<AActor*>& OutActors);
	static bool ShouldIgnoreOverlapActor(const AActor* OtherActor);
#endif
};
