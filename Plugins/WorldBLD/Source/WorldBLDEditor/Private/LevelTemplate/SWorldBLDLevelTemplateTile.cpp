#include "LevelTemplate/SWorldBLDLevelTemplateTile.h"

#include "AssetLibrary/WorldBLDAssetLibraryImageLoader.h"
#include "WorldBLDLevelTemplateBundle.h"

#include "Engine/AssetManager.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "Misc/SecureHash.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	static FName MakeResourceNameFromObjectPath(const FString& ObjectPath)
	{
		const FString HexHash = FMD5::HashAnsiString(*ObjectPath);
		return FName(*FString::Printf(TEXT("WorldBLDLevelTemplateThumb_%s"), *HexHash));
	}
}

void SWorldBLDLevelTemplateTile::Construct(const FArguments& InArgs)
{
	Bundle = InArgs._Bundle;
	OnClicked = InArgs._OnClicked;

	PreviewBrush = FWorldBLDAssetLibraryImageLoader::Get().GetPlaceholderBrush();
	StartLoadPreviewImage();

	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(200.0f)
		.HeightOverride(250.0f)
		[
			SNew(SBorder)
			.Padding(0.0f)
			.BorderBackgroundColor(FLinearColor(0.12f, 0.12f, 0.12f, 1.0f))
			.Cursor(EMouseCursor::Hand)
			.ToolTipText(this, &SWorldBLDLevelTemplateTile::GetTooltipText)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SImage)
					.Image(this, &SWorldBLDLevelTemplateTile::GetPreviewBrush)
				]
				+ SOverlay::Slot()
				.VAlign(VAlign_Bottom)
				[
					SNew(SBorder)
					.Padding(FMargin(8.0f))
					.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.75f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(this, &SWorldBLDLevelTemplateTile::GetTemplateNameText)
							.ColorAndOpacity(FLinearColor(0.92f, 0.92f, 0.92f, 1.0f))
							.WrapTextAt(180.0f)
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(FMargin(0.0f, 2.0f, 0.0f, 0.0f))
						[
							SNew(STextBlock)
							.Text(this, &SWorldBLDLevelTemplateTile::GetPluginLabelText)
							.ColorAndOpacity(FLinearColor(0.75f, 0.75f, 0.75f, 1.0f))
						]
					]
				]
				+ SOverlay::Slot()
				[
					SNew(SBorder)
					.Visibility(this, &SWorldBLDLevelTemplateTile::GetHoverOverlayVisibility)
					.Padding(FMargin(8.0f))
					.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.35f))
				]
			]
		]
	];
}

void SWorldBLDLevelTemplateTile::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	bIsHovered = true;
	SCompoundWidget::OnMouseEnter(MyGeometry, MouseEvent);
	Invalidate(EInvalidateWidgetReason::Paint);
}

void SWorldBLDLevelTemplateTile::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	bIsHovered = false;
	SCompoundWidget::OnMouseLeave(MouseEvent);
	Invalidate(EInvalidateWidgetReason::Paint);
}

FReply SWorldBLDLevelTemplateTile::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		return HandleClicked();
	}

	return SCompoundWidget::OnMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SWorldBLDLevelTemplateTile::HandleClicked()
{
	UWorldBLDLevelTemplateBundle* BundlePtr = Bundle.Get();
	if (OnClicked.IsBound())
	{
		return OnClicked.Execute(BundlePtr);
	}

	return FReply::Handled();
}

const FSlateBrush* SWorldBLDLevelTemplateTile::GetPreviewBrush() const
{
	if (PreviewBrush.IsValid())
	{
		return PreviewBrush.Get();
	}

	TSharedPtr<FSlateDynamicImageBrush> Placeholder = FWorldBLDAssetLibraryImageLoader::Get().GetPlaceholderBrush();
	return Placeholder.IsValid() ? Placeholder.Get() : nullptr;
}

