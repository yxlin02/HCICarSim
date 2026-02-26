// Copyright WorldBLD LLC. All rights reserved.

#include "WorldBLDEditController.h"

#include "WorldBLDEditorToolkit.h"
#include "WorldBLDKitElementUtils.h"
#include "WorldBLDToolPaletteWidget.h"
#include "WorldBLDKitEditorUtils.h"
#include "WorldBLDViewportWidgetInterface.h"
#include "ViewportButtonBase.h"

#include "Engine/Selection.h"
#include "GameFramework/Actor.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "LevelEditor.h"
#include "LevelEditorViewport.h"
#include "SLevelViewport.h"
#include "Modules/ModuleManager.h"
#include "SceneView.h"
#include "EditorViewportClient.h"
#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"

///////////////////////////////////////////////////////////////////////////////////////////////////

void UWorldBLDEditController::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyStylesToChildWidgets();
};

void UWorldBLDEditController::ToolBegin_Implementation()
{
	bHasBegun = true;
	ToolActivate();
	OnToolBegin.Broadcast(this);
}

void UWorldBLDEditController::ToolActivate_Implementation()
{
	bIsActive = true;
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	
	// Force focus to this controller.
	SetIsFocusable(true);
	if (GetCachedWidget() != nullptr)
	{
		FSlateApplication::Get().SetKeyboardFocus(GetCachedWidget());
	}
}


void UWorldBLDEditController::RenderInView(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	FPrimitiveDrawWrapper Renderer;
	Renderer.PDI = PDI;

	PDI->SetHitProxy(nullptr);
	ToolRender(Renderer);
	PDI->SetHitProxy(nullptr);
}

void UWorldBLDEditController::ToolResign_Implementation()
{
	SetVisibility(ESlateVisibility::Hidden);
	bIsActive = false;
	CleanupSelection();
}

void UWorldBLDEditController::CleanupSelection() const
{
	USelection* EditorSelections = GEditor->GetSelectedActors();
	if (IsValid(ActiveBrushActor) && EditorSelections->IsSelected(ActiveBrushActor))
	{
		EditorSelections->Deselect(ActiveBrushActor);
	}
}

void UWorldBLDEditController::TryExitTool_Implementation(bool bForceEnd)
{
	ToolExit(bForceEnd);
}

void UWorldBLDEditController::ToolExit_Implementation(bool bForceEnd)
{
	bHasBegun = false;
	if (UWorldBLDEdMode* EdMode = UWorldBLDEdMode::GetWorldBLDEdMode())
	{
		EdMode->PopEditController(this);
	}

	CleanupSelection();
	if (IsValid(ActiveBrushActor))
	{
		UWorldBLDKitElementUtils::WorldBLDKitElement_DemolishAndDestroy(ActiveBrushActor);
		OnBrushActorDestroyed(ActiveBrushActor);
		ActiveBrushActor = nullptr;
	}
	OnToolExit.Broadcast(this);
}

bool UWorldBLDEditController::AddToEditorLevelViewport()
{
	bool bSuccess = false;

	// SEE: https://forums.unrealengine.com/t/how-to-draw-hud-in-editor/58807
	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
    
    TWeakPtr<ILevelEditor> Editor = LevelEditor.GetLevelEditorInstance();
    TSharedPtr<ILevelEditor> PinnedEditor(Editor.Pin());
    
    if (PinnedEditor.IsValid())
    {
		if (TSharedPtr<SLevelViewport> ActiveViewport = PinnedEditor->GetActiveViewportInterface())
		{
			TSharedPtr<SViewport> ViewportWidget = ActiveViewport->GetViewportWidget().Pin();
			if (FChildren* Children = ViewportWidget->GetChildren())
			{
				for (int32 Idx = 0; Idx < Children->Num(); Idx += 1)
				{
					TSharedPtr<SWidget> Child = Children->GetChildAt(Idx);
					if (Child->GetType() == TEXT("SOverlay"))
					{
						TSharedPtr<SOverlay> Overlay = StaticCastSharedPtr<SOverlay>(Child);
						// Add the widget to the overlay.
						Overlay->AddSlot()
							.VAlign(VAlign_Fill)
							.HAlign(HAlign_Fill)
							[
								TakeWidget()
							];
						bSuccess = true;
						break;
					}
				}
			}        
		}
    }

	return bSuccess;
}

