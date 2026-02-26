// Copyright WorldBLD LLC. All Rights Reserved.

#include "ContextMenu/WorldBLDContextMenuIcons.h"

#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"
#include "UObject/SoftObjectPath.h"

namespace WorldBLDContextMenuIcons
{
	const FVector2D& GetActionIconSize()
	{
		static const FVector2D IconSize(16.0f, 16.0f);
		return IconSize;
	}

	static FSoftObjectPath GetActionIconTexturePath(EActionIcon Icon)
	{
		// Plugin content root is mounted at /WorldBLD/.
		// Texture packages live at /WorldBLD/Textures/.
		switch (Icon)
		{
		case EActionIcon::Add:
			return FSoftObjectPath(TEXT("/WorldBLD/Textures/UIplus-large.UIplus-large"));
		case EActionIcon::Remove:
			return FSoftObjectPath(TEXT("/WorldBLD/Textures/UI_minus.UI_minus"));
		case EActionIcon::Minus:
			return FSoftObjectPath(TEXT("/WorldBLD/Textures/UI_minus.UI_minus"));
		case EActionIcon::Move:
			return FSoftObjectPath(TEXT("/WorldBLD/Textures/UIMove.UIMove"));
		case EActionIcon::Confirm:
			return FSoftObjectPath(TEXT("/WorldBLD/Textures/UIcheck.UIcheck"));
		case EActionIcon::Close:
			return FSoftObjectPath(TEXT("/WorldBLD/Textures/UI_x_exit.UI_x_exit"));
		case EActionIcon::Settings:
			return FSoftObjectPath(TEXT("/WorldBLD/Textures/UISettings.UISettings"));
		case EActionIcon::Search:
			return FSoftObjectPath(TEXT("/WorldBLD/Textures/UIsearch.UIsearch"));
		case EActionIcon::More:
			return FSoftObjectPath(TEXT("/WorldBLD/Textures/T_UI_ThreeDots.T_UI_ThreeDots"));
		case EActionIcon::ArrowLeft:
			return FSoftObjectPath(TEXT("/WorldBLD/Textures/UI_arrow-left-small.UI_arrow-left-small"));
		case EActionIcon::ArrowRightLarge:
			return FSoftObjectPath(TEXT("/WorldBLD/Textures/UI_arrow-right.UI_arrow-right"));
		case EActionIcon::ArrowRight:
			return FSoftObjectPath(TEXT("/WorldBLD/Textures/UI_arrow-right-small.UI_arrow-right-small"));
		case EActionIcon::ArrowDown:
			return FSoftObjectPath(TEXT("/WorldBLD/Textures/UI_arrow-down-small.UI_arrow-down-small"));
		case EActionIcon::None:
		default:
			return FSoftObjectPath();
		}
	}

	static UTexture2D* TryLoadActionIconTexture(EActionIcon Icon)
	{
		const FSoftObjectPath Path = GetActionIconTexturePath(Icon);
		if (!Path.IsValid())
		{
			return nullptr;
		}

		return Cast<UTexture2D>(Path.TryLoad());
	}

	const FSlateBrush* GetActionIconBrush(EActionIcon Icon)
	{
		if (Icon == EActionIcon::None)
		{
			return nullptr;
		}

		// One brush per icon for stable lifetime.
		struct FIconBrushEntry
		{
			FSlateBrush Brush;
			bool bInitialized = false;
		};

		static TMap<EActionIcon, FIconBrushEntry> IconBrushes;
		FIconBrushEntry& Entry = IconBrushes.FindOrAdd(Icon);

		if (!Entry.bInitialized)
		{
			if (UTexture2D* Texture = TryLoadActionIconTexture(Icon))
			{
				Entry.Brush.SetResourceObject(Texture);
				Entry.Brush.ImageSize = GetActionIconSize();
				Entry.Brush.DrawAs = ESlateBrushDrawType::Image;
			}

			Entry.bInitialized = true;
		}

		return Entry.Brush.GetResourceObject() ? &Entry.Brush : nullptr;
	}
}


