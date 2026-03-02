// Copyright WorldBLD LLC. All rights reserved. 

#include "WorldBLDRuntime/Classes/WorldBLDKitBase.h"

///////////////////////////////////////////////////////////////////////////////////////////////////

UE_DEFINE_GAMEPLAY_TAG(Tag_WorldBLD_Style, TEXT("WorldBLD.Style"));
UE_DEFINE_GAMEPLAY_TAG(Tag_WorldBLD_Style_Street, TEXT("WorldBLD.Style.Street"));
UE_DEFINE_GAMEPLAY_TAG(Tag_WorldBLD_Style_KitRoad, TEXT("WorldBLD.Style.KitRoad"));
UE_DEFINE_GAMEPLAY_TAG(Tag_WorldBLD_Style_KitRoadModule, TEXT("WorldBLD.Style.KitRoadModule"));
UE_DEFINE_GAMEPLAY_TAG(Tag_WorldBLD_Style_RoadPreset, TEXT("WorldBLD.Style.RoadPreset"));

///////////////////////////////////////////////////////////////////////////////////////////////////

UWorldBLDKitStyleBase::UWorldBLDKitStyleBase()
{
	// Empty
}

void UWorldBLDKitStyleBase::Serialize(FArchive& Ar)
{
	if (Ar.IsSaving() && UniqueId == FGuid())
	{
		MakeUnique(true);
	}

	Super::Serialize(Ar);

	if (Ar.IsLoading() && UniqueId == FGuid())
	{
		MakeUnique(true);
	}
}

FWorldBLDKitStyleReference UWorldBLDKitStyleBase::CreateReference() const
{
	FWorldBLDKitStyleReference Reference;
	Reference.Type = StyleType;
	Reference.Class = GetClass();
	Reference.Name = StyleName;
	Reference.Guid = UniqueId;
	Reference.bIsSubStyle = IsValid(GetOuter()) && GetOuter()->IsA(UWorldBLDKitStyleBase::StaticClass());
	return Reference;
}

UWorldBLDKitStyleBase* UWorldBLDKitStyleBase::Duplicate(UObject* NewOwner, bool bSharedUniqueId) const
{
	UWorldBLDKitStyleBase* Copy = DuplicateObject(this, NewOwner);
	if (Copy && bSharedUniqueId)
	{
		Copy->bIdIsShared = true;
		Copy->UniqueId = UniqueId;
	}
	return Copy;
}

void UWorldBLDKitStyleBase::MakeUnique(bool bForce)
{
	if (bForce || bIdIsShared)
	{
		UniqueId = FGuid::NewGuid();
		bIdIsShared = false;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/*
const TArray<UCityKitStyleBase*>& ACityKitBase::GetAllStyles(bool bIncludeSubStyles)
{
	if ((CachedAllStyles.Num() < Styles.Num()) || bDirtyStyleCache)
	{
		CachedAllStyles.Empty();
		CachedAllStylesDeep.Empty();

		for (UCityKitStyleBase* Style : Styles)
		{
			if (IsValid(Style))
			{
				CachedAllStyles.Add(Style);
				CachedAllStylesDeep.Add(Style);
				CachedAllStylesDeep.Append(Style->SubStyles());
			}
		}

		for (UCityKitExtension* Extension : Extensions)
		{
			if (IsValid(Extension))
			{
				for (UCityKitPrimaryStyleBase* ExtStyle : Extension->Styles)
				{
					if (IsValid(ExtStyle))
					{
						CachedAllStyles.Add(Cast<UCityKitStyleBase>(ExtStyle));
						CachedAllStylesDeep.Add(Cast<UCityKitStyleBase>(ExtStyle));
						CachedAllStylesDeep.Append(ExtStyle->SubStyles());
					}
				}
			}
		}

		bDirtyStyleCache = false;
	}

	return bIncludeSubStyles ? CachedAllStylesDeep : CachedAllStyles;
}

TArray<UCityKitStyleBase*> ACityKitBase::FindStylesByType(FGameplayTag Type, bool bIncludeSubStyles)
{
	return GetAllStyles(bIncludeSubStyles).FilterByPredicate([&](UCityKitStyleBase* Style)
	{
		return IsValid(Style) && Style->StyleType.MatchesTag(Type);
	});
}

TArray<UCityKitStyleBase*> ACityKitBase::FindStylesByClass(TSubclassOf<UCityKitStyleBase> Class, bool bIncludeSubStyles)
{
	return GetAllStyles(bIncludeSubStyles).FilterByPredicate([&](UCityKitStyleBase* Style)
	{
		return IsValid(Style) && Style->IsA(Class);
	});
}

UCityKitStyleBase* ACityKitBase::FindStyleByGuid(const FGuid& Guid)
{
	UCityKitStyleBase*const* ResultRef = GetAllStyles(true).FindByPredicate([&](UCityKitStyleBase* Style)
	{
		return IsValid(Style) && (Style->UniqueId == Guid);
	});
	return ResultRef ? *ResultRef : nullptr;
}

UCityKitStyleBase* ACityKitBase::FindStyleByTypeAndName(FGameplayTag Type, const FString& Name, bool bIncludeSubStyles)
{
	UCityKitStyleBase*const* ResultRef = GetAllStyles(bIncludeSubStyles).FindByPredicate([&](UCityKitStyleBase* Style)
	{
		return IsValid(Style) && Style->StyleType.MatchesTag(Type) && (Style->StyleName == Name);
	});
	return ResultRef ? *ResultRef : nullptr;
}

UCityKitStyleBase* ACityKitBase::FindStyleByClassAndName(TSubclassOf<UCityKitStyleBase> Class, const FString& Name, bool bIncludeSubStyles)
{
	UCityKitStyleBase*const* ResultRef = GetAllStyles(bIncludeSubStyles).FindByPredicate([&](UCityKitStyleBase* Style)
	{
		return IsValid(Style) && Style->IsA(Class) && (Style->StyleName == Name);
	});
	return ResultRef ? *ResultRef : nullptr;
}
*/

/*
UCityKitStyleBase* ACityKitBase::FindStyleByReference(const FCityKitStyleReference& Reference)
{
	UCityKitStyleBase* Ref = nullptr;
	if (Reference.Guid.IsValid())
	{
		Ref = FindStyleByGuid(Reference.Guid);
	}
	if (!IsValid(Ref) && IsValid(Reference.Class) && !Reference.Name.IsEmpty())
	{
		Ref = FindStyleByClassAndName(Reference.Class, Reference.Name, Reference.bIsSubStyle);
	}
	if (!IsValid(Ref) && Reference.Type.IsValid() && !Reference.Name.IsEmpty())
	{
		Ref = FindStyleByTypeAndName(Reference.Type, Reference.Name, Reference.bIsSubStyle);
	}
	return Ref;
}
*/

/*
void ACityKitBase::InvalidateStyleCache()
{
	bDirtyStyleCache = true;
	CachedAllStyles.Empty();
	CachedAllStylesDeep.Empty();
}
*/

///////////////////////////////////////////////////////////////////////////////////////////////////

/* This is a utility currently contained in the CityBLD plugin, we want to move it to WorldBLD
void AWorldBLDKitBase::OnConstruction(const FTransform& Transform)
{
	UCityKitElementUtils::AssignActorsToCityFolderPath({this});
}
*/

#if WITH_EDITOR
void AWorldBLDKitBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	//InvalidateStyleCache();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
