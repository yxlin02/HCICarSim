// Copyright WorldBLD LLC. All rights reserved.


#include "ViewportButtonBase.h"

EViewportButtonState UViewportButtonBase::GetButtonState() const
{
	// `MyButton` is the underlying Slate widget. It can be invalid if this UWidget has not been rebuilt yet
	// or is in the process of being torn down. Callers may still want a safe "best effort" state.
	if (!MyButton.IsValid())
	{
		return GetIsEnabled() ? EViewportButtonState::EVBS_Normal : EViewportButtonState::EVBS_Disabled;
	}

	if (!MyButton->IsEnabled())
	{
		return EViewportButtonState::EVBS_Disabled;
	}

	if (MyButton->IsPressed())
	{
		return EViewportButtonState::EVBS_Pressed;
	}

	if (MyButton->IsHovered())
	{
		return EViewportButtonState::EVBS_Hovered;
	}

	return EViewportButtonState::EVBS_Normal;
}

bool UViewportButtonBase::GetBrushByState(const FSlateWidgetStyle& Style, const EViewportButtonState& ButtonState, FSlateBrush& OutBrush)
{
	TArray<const FSlateBrush*> Brushes;
	Style.GetResources(Brushes);
	int BrushIdx = (int)ButtonState + 4;
	bool bValidIndex = Brushes.IsValidIndex(BrushIdx);
	if (bValidIndex)
	{
		OutBrush = *Brushes[BrushIdx];
	}
	return bValidIndex;
}

TSharedRef<SWidget> UViewportButtonBase::RebuildWidget() 
{
	TSharedRef<SButton> Button = StaticCastSharedRef<SButton, SWidget>(Super::RebuildWidget());
	if (!IsValid(ContentPanelClass))
	{
		ContentPanelClass = UOverlay::StaticClass();
	}
	ContentPanel = NewObject<UPanelWidget>(this, ContentPanelClass);
	Button->SetContent(ContentPanel->TakeWidget());
	OnRebuildWidget();
	return Button;
}

void UViewportButtonBase::SynchronizeProperties() 
{
	Super::SynchronizeProperties();
	OnSynchronizeProperties();
}

void UViewportImageButton::UpdateImageBrush()
{
	if (ImageStyle)
	{
		Super::SetStyle(ImageStyle->ButtonStyle);
	}

	auto GetBrushByState = [&](const EViewportButtonState& ButtonState, FSlateBrush& OutBrush) -> int32
	{
		const FSlateBrush* ImageBrushes[] =
		{
			&ImageStyle->ImageNormal,
			&ImageStyle->ImageHovered,
			&ImageStyle->ImagePressed,
			&ImageStyle->ImageDisabled
		};
		int BrushIdx = FMath::Clamp((int)ButtonState, 0, 3);
		OutBrush = *ImageBrushes[BrushIdx];
		return BrushIdx;
	};

	if (ButtonImage && ImageStyle)
	{
		EViewportButtonState State = GetButtonState();
		FSlateBrush ImageBrush;
		GetBrushByState(State, ImageBrush);
		ButtonImage->SetBrush(ImageBrush);

		if (MyButton.IsValid())
		{
			MyButton->SetContentPadding(FMargin(0, 0));
		}
		ButtonImage->SetRenderTransform(ImageStyle->ImageRenderTransform);

		if (ImageOverride)
		{
			FSlateBrush Brush = ButtonImage->GetBrush();
			Brush.SetResourceObject(ImageOverride);
			ButtonImage->SetBrush(Brush);			
		}
		if (bOverrideImageStyleTransform)
		{
			ButtonImage->SetRenderTransform(ImageRenderTransform);
		}
		if (ImageSizeOverride != FVector2D::ZeroVector)
		{
			ButtonImage->SetDesiredSizeOverride(ImageSizeOverride);
		}

		// Force a repaint/layout pass so the new brush takes effect immediately even if only enabled state changed.
		ButtonImage->InvalidateLayoutAndVolatility();
	}
}