FText SWorldBLDLevelTemplateTile::GetTemplateNameText() const
{
	if (const UWorldBLDLevelTemplateBundle* BundlePtr = Bundle.Get())
	{
		if (!BundlePtr->TemplateName.IsEmpty())
		{
			return FText::FromString(BundlePtr->TemplateName);
		}
	}
	return FText::FromString(TEXT("Unnamed"));
}

FText SWorldBLDLevelTemplateTile::GetPluginLabelText() const
{
	return FText::FromString(GetPluginLabel(Bundle.Get()));
}

FText SWorldBLDLevelTemplateTile::GetTooltipText() const
{
	if (const UWorldBLDLevelTemplateBundle* BundlePtr = Bundle.Get())
	{
		return BundlePtr->TemplateDescription;
	}
	return FText::GetEmpty();
}

EVisibility SWorldBLDLevelTemplateTile::GetHoverOverlayVisibility() const
{
	return bIsHovered ? EVisibility::Visible : EVisibility::Collapsed;
}

void SWorldBLDLevelTemplateTile::StartLoadPreviewImage()
{
	const UWorldBLDLevelTemplateBundle* BundlePtr = Bundle.Get();
	if (!IsValid(BundlePtr))
	{
		return;
	}

	const TSoftObjectPtr<UTexture2D> SoftTexture = BundlePtr->TemplatePreviewImage;
	if (SoftTexture.IsNull())
	{
		return;
	}

	const FSoftObjectPath TexturePath = SoftTexture.ToSoftObjectPath();
	if (!TexturePath.IsValid())
	{
		return;
	}

	FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
	Streamable.RequestAsyncLoad(TexturePath, [WeakThis = TWeakPtr<SWorldBLDLevelTemplateTile>(SharedThis(this)), TexturePath]()
	{
		if (TSharedPtr<SWorldBLDLevelTemplateTile> Pinned = WeakThis.Pin())
		{
			UTexture2D* LoadedTexture = Cast<UTexture2D>(TexturePath.ResolveObject());
			if (!LoadedTexture)
			{
				return;
			}

			Pinned->PreviewTexture = TStrongObjectPtr<UTexture2D>(LoadedTexture);

			const int32 SizeX = FMath::Max(1, LoadedTexture->GetSizeX());
			const int32 SizeY = FMath::Max(1, LoadedTexture->GetSizeY());
			const FName ResourceName = MakeResourceNameFromObjectPath(TexturePath.ToString());

			TSharedPtr<FSlateDynamicImageBrush> Brush = MakeShared<FSlateDynamicImageBrush>(ResourceName, FVector2D((float)SizeX, (float)SizeY));
			Brush->SetResourceObject(LoadedTexture);
			Pinned->PreviewBrush = Brush;

			Pinned->Invalidate(EInvalidateWidgetReason::Paint);
		}
	});
}

FString SWorldBLDLevelTemplateTile::GetPluginLabel(const UWorldBLDLevelTemplateBundle* BundlePtr)
{
	if (!IsValid(BundlePtr))
	{
		return TEXT("Content");
	}

	const FString PackageName = BundlePtr->GetOutermost() ? BundlePtr->GetOutermost()->GetName() : BundlePtr->GetPathName();

	const int32 PluginsIndex = PackageName.Find(TEXT("/Game/Plugins/"), ESearchCase::IgnoreCase);
	if (PluginsIndex != INDEX_NONE)
	{
		FString Remainder = PackageName.Mid(PluginsIndex + FString(TEXT("/Game/Plugins/")).Len());
		FString PluginName;
		if (Remainder.Split(TEXT("/"), &PluginName, nullptr) && !PluginName.IsEmpty())
		{
			return PluginName;
		}
	}

	if (PackageName.StartsWith(TEXT("/")) && !PackageName.StartsWith(TEXT("/Game/")))
	{
		FString WithoutLeading = PackageName.Mid(1);
		FString Root;
		if (WithoutLeading.Split(TEXT("/"), &Root, nullptr) && !Root.IsEmpty())
		{
			return Root;
		}
	}

	return TEXT("Content");
}
