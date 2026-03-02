// Copyright WorldBLD. All rights reserved.

#include "UtilityBundleItemButtonWidget.h"
#include "WorldBLDEditController.h"
#include "Engine/Texture2D.h"

void UUtilityBundleItemButtonWidget::SetUtilityBundle(UWorldBLDUtilityBundle* InBundleAsset)
{
	// Store the asset instance
	UtilityBundleAsset = InBundleAsset;
	
	// Set the item using the asset's class (for compatibility with base widget system)
	if (UtilityBundleAsset)
	{
		SetItem(UtilityBundleAsset->GetClass());
	}
}

UTexture2D* UUtilityBundleItemButtonWidget::GetThumbnailFromClass_Implementation(UClass* InClass)
{
	// Use the stored asset instance directly instead of trying to get CDO
	if (UtilityBundleAsset)
	{
		return UtilityBundleAsset->ToolImage;
	}
	return nullptr;
}

FText UUtilityBundleItemButtonWidget::GetTooltipTextFromClass_Implementation(UClass* InClass)
{
	// Use the stored asset instance directly instead of trying to get CDO
	if (UtilityBundleAsset && !UtilityBundleAsset->UtilityName.IsEmpty())
	{
		return UtilityBundleAsset->UtilityName;
	}
	return FText::GetEmpty();
}

