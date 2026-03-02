#include "PointGizmoEditController.h"

#include "Editor.h"
#include "Components/SceneComponent.h"
#include "Components/BillboardComponent.h"
#include "PrimitiveDrawWrapper.h"

namespace
{
	static const int32 kNumPoints = 5;
	static const float kPointDrawSize = 12.0f;
}

void UPointGizmoEditController::ToolBegin_Implementation()
{
	Super::ToolBegin_Implementation();
	EnsurePointsSpawned();
	ForceEditorSelectionToActiveBrushActor();
}

void UPointGizmoEditController::ToolActivate_Implementation()
{
	Super::ToolActivate_Implementation();
	EnsurePointsSpawned();
	ForceEditorSelectionToActiveBrushActor();
}

void UPointGizmoEditController::ToolResign_Implementation()
{
	Super::ToolResign_Implementation();
}

void UPointGizmoEditController::ToolExit_Implementation(bool bForceEnd)
{
	DestroyPoints();
	Super::ToolExit_Implementation(bForceEnd);
}

void UPointGizmoEditController::EnsurePointsSpawned()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return;
	}

	if (InitialPointLocations.Num() == 0)
	{
		InitialPointLocations.SetNum(kNumPoints);
		for (int32 i = 0; i < kNumPoints; ++i)
		{
			InitialPointLocations[i] = FVector(100.0f * i, 0.0f, 0.0f);
		}
	}

	if (PointMetadata.Num() == 0)
	{
		PointMetadata.SetNum(kNumPoints);
		for (int32 i = 0; i < kNumPoints; ++i)
		{
			PointMetadata[i].Label = *FString::Printf(TEXT("Point_%d"), i);
		}
	}

    // Spawn actors if missing (may clear LockedZPerPoint inside DestroyPoints)
    if (PointActors.Num() != kNumPoints)
    {
        DestroyPoints();
        PointActors.SetNum(kNumPoints);
    }

    // Ensure Z cache sized AFTER any DestroyPoints() call
    LockedZPerPoint.SetNum(kNumPoints);

	for (int32 i = 0; i < kNumPoints; ++i)
	{
		if (!PointActors[i].IsValid())
		{
			FActorSpawnParameters SpawnInfo;
			SpawnInfo.ObjectFlags |= RF_Transactional | RF_Transient;
			SpawnInfo.Name = MakeUniqueObjectName(World, AActor::StaticClass(), *FString::Printf(TEXT("PointGizmo_Brush_%d"), i));
			AActor* Brush = World->SpawnActor<AActor>(SpawnInfo);
			if (Brush)
			{
				if (!Brush->GetRootComponent())
				{
					USceneComponent* Root = NewObject<USceneComponent>(Brush, TEXT("Root"));
					Root->SetMobility(EComponentMobility::Movable);
					Brush->SetRootComponent(Root);
					Root->RegisterComponent();
				}
				Brush->SetActorLocation(InitialPointLocations.IsValidIndex(i) ? InitialPointLocations[i] : FVector::ZeroVector);
				LockedZPerPoint[i] = Brush->GetActorLocation().Z;

				PointActors[i] = Brush;
			}
		}
	}

    // Default active selection is first point (guard size)
    if (PointActors.Num() > 0 && PointActors[0].IsValid())
    {
        ActiveBrushActor = PointActors[0].Get();
    }
}

void UPointGizmoEditController::DestroyPoints()
{
	for (TWeakObjectPtr<AActor>& P : PointActors)
	{
		if (P.IsValid())
		{
			P->Destroy();
		}
	}
	PointActors.Empty();
	LockedZPerPoint.Empty();
	ActiveBrushActor = nullptr;
}

void UPointGizmoEditController::ToolRender_Implementation(FPrimitiveDrawWrapper Renderer)
{
	for (int32 i = 0; i < PointActors.Num(); ++i)
	{
		if (AActor* A = PointActors[i].Get())
		{
			const FLinearColor Color = PointMetadata.IsValidIndex(i) ? PointMetadata[i].Color : FLinearColor::Yellow;
			FPrimitiveDrawParams Params(Color, kPointDrawSize);
			FElementHitProxyContext Ctx; Ctx.Id = i; Ctx.Object = nullptr; Ctx.String = TEXT("Point");
			UPrimitiveDrawWrapperUtils::PDI_SetHitProxy(Renderer, Ctx);
			UPrimitiveDrawWrapperUtils::PDI_DrawPoint(Renderer, Params, A->GetActorLocation());
			UPrimitiveDrawWrapperUtils::PDI_ClearHitProxy(Renderer);
		}
	}
}

bool UPointGizmoEditController::ToolMouseMove_Implementation(int32 X, int32 Y)
{
	// Enforce XY constraint by snapping Z back to the locked Z for whichever point is selected
	if (ActiveBrushActor)
	{
		const int32 Index = FindPointIndexByActor(ActiveBrushActor);
		if (LockedZPerPoint.IsValidIndex(Index) && Index >= 0)
		{
			FVector Loc = ActiveBrushActor->GetActorLocation();
			Loc.Z = LockedZPerPoint[Index];
			ActiveBrushActor->SetActorLocation(Loc);
		}
	}
	return true;
}

void UPointGizmoEditController::ToolHitProxyClick_Implementation(FElementHitProxyContext Element, const FKey& MouseKey)
{
	const int32 Index = Element.Id;
	if (PointActors.IsValidIndex(Index) && PointActors[Index].IsValid())
	{
		ActiveBrushActor = PointActors[Index].Get();
		bActiveBrushIsForceSelected = true;
		ForceEditorSelectionToActiveBrushActor();
	}
}

void UPointGizmoEditController::ToolActorSelectionChanged_Implementation(const TArray<AActor*>& NewlySelected,
		const TArray<AActor*>& Deselected, const TArray<AActor*>& CurrentSelections)
{
	UpdateActiveFromSelection(CurrentSelections);
	ForceEditorSelectionToActiveBrushActor();
}

void UPointGizmoEditController::UpdateActiveFromSelection(const TArray<AActor*>& CurrentSelections)
{
	for (AActor* Sel : CurrentSelections)
	{
		const int32 Index = FindPointIndexByActor(Sel);
		if (Index >= 0)
		{
			ActiveBrushActor = Sel;
			return;
		}
	}
}

int32 UPointGizmoEditController::FindPointIndexByActor(const AActor* Actor) const
{
	for (int32 i = 0; i < PointActors.Num(); ++i)
	{
		if (PointActors[i].IsValid() && PointActors[i].Get() == Actor)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

FVector UPointGizmoEditController::GetDesiredWidgetLocation_Implementation() const
{
	if (ActiveBrushActor)
	{
		return ActiveBrushActor->GetActorLocation();
	}
	return FVector::ZeroVector;
}


