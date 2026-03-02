// Copyright WorldBLD LLC. All rights reserved.

#include "WorldBLDElementSnapPointComponent.h"
#include "WorldBLDRuntimeSettings.h"

#include "Engine/Texture2D.h"

UWorldBLDElementSnapPointComponent::UWorldBLDElementSnapPointComponent()
{
	const TSoftObjectPtr<UTexture2D> MaybeTexture = UWorldBLDRuntimeSettings::Get()->SnapPointTexture;
	if (!MaybeTexture.IsNull())
	{
		if (UTexture2D* Texture = MaybeTexture.LoadSynchronous())
		{
			Sprite = Texture;
		}
	}
}

FWorldBLDKitElementPoint UWorldBLDElementSnapPointComponent::GetPoint() const
{
	FWorldBLDKitElementPoint ThisPoint;

	ThisPoint.Owner = GetOwner();
	ThisPoint.Id = PointId;
	if (PointId.IsNone())
	{
		/// @TODO: Use full component path?
		ThisPoint.Id = *GetName();
	}
	ThisPoint.Tags = PointTags;
	ThisPoint.LocalTransform = GetSocketTransform(NAME_None, ERelativeTransformSpace::RTS_Actor);
	
	return ThisPoint;
}