void UWorldBLDEditController::RemoveFromEditorLevelViewport()
{
	// SEE: https://forums.unrealengine.com/t/how-to-draw-hud-in-editor/58807
	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
    
    TWeakPtr<ILevelEditor> Editor = LevelEditor.GetLevelEditorInstance();
    TSharedPtr<ILevelEditor> PinnedEditor(Editor.Pin());
    
    if (PinnedEditor.IsValid())
    {
		if (TSharedPtr<SLevelViewport> ActiveViewport = PinnedEditor->GetActiveViewportInterface())
		{
			TSharedPtr<SViewport> ViewportWidget = ActiveViewport->GetViewportWidget().Pin();
			if (FChildren* Children = ViewportWidget->GetChildren())
			{
				for (int32 Idx = 0; Idx < Children->Num(); Idx += 1)
				{
					TSharedPtr<SWidget> Child = Children->GetChildAt(Idx);
					if (Child->GetType() == TEXT("SOverlay"))
					{
						TSharedPtr<SOverlay> Overlay = StaticCastSharedPtr<SOverlay>(Child);
						Overlay->RemoveSlot(TakeWidget());
						break;
					}
				}
			}        
		}
    }
}

bool UWorldBLDEditController::ForceEditorSelectionToActiveBrushActor()
{
	if (bForceSelectionGuard || !bIsActive)
	{
		return false;
	}

	bForceSelectionGuard = true;
	USelection* EditorSelections = GEditor->GetSelectedActors();
	bool bHandled = false;
	if (IsValid(ActiveBrushActor) && (!EditorSelections->IsSelected(ActiveBrushActor) || (EditorSelections->CountSelections<AActor>() != 1)))
	{
		GEditor->SelectNone(/* Note Change? */ true, /* Deselect BSPs Surfaces? */ true);
		if (bActiveBrushIsForceSelected)
		{
			GEditor->SelectActor(ActiveBrushActor, /* Selected? */ true, /* Notify? */ true, /* Select Even if Hidden? */ true);
			ActiveBrushActor->OnDestroyed.AddUniqueDynamic(this, &UWorldBLDEditController::OnBrushActorDestroyed);
			bHandled = true;
		}
	}
	bForceSelectionGuard = false;
	return bHandled;
}

void UWorldBLDEditController::OnActorSelectionSetChanged()
{
	USelection* EditorSelections = GEditor->GetSelectedActors();
	TArray<UObject*> SelectedObjects;
	EditorSelections->GetSelectedObjects(AActor::StaticClass(), SelectedObjects);

	// Cleanup dead tracked actors
	for (int32 Idx = 0; Idx < TrackedSelections.Num(); Idx += 1)
	{
		if (!TrackedSelections[Idx].IsValid())
		{
			TrackedSelections.RemoveAtSwap(Idx);
			Idx -= 1;
		}
	}

	using FWeakActorSet = TSet<TWeakObjectPtr<AActor>>;
	TArray<TWeakObjectPtr<AActor>> CurrentSelections;
	for (UObject* Object : SelectedObjects)
	{
		CurrentSelections.Add((AActor*)Object);
	}

	TArray<TWeakObjectPtr<AActor>> NewSelections = FWeakActorSet(CurrentSelections).Difference(FWeakActorSet(TrackedSelections)).Array();
	TArray<TWeakObjectPtr<AActor>> RemovedSelections = FWeakActorSet(TrackedSelections).Difference(FWeakActorSet(CurrentSelections)).Array();
	TArray<AActor*> StrongNewSelections;
	TArray<AActor*> StrongRemovedSelections;
	TArray<AActor*> StrongCurrentSelections;

	auto ToStrongArray = [](const TArray<TWeakObjectPtr<AActor>>& WeakArray, TArray<AActor*>& OutStrongArray)
	{
		for (auto Item : WeakArray)
		{
			OutStrongArray.Add(Item.Get());
		}
	};

	ToStrongArray(NewSelections, StrongNewSelections);
	ToStrongArray(RemovedSelections, StrongRemovedSelections);
	ToStrongArray(CurrentSelections, StrongCurrentSelections);

	ToolActorSelectionChanged(StrongNewSelections, StrongRemovedSelections, StrongCurrentSelections);

	if (UWorldBLDEdMode* EdMode = UWorldBLDEdMode::GetWorldBLDEdMode())
	{
		if (UWorldBLDToolPaletteWidget* ToolPalette = EdMode->GetActiveToolPalette())
		{
			ToolPalette->ToolPaletteActorSelectionChanged(this, StrongNewSelections, 
					StrongRemovedSelections, StrongCurrentSelections);
		}
	}
}

