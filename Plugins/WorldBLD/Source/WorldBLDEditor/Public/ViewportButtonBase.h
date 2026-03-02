// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EditorUtilityWidgetComponents.h"
#include "Components/ButtonSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "EditorUtilityWidget.h"
#include "Styling/SlateTypes.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/SlateWidgetStyleContainerBase.h"
#include "Framework/Styling/ButtonWidgetStyle.h"
#include "WorldBLDViewportWidgetInterface.h"
#include "ViewportButtonBase.generated.h"

UENUM(BlueprintType)
enum class EViewportButtonState : uint8
{
	EVBS_Normal,
	EVBS_Hovered,
	EVBS_Pressed,
	EVBS_Disabled
};

UCLASS(Abstract)
class WORLDBLDEDITOR_API UViewportButtonBase : public UEditorUtilityButton
{
	GENERATED_BODY()

public:
	/*
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WorldBLD")
	USlateWidgetStyleAsset* ButtonStyleAsset;*/

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WorldBLD")
	TSubclassOf<UPanelWidget> ContentPanelClass;

	UPROPERTY(Instanced, BlueprintReadWrite, Category="WorldBLD")
	UPanelWidget* ContentPanel;

	/** If true, this widget will pull its style object from the owning controller's AlternateWidgetStyle (when available). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WorldBLD", meta=(DisplayName="Use Alternate Style"))
	bool bUseAlternateStyle {false};

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category="WorldBLD")
	void OnRebuildWidget();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category="WorldBLD")
	void OnSynchronizeProperties();

	UFUNCTION(BlueprintCallable, Category="WorldBLD")
	EViewportButtonState GetButtonState() const;

protected:
	
	static bool GetBrushByState(const FSlateWidgetStyle& Style, const EViewportButtonState& ButtonState, FSlateBrush& OutBrush);
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void SynchronizeProperties() override;
};

UCLASS(EditInlineNew, Blueprintable)
class UViewportImageButtonStyle : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD") FButtonStyle ButtonStyle;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD") FSlateBrush ImageNormal;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD") FSlateBrush ImageHovered;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD") FSlateBrush ImagePressed;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD") FSlateBrush ImageDisabled;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD") FWidgetTransform ImageRenderTransform;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD") FSlateFontInfo TextFont;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD") FSlateColor TextColor{ FLinearColor::White };
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD") FMargin TextPadding{ 0.0f, 4.0f, 0.0f, 0.0f };
};

UCLASS(Blueprintable, BlueprintType)
class WORLDBLDEDITOR_API UViewportImageButton : 
	public UViewportButtonBase,
	public IWorldBLDViewportWidgetInterface
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category= Appearance)
	UTexture2D* ImageOverride;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="WorldBLD")
	FVector2D ImageSizeOverride;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="WorldBLD")
	bool bOverrideImageStyleTransform {false};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category="WorldBLD", meta=(EditCondition="bOverrideImageStyleTransform", EditConditionHides))
	FWidgetTransform ImageRenderTransform;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Appearance)
	const UViewportImageButtonStyle* ImageStyle;

	UPROPERTY(Instanced,BlueprintReadOnly, Category="WorldBLD")
	UImage* ButtonImage;

	// Text Label Properties
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Appearance)
	bool bShowTextLabel {false};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Appearance, meta=(EditCondition="bShowTextLabel", EditConditionHides))
	FText ButtonText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Appearance, meta=(EditCondition="bShowTextLabel", EditConditionHides))
	FSlateFontInfo TextFont;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Appearance, meta=(EditCondition="bShowTextLabel", EditConditionHides))
	FSlateColor TextColor {FLinearColor::White};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Appearance, meta=(EditCondition="bShowTextLabel", EditConditionHides))
	FMargin TextPadding {0.0f, 4.0f, 0.0f, 0.0f};

	UPROPERTY(Instanced, BlueprintReadOnly, Category="WorldBLD")
	UTextBlock* ButtonTextBlock;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD")
	FName StyleName {NAME_None};

	UFUNCTION(BlueprintCallable, Category="WorldBLD")
	void UpdateImageBrush();
	
	UFUNCTION(BlueprintCallable, Category="WorldBLD")
	void UpdateTextLabel();
	
	// IWorldBLDViewportWidgetInterface
	virtual void GetStyleName_Implementation(FName& Name) override { Name = StyleName; };
	virtual void SetWidgetStyle_Implementation(const UObject* StyleObj) override 
	{
		if (const UViewportImageButtonStyle* StyleAsset = Cast<UViewportImageButtonStyle>(StyleObj))
		{
			ImageStyle = StyleAsset;
			TextFont = StyleAsset->TextFont;
			TextColor = StyleAsset->TextColor;
			TextPadding = StyleAsset->TextPadding;
			SynchronizeProperties();
		}
	};
	// IWorldBLDViewportWidgetInterface

protected:

	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void SynchronizeProperties() override;
};
