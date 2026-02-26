// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/Blueprint.h"
#include "IContentBrowserSingleton.h"
#include "UObject/SoftObjectPath.h"

#include "InputAssetComboPanel.h" // FAssetComboFilterSetup

namespace WorldBLD::Editor::InputAssetFilterUtils
{
	inline bool PassesFilterSetup(const FAssetData& Asset, const FAssetComboFilterSetup& FilterSetup)
	{
		if (!Asset.IsValid())
		{
			return false;
		}

		// Whitelist-only mode.
		if (FilterSetup.bPresentSpecificAssets)
		{
			for (const FAssetData& Allowed : FilterSetup.SpecificAssets)
			{
				if (Allowed.ToSoftObjectPath() == Asset.ToSoftObjectPath())
				{
					return true;
				}
			}
			return false;
		}

		// Loaded-kits-only mode.
		if (FilterSetup.bRestrictToAllowedPackageNames)
		{
			if (!FilterSetup.AllowedPackageNames.IsValid() || !FilterSetup.AllowedPackageNames->Contains(Asset.PackageName))
			{
				return false;
			}
		}

		// Class filter mode.
		if (!IsValid(FilterSetup.AssetClassType) || FilterSetup.AssetClassType == UObject::StaticClass())
		{
			return true;
		}

		const FTopLevelAssetPath DesiredClassPath = FilterSetup.AssetClassType->GetClassPathName();
		if (Asset.AssetClassPath == DesiredClassPath)
		{
			return true;
		}

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

		// Collect derived class paths (native + any blueprint-generated UClasses that the registry knows about).
		TSet<FTopLevelAssetPath> DerivedClassPaths;
		{
			TArray<FTopLevelAssetPath> BaseClassPaths;
			BaseClassPaths.Add(DesiredClassPath);

			TSet<FTopLevelAssetPath> Excluded;
			AssetRegistry.GetDerivedClassNames(BaseClassPaths, Excluded, DerivedClassPaths);
		}

		if (DerivedClassPaths.Contains(Asset.AssetClassPath))
		{
			return true;
		}

		// Blueprint-like assets: accept if their GeneratedClass is derived from the desired base class.
		FString GeneratedClassPathStr = Asset.GetTagValueRef<FString>(FName(TEXT("GeneratedClass")));
		if (GeneratedClassPathStr.IsEmpty())
		{
			GeneratedClassPathStr = Asset.GetTagValueRef<FString>(FName(TEXT("GeneratedClassPath")));
		}
		if (GeneratedClassPathStr.IsEmpty())
		{
			return false;
		}

		const FString GeneratedClassNameStr = FSoftClassPath(GeneratedClassPathStr).GetAssetName();
		if (GeneratedClassNameStr.IsEmpty())
		{
			return false;
		}

		// Compare by the UClass object name (ex: "BP_MyActor_C"), which matches FTopLevelAssetPath::GetAssetName().
		TSet<FName> DerivedClassNames;
		DerivedClassNames.Reserve(DerivedClassPaths.Num());
		for (const FTopLevelAssetPath& ClassPath : DerivedClassPaths)
		{
			DerivedClassNames.Add(ClassPath.GetAssetName());
		}

		return DerivedClassNames.Contains(FName(*GeneratedClassNameStr));
	}