void UWorldBLDEditController::OnBrushActorDestroyed(AActor* DestroyedActor)
{
	CleanupSelection();
}

bool UWorldBLDEditController::EditorViewportHasFocus()
{
	return GCurrentLevelEditingViewportClient ? GCurrentLevelEditingViewportClient->Viewport->HasFocus() : false;
}

UEditorActorSubsystem* UWorldBLDEditController::InternalGetEditorActorSubsystem() const
{
	return GEditor->GetEditorSubsystem<UEditorActorSubsystem>();
}

void UWorldBLDEditController::ApplyStylesToChildWidgets()
{
	if (!WidgetStyle && !AlternateStyle)
	{
		return;
	}

	TGuardValue<bool> AutoRestore(GAllowActorScriptExecutionInEditor, true);

	// Find all widgets implementing IWorldBLDViewportWidgetInterface
	TArray<UWidget*> StyledWidgets;
	UWorldBLDKitEditorUtils::GetAllWidgetsWithInterface(this, UWorldBLDViewportWidgetInterface::StaticClass(), StyledWidgets);

	// Apply styles to each widget based on its StyleName property
	for (UWidget* Widget : StyledWidgets)
	{
		if (IWorldBLDViewportWidgetInterface* StyledWidget = Cast<IWorldBLDViewportWidgetInterface>(Widget))
		{
			const UObject* StyleRoot = WidgetStyle;
			if (const UViewportButtonBase* ViewportButton = Cast<UViewportButtonBase>(Widget))
			{
				if (ViewportButton->bUseAlternateStyle && AlternateStyle)
				{
					StyleRoot = AlternateStyle;
				}
			}

			// Get the widget's style name
			FName StyleName;
			IWorldBLDViewportWidgetInterface::Execute_GetStyleName(Widget, StyleName);
			
			// If the widget has a style name, look up the corresponding style object
			if (StyleName != NAME_None)
			{
				const UObject* StyleObj = UWorldBLDKitEditorUtils::GetStyleObjectByName(StyleRoot, StyleName);
				if (!StyleObj && StyleRoot == AlternateStyle && WidgetStyle)
				{
					// Fallback to primary style if alternate style root doesn't define this style name.
					StyleObj = UWorldBLDKitEditorUtils::GetStyleObjectByName(WidgetStyle, StyleName);
				}
				if (StyleObj)
				{
					// Apply the style to the widget
					IWorldBLDViewportWidgetInterface::Execute_SetWidgetStyle(Widget, StyleObj);
				}
			}
		}
	}
}

void UWorldBLDEditController::AddIgnoredActorsForMouseTrace(FCollisionQueryParams& TraceParams) const
{
	if (IsValid(ActiveBrushActor))
	{
		TraceParams.AddIgnoredActor(ActiveBrushActor);
	}
}

