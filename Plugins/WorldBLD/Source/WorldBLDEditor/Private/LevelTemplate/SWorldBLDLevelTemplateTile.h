#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "UObject/StrongObjectPtr.h"

class UTexture2D;
class UWorldBLDLevelTemplateBundle;
struct FSlateDynamicImageBrush;

DECLARE_DELEGATE_RetVal_OneParam(FReply, FOnWorldBLDLevelTemplateTileClicked, UWorldBLDLevelTemplateBundle* /*Bundle*/);

class SWorldBLDLevelTemplateTile : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorldBLDLevelTemplateTile) {}
		SLATE_ARGUMENT(UWorldBLDLevelTemplateBundle*, Bundle)
		SLATE_EVENT(FOnWorldBLDLevelTemplateTileClicked, OnClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	FReply HandleClicked();

	const FSlateBrush* GetPreviewBrush() const;
	FText GetTemplateNameText() const;
	FText GetPluginLabelText() const;
	FText GetTooltipText() const;
	EVisibility GetHoverOverlayVisibility() const;

	void StartLoadPreviewImage();

	static FString GetPluginLabel(const UWorldBLDLevelTemplateBundle* Bundle);

private:
	TWeakObjectPtr<UWorldBLDLevelTemplateBundle> Bundle;
	FOnWorldBLDLevelTemplateTileClicked OnClicked;

	TSharedPtr<FSlateDynamicImageBrush> PreviewBrush;
	TStrongObjectPtr<UTexture2D> PreviewTexture;

	bool bIsHovered = false;
};
