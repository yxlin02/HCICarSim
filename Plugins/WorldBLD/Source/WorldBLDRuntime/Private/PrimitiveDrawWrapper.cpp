// Copyright WorldBLD LLC. All rights reserved.

#include "PrimitiveDrawWrapper.h"

#include "SceneManagement.h"
#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "Engine/Canvas.h"
#include "Widgets/SWidget.h"
#include "SceneView.h"
#include "UnrealClient.h"
#include "UObject/Package.h"

#if WITH_EDITOR
#include "SEditorViewport.h"
#include "LevelEditorViewport.h"
#include "EditorViewportClient.h"
#endif

void UPrimitiveDrawWrapperUtils::PDI_SetHitProxy(
	FPrimitiveDrawWrapper Renderer,
	const FElementHitProxyContext& Context)
{
	if (Renderer.PDI != nullptr)
	{
		Renderer.PDI->SetHitProxy(new HCityKitSelectableObjectHitProxy(Context));
	}
}

void UPrimitiveDrawWrapperUtils::PDI_ClearHitProxy(
	FPrimitiveDrawWrapper Renderer)
{
	if (Renderer.PDI != nullptr)
	{
		Renderer.PDI->SetHitProxy(nullptr);
	}
}

void UPrimitiveDrawWrapperUtils::PDI_DrawPoint(
	FPrimitiveDrawWrapper Renderer, 
	const FPrimitiveDrawParams& DrawParams, 
	const FVector& Location)
{
	if (Renderer.PDI != nullptr)
	{
		Renderer.PDI->DrawPoint(Location, DrawParams.Color, DrawParams.Thickness, DrawParams.bForegroundDepth ? SDPG_Foreground : SDPG_World);
	}
}

void UPrimitiveDrawWrapperUtils::PDI_DrawLine(
	FPrimitiveDrawWrapper Renderer, 
	const FPrimitiveDrawParams& DrawParams, 
	const FVector& Start, 
	const FVector& End)
{
	if (Renderer.PDI != nullptr)
	{
		Renderer.PDI->DrawLine(Start, End, DrawParams.Color, DrawParams.bForegroundDepth ? SDPG_Foreground : SDPG_World, DrawParams.Thickness, 0.0f, DrawParams.bScreenSpace);
	}
}

void UPrimitiveDrawWrapperUtils::PDI_DrawText(
	FPrimitiveDrawWrapper Renderer, 
	const FPrimitiveDrawParams& DrawParams, 
	const FString& Message, 
	const FVector& Location)
{
	DebugRenderTextToActiveViewportCanvas(DrawParams, Message, Location);
}

void UPrimitiveDrawWrapperUtils::PDI_DrawDirectionalArrow(
	FPrimitiveDrawWrapper Renderer,
	const FPrimitiveDrawParams& DrawParams,
	const FVector& Start,
	const FVector& End,
	float ArrowSize)
{
	if (Renderer.PDI != nullptr)
	{
		const FVector Direction = End - Start;
		const float Length = Direction.Size();
		if (Length > KINDA_SMALL_NUMBER)
		{
			const FTransform ArrowXform(FRotationMatrix::MakeFromX(Direction).ToQuat(), Start);
			const FMatrix ArrowToWorld = ArrowXform.ToMatrixNoScale();
			DrawDirectionalArrow(Renderer.PDI,
				ArrowToWorld,
				DrawParams.Color,
				Length,
				ArrowSize,
				DrawParams.bForegroundDepth ? SDPG_Foreground : SDPG_World,
				DrawParams.Thickness);
		}
	}
}

void UPrimitiveDrawWrapperUtils::DebugRenderTextToActiveViewportCanvas(
	const FPrimitiveDrawParams& DrawParams, 
	const FString& Message, 
	const FVector& Location)
{
	UCanvas* DebugCanvas = FindObjectChecked<UCanvas>(GetTransientPackage(), TEXT("DebugCanvasObject"));
	if (!IsValid(DebugCanvas))
	{
		return;
	}

	FViewport* TargetViewport {nullptr};
	FSceneView* View {nullptr};
#if WITH_EDITOR
	if (FSlateApplication::IsInitialized() && GEditor)
	{
		FWidgetPath Path = FSlateApplication::Get().LocateWindowUnderMouse(FSlateApplication::Get().GetCursorPos(), FSlateApplication::Get().GetInteractiveTopLevelWindows(), true);
		if (Path.IsValid())
		{
			FVector2D MouseLocation = FSlateApplication::Get().GetCursorPos();
			for (FLevelEditorViewportClient* LevelVC : GEditor->GetLevelViewportClients())
			{
				TSharedPtr<SEditorViewport> ViewportWidget = LevelVC->GetEditorViewportWidget();
				if (ViewportWidget.IsValid() && Path.ContainsWidget(ViewportWidget.Get()))
				{
					TargetViewport = LevelVC->Viewport;
					FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
						LevelVC->Viewport,
						LevelVC->GetScene(),
						LevelVC->EngineShowFlags)
						.SetRealtimeUpdate(true));
					View = LevelVC->CalcSceneView(&ViewFamily);
					break;
				}
			}
		}
	}
#endif
	if (TargetViewport == nullptr || View == nullptr)
	{
		return;
	}
	
	FCanvas* DebugCanvasInternal = TargetViewport->GetDebugCanvas();
	DebugCanvas->Canvas = DebugCanvasInternal;
	DebugCanvas->Init(TargetViewport->GetSizeXY().X, TargetViewport->GetSizeXY().Y, View , DebugCanvasInternal);

#if 0
	FRotator CameraRot;
	FVector CameraLoc;
	PlayerOwner->GetPlayerViewPoint(CameraLoc, CameraRot);

	// don't draw text behind the camera
	if (((WorldTextLoc - CameraLoc) | CameraRot.Vector()) > 0.f)
#endif
	{
		FVector ScreenLoc = DebugCanvas->Project(Location);
		FCanvasTextItem TextItem(FVector2D(FMath::CeilToFloat(ScreenLoc.X), FMath::CeilToFloat(ScreenLoc.Y)), FText::FromString(Message), GEngine->GetSmallFont(), DrawParams.Color);
		TextItem.Scale = FVector2D(DrawParams.Thickness, DrawParams.Thickness);
		TextItem.DisableShadow();
		DebugCanvas->DrawItem(TextItem, TextItem.Position);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_HIT_PROXY(HCityKitSelectableObjectHitProxy, HHitProxy)

///////////////////////////////////////////////////////////////////////////////////////////////////