TSharedRef<SWidget> UViewportImageButton::RebuildWidget()
{
	TSharedRef<SButton> Button = StaticCastSharedRef<SButton, SWidget>(Super::RebuildWidget());

	ContentPanelClass = UOverlay::StaticClass();

	// Add VerticalBox as child to Overlay panel
	UPanelWidget* Container = ContentPanel;
	if (bShowTextLabel)
	{
		UVerticalBox* VBox = NewObject<UVerticalBox>(this, TEXT("VerticalBox"));
		UPanelSlot* OverlaySlot = ContentPanel->AddChild(VBox);
		if (UVerticalBoxSlot* VBoxSlot = Cast<UVerticalBoxSlot>(OverlaySlot))
		{
			VBoxSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Fill);
			VBoxSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Fill);
			VBoxSlot->SetPadding(FMargin(0, 0, 0, 0));
			VBoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		Container = VBox;
	}
	
	ButtonImage = NewObject<UImage>(this, TEXT("MyImage"));	
	UPanelSlot* ImageSlot = Container->AddChild(ButtonImage);
	
	if (bShowTextLabel)
	{
		// Using VerticalBox layout - image on top, text below
		if (UVerticalBoxSlot* VBoxSlot = Cast<UVerticalBoxSlot>(ImageSlot))
		{
			VBoxSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);
			VBoxSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Top);
			VBoxSlot->SetPadding(FMargin(0, 0, 0, 0));
			VBoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
		
		// Create text label below the image
		ButtonTextBlock = NewObject<UTextBlock>(this, TEXT("MyTextLabel"));
		UPanelSlot* TextSlot = Container->AddChild(ButtonTextBlock);
		if (UVerticalBoxSlot* TextVBoxSlot = Cast<UVerticalBoxSlot>(TextSlot))
		{
			TextVBoxSlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);
			TextVBoxSlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Top);
			TextVBoxSlot->SetPadding(TextPadding);
			TextVBoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
	}
	else
	{
		// Using Overlay layout - image fills entire button
		if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(ImageSlot))
		{
			OverlaySlot->SetHorizontalAlignment(EHorizontalAlignment::HAlign_Center);
			OverlaySlot->SetVerticalAlignment(EVerticalAlignment::VAlign_Center);			
			OverlaySlot->SetPadding(FMargin(0, 0));
		}
	}

	if (!OnPressed.IsAlreadyBound(this, &UViewportImageButton::UpdateImageBrush)) OnPressed.AddDynamic(this, &UViewportImageButton::UpdateImageBrush);
	if (!OnReleased.IsAlreadyBound(this, &UViewportImageButton::UpdateImageBrush)) OnReleased.AddDynamic(this, &UViewportImageButton::UpdateImageBrush);
	if (!OnHovered.IsAlreadyBound(this, &UViewportImageButton::UpdateImageBrush)) OnHovered.AddDynamic(this, &UViewportImageButton::UpdateImageBrush);
	if (!OnUnhovered.IsAlreadyBound(this, &UViewportImageButton::UpdateImageBrush)) OnUnhovered.AddDynamic(this, &UViewportImageButton::UpdateImageBrush);

	return Button;
}

void UViewportImageButton::UpdateTextLabel()
{
	if (ButtonTextBlock && bShowTextLabel)
	{
		ButtonTextBlock->SetText(ButtonText);
		ButtonTextBlock->SetFont(TextFont);
		ButtonTextBlock->SetColorAndOpacity(TextColor);
		ButtonTextBlock->SetVisibility(ESlateVisibility::HitTestInvisible);
		
		// Update padding if it changed (works with VerticalBoxSlot)
		if (UVerticalBoxSlot* TextVBoxSlot = Cast<UVerticalBoxSlot>(ButtonTextBlock->Slot))
		{
			TextVBoxSlot->SetPadding(TextPadding);
		}
	}
	else if (ButtonTextBlock)
	{
		ButtonTextBlock->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UViewportImageButton::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	UpdateImageBrush();
	UpdateTextLabel();
}