	inline void ApplyFilterSetupToAssetPickerConfig(FAssetPickerConfig& InOutConfig, const FAssetComboFilterSetup& FilterSetup)
	{
		InOutConfig.Filter.bRecursiveClasses = true;
		InOutConfig.Filter.bRecursivePaths = true;

		const auto ComposeShouldFilterAsset = [](FAssetPickerConfig& Config, const FOnShouldFilterAsset& AdditionalFilter)
		{
			const FOnShouldFilterAsset ExistingFilter = Config.OnShouldFilterAsset;
			Config.OnShouldFilterAsset = FOnShouldFilterAsset::CreateLambda(
				[ExistingFilter, AdditionalFilter](const FAssetData& Asset)
				{
					const bool bFilteredByExisting = ExistingFilter.IsBound() ? ExistingFilter.Execute(Asset) : false;
					const bool bFilteredByAdditional = AdditionalFilter.IsBound() ? AdditionalFilter.Execute(Asset) : false;
					return bFilteredByExisting || bFilteredByAdditional;
				});
		};

		const bool bUsingClassFilter = !FilterSetup.bPresentSpecificAssets;
		if (bUsingClassFilter && IsValid(FilterSetup.AssetClassType))
		{
			InOutConfig.Filter.ClassPaths.Add(FilterSetup.AssetClassType->GetClassPathName());

			const bool bWantsAnyObjectType = (FilterSetup.AssetClassType == UObject::StaticClass());
			if (!bWantsAnyObjectType)
			{
				InOutConfig.Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());

				FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
				IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

				TSet<FName> DerivedClassNames;
				{
					TArray<FTopLevelAssetPath> BaseClassPaths;
					BaseClassPaths.Add(FilterSetup.AssetClassType->GetClassPathName());

					TSet<FTopLevelAssetPath> Excluded;
					TSet<FTopLevelAssetPath> DerivedClassPaths;
					AssetRegistry.GetDerivedClassNames(BaseClassPaths, Excluded, DerivedClassPaths);

					DerivedClassNames.Reserve(DerivedClassPaths.Num());
					for (const FTopLevelAssetPath& ClassPath : DerivedClassPaths)
					{
						DerivedClassNames.Add(ClassPath.GetAssetName());
					}
				}

				const TSet<FName> DerivedClassNamesCopy = MoveTemp(DerivedClassNames);
				const FOnShouldFilterAsset BlueprintGeneratedClassFilter = FOnShouldFilterAsset::CreateLambda(
					[DerivedClassNamesCopy](const FAssetData& Asset)
					{
						// IMPORTANT:
						// Blueprint assets can have many asset classes (Blueprint, WidgetBlueprint, AnimBlueprint, etc).
						// Instead of checking the *asset class* (which causes derived blueprint types to bypass filtering),
						// treat any asset with a GeneratedClass tag as a blueprint-like asset and validate its generated class.
						FString GeneratedClassPathStr = Asset.GetTagValueRef<FString>(FName(TEXT("GeneratedClass")));
						if (GeneratedClassPathStr.IsEmpty())
						{
							GeneratedClassPathStr = Asset.GetTagValueRef<FString>(FName(TEXT("GeneratedClassPath")));
						}
						if (GeneratedClassPathStr.IsEmpty())
						{
							return false;
						}

						const FString GeneratedClassNameStr = FSoftClassPath(GeneratedClassPathStr).GetAssetName();
						const FName GeneratedClassName(*GeneratedClassNameStr);
						return !DerivedClassNamesCopy.Contains(GeneratedClassName);
					});

				ComposeShouldFilterAsset(InOutConfig, BlueprintGeneratedClassFilter);
			}
		}

		if (FilterSetup.bPresentSpecificAssets)
		{
			for (const FAssetData& AssetData : FilterSetup.SpecificAssets)
			{
				InOutConfig.Filter.SoftObjectPaths.Add(AssetData.ToSoftObjectPath());
			}

			// Whitelist-only mode.
			TSet<FSoftObjectPath> AllowedPaths;
			AllowedPaths.Append(InOutConfig.Filter.SoftObjectPaths);
			ComposeShouldFilterAsset(InOutConfig, FOnShouldFilterAsset::CreateLambda([AllowedPaths](const FAssetData& Asset)
			{
				return !AllowedPaths.Contains(Asset.ToSoftObjectPath());
			}));
		}
		else if (FilterSetup.bRestrictToAllowedPackageNames)
		{
			// Loaded-kits-only mode. If no allowed packages were provided, show nothing.
			const TSharedPtr<const TSet<FName>> AllowedPackages = FilterSetup.AllowedPackageNames;
			ComposeShouldFilterAsset(InOutConfig, FOnShouldFilterAsset::CreateLambda([AllowedPackages](const FAssetData& Asset)
			{
				return !AllowedPackages.IsValid() || !AllowedPackages->Contains(Asset.PackageName);
			}));
		}
	}
}