FVector UWorldBLDEditController::GetMouseHitPoint(int32 X, int32 Y) const
{
	// Get the active viewport client
	if (!GEditor)
	{
		return FVector::ZeroVector;
	}
	
	FViewport* Viewport = nullptr;
	FEditorViewportClient* ViewportClient = nullptr;
	
	// Find the active viewport
	for (FLevelEditorViewportClient* LevelVC : GEditor->GetLevelViewportClients())
	{
		if (LevelVC && LevelVC->Viewport && LevelVC->Viewport->HasFocus())
		{
			ViewportClient = LevelVC;
			Viewport = LevelVC->Viewport;
			break;
		}
	}
	
	// If no viewport has focus, try to use the first available one
	if (!ViewportClient && GEditor->GetLevelViewportClients().Num() > 0)
	{
		ViewportClient = GEditor->GetLevelViewportClients()[0];
		if (ViewportClient)
		{
			Viewport = ViewportClient->Viewport;
		}
	}
	
	if (!ViewportClient || !Viewport)
	{
		return FVector::ZeroVector;
	}
	
	// Get the world
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return FVector::ZeroVector;
	}
	
	// Create a scene view to deproject the mouse coordinates
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		Viewport,
		ViewportClient->GetScene(),
		ViewportClient->EngineShowFlags)
		.SetRealtimeUpdate(true));
	
	FSceneView* View = ViewportClient->CalcSceneView(&ViewFamily);
	if (!View)
	{
		return FVector::ZeroVector;
	}
	
	// Deproject the mouse position to get a world ray
	FVector WorldOrigin;
	FVector WorldDirection;
	View->DeprojectFVector2D(FVector2D(X, Y), WorldOrigin, WorldDirection);
	
	// Perform a line trace to find the world location
	FHitResult HitResult;
	FCollisionQueryParams TraceParams;
	TraceParams.bReturnPhysicalMaterial = false;
	AddIgnoredActorsForMouseTrace(TraceParams);
	
	bool bHit = World->LineTraceSingleByChannel(
		HitResult,
		WorldOrigin,
		WorldOrigin + WorldDirection * 1000000.0, // Very long trace distance
		ECC_Visibility,
		TraceParams
	);
	
	if (bHit)
	{
		return HitResult.ImpactPoint;
	}
	
	// If no hit, return a point along the ray at a reasonable distance
	return WorldOrigin + WorldDirection * 10000.0;
}

bool UWorldBLDEditController::TryGetMouseHitPoint(int32 X, int32 Y, FVector& OutHitPoint) const
{
	OutHitPoint = FVector::ZeroVector;

	// Get the active viewport client
	if (!GEditor)
	{
		return false;
	}
	
	FViewport* Viewport = nullptr;
	FEditorViewportClient* ViewportClient = nullptr;
	
	// Find the active viewport
	for (FLevelEditorViewportClient* LevelVC : GEditor->GetLevelViewportClients())
	{
		if (LevelVC && LevelVC->Viewport && LevelVC->Viewport->HasFocus())
		{
			ViewportClient = LevelVC;
			Viewport = LevelVC->Viewport;
			break;
		}
	}
	
	// If no viewport has focus, try to use the first available one
	if (!ViewportClient && GEditor->GetLevelViewportClients().Num() > 0)
	{
		ViewportClient = GEditor->GetLevelViewportClients()[0];
		if (ViewportClient)
		{
			Viewport = ViewportClient->Viewport;
		}
	}
	
	if (!ViewportClient || !Viewport)
	{
		return false;
	}
	
	// Get the world
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return false;
	}
	
	// Create a scene view to deproject the mouse coordinates
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		Viewport,
		ViewportClient->GetScene(),
		ViewportClient->EngineShowFlags)
		.SetRealtimeUpdate(true));
	
	FSceneView* View = ViewportClient->CalcSceneView(&ViewFamily);
	if (!View)
	{
		return false;
	}
	
	// Deproject the mouse position to get a world ray
	FVector WorldOrigin;
	FVector WorldDirection;
	View->DeprojectFVector2D(FVector2D(X, Y), WorldOrigin, WorldDirection);
	
	// Perform a line trace to find the world location
	FHitResult HitResult;
	FCollisionQueryParams TraceParams;
	TraceParams.bReturnPhysicalMaterial = false;
	AddIgnoredActorsForMouseTrace(TraceParams);
	
	const bool bHit = World->LineTraceSingleByChannel(
		HitResult,
		WorldOrigin,
		WorldOrigin + WorldDirection * 1000000.0, // Very long trace distance
		ECC_Visibility,
		TraceParams
	);
	
	if (!bHit)
	{
		return false;
	}

	OutHitPoint = HitResult.ImpactPoint;
	return true;
}



///////////////////////////////////////////////////////////////////////////////////////////////////
