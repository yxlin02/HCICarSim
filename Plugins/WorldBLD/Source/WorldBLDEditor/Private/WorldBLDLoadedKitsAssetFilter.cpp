// Copyright WorldBLD LLC. All rights reserved.

#include "WorldBLDLoadedKitsAssetFilter.h"

#include "WorldBLDEditorToolkit.h"
#include "WorldBLDKitBase.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Modules/ModuleManager.h"

namespace WorldBLD::Editor::LoadedKitsAssetFilter
{
	namespace Private
	{
		static TWeakObjectPtr<UWorldBLDEdMode> CachedEdMode;
		static int32 CachedKitCount = INDEX_NONE;
		static int32 CachedBundleCount = INDEX_NONE;
		static TSharedPtr<const TSet<FName>> CachedAllowedPackages;

		static void AddPackageNameFromObject(const UObject* Object, TSet<FName>& InOutPackageNames)
		{
			if (!IsValid(Object))
			{
				return;
			}

			if (const UPackage* Package = Object->GetOutermost())
			{
				InOutPackageNames.Add(Package->GetFName());
			}
		}

		static void AddPackageNameFromSoftObjectPath(const FSoftObjectPath& SoftPath, TSet<FName>& InOutPackageNames)
		{
			const FString LongPackageName = SoftPath.GetLongPackageName();
			if (!LongPackageName.IsEmpty())
			{
				InOutPackageNames.Add(*LongPackageName);
			}
		}

		static void AddBundleReferencedPackages(const UWorldBLDKitBundle* Bundle, TSet<FName>& InOutPackageNames)
		{
			if (!IsValid(Bundle))
			{
				return;
			}

			AddPackageNameFromObject(Bundle, InOutPackageNames);

			for (const FWorldBLDKitBundleItem& Item : Bundle->WorldBLDKits)
			{
				AddPackageNameFromSoftObjectPath(Item.WorldBLDKitClass.ToSoftObjectPath(), InOutPackageNames);
			}

			for (const FWorldBLDKitExtensionBundleItem& Item : Bundle->WorldBLDKitExtensions)
			{
				AddPackageNameFromSoftObjectPath(Item.Extension.ToSoftObjectPath(), InOutPackageNames);
			}

			for (const FWorldBLDKitExampleBundleItem& Item : Bundle->ExampleMaps)
			{
				AddPackageNameFromSoftObjectPath(Item.LevelPreview.ToSoftObjectPath(), InOutPackageNames);
				AddPackageNameFromSoftObjectPath(Item.LevelReference.ToSoftObjectPath(), InOutPackageNames);
			}
		}

		static TSharedPtr<const TSet<FName>> BuildAllowedPackageNames(const UWorldBLDEdMode& EdMode)
		{
			TSet<FName> RootPackages;

			// Root packages from in-world kits and their extensions.
			for (const AWorldBLDKitBase* Kit : EdMode.GetLoadedWorldBLDKits())
			{
				if (!IsValid(Kit))
				{
					continue;
				}

				// Include the kit blueprint/class package.
				AddPackageNameFromObject(Kit->GetClass(), RootPackages);

				// Include extension data asset packages (and later their dependencies).
				for (const UWorldBLDKitExtension* Extension : Kit->Extensions)
				{
					AddPackageNameFromObject(Extension, RootPackages);
				}
			}

			// Root packages from loaded bundles (and their explicitly referenced soft assets).
			for (const UWorldBLDKitBundle* Bundle : EdMode.GetLoadedWorldBLDKitBundles())
			{
				AddBundleReferencedPackages(Bundle, RootPackages);
			}

			if (RootPackages.IsEmpty())
			{
				return nullptr;
			}

			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

			// BFS over dependency graph, collecting package names.
			TSet<FName> AllowedPackages;
			TArray<FName> Stack = RootPackages.Array();

			// Note: FAssetRegistryDependencyOptions::Get*Query helpers are not exported from the engine DLL in UE5.6,
			// so calling them from a plugin module causes unresolved externals at link time. For this use-case we
			// want to include all package + manage dependencies, so the default query flags are sufficient.
			const UE::AssetRegistry::EDependencyCategory DependencyCategory =
				UE::AssetRegistry::EDependencyCategory::Package | UE::AssetRegistry::EDependencyCategory::Manage;
			const UE::AssetRegistry::FDependencyQuery DependencyQuery;

			while (!Stack.IsEmpty())
			{
				const FName PackageName = Stack.Pop(EAllowShrinking::No);
				if (AllowedPackages.Contains(PackageName))
				{
					continue;
				}

				AllowedPackages.Add(PackageName);

				TArray<FName> Deps;
				AssetRegistry.GetDependencies(PackageName, Deps, DependencyCategory, DependencyQuery);
				for (const FName DepPkg : Deps)
				{
					if (!AllowedPackages.Contains(DepPkg))
					{
						Stack.Add(DepPkg);
					}
				}
			}

			return MakeShared<const TSet<FName>>(MoveTemp(AllowedPackages));
		}
	}

	TSharedPtr<const TSet<FName>> GetAllowedPackageNamesFromLoadedKits()
	{
		UWorldBLDEdMode* EdMode = UWorldBLDEdMode::GetWorldBLDEdMode();
		if (!IsValid(EdMode))
		{
			InvalidateCache();
			return nullptr;
		}

		const int32 KitCount = EdMode->GetLoadedWorldBLDKitCount();
		const int32 BundleCount = EdMode->GetLoadedWorldBLDKitBundleCount();

		const bool bCacheValid =
			(Private::CachedEdMode.Get() == EdMode) &&
			(Private::CachedKitCount == KitCount) &&
			(Private::CachedBundleCount == BundleCount) &&
			Private::CachedAllowedPackages.IsValid();

		if (bCacheValid)
		{
			return Private::CachedAllowedPackages;
		}

		Private::CachedEdMode = EdMode;
		Private::CachedKitCount = KitCount;
		Private::CachedBundleCount = BundleCount;
		Private::CachedAllowedPackages = Private::BuildAllowedPackageNames(*EdMode);

		return Private::CachedAllowedPackages;
	}

	void InvalidateCache()
	{
		Private::CachedEdMode = nullptr;
		Private::CachedAllowedPackages.Reset();
		Private::CachedKitCount = INDEX_NONE;
		Private::CachedBundleCount = INDEX_NONE;
	}
}


