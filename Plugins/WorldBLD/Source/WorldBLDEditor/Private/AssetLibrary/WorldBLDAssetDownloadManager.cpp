#include "AssetLibrary/WorldBLDAssetDownloadManager.h"

#include "AssetLibrary/WorldBLDAssetLibrarySubsystem.h"
#include "Authorization/WorldBLDAuthSubsystem.h"
#include "WorldBLDEditorModule.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetLibrary/WorldBLDAssetUserData.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Texture.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ScopeLock.h"
#include "UObject/ObjectRedirector.h"

#include "AssetLibrary/Jobs/WorldBLDAssetLibraryJobQueue.h"
#include "AssetLibrary/Jobs/WorldBLDAssetLibraryJobs.h"
#include "AssetLibrary/Services/WorldBLDAssetCacheService.h"
#include "AssetLibrary/Services/WorldBLDAssetDownloadService.h"
#include "AssetLibrary/Services/WorldBLDAssetDependencyService.h"
#include "AssetLibrary/Services/WorldBLDAssetImportService.h"
#include "AssetLibrary/Services/WorldBLDAssetImportArtifact.h"
#include "AssetLibrary/Services/WorldBLDAssetStateStore.h"

void FWorldBLDAssetLibraryJobQueueDeleter::operator()(FWorldBLDAssetLibraryJobQueue* Ptr) const
{
	delete Ptr;
}

void FWorldBLDAssetStateStoreDeleter::operator()(FWorldBLDAssetStateStore* Ptr) const
{
	delete Ptr;
}

void FWorldBLDAssetCacheServiceDeleter::operator()(FWorldBLDAssetCacheService* Ptr) const
{
	delete Ptr;
}

void FWorldBLDAssetImportServiceDeleter::operator()(FWorldBLDAssetImportService* Ptr) const
{
	delete Ptr;
}

void FWorldBLDAssetDownloadServiceDeleter::operator()(FWorldBLDAssetDownloadService* Ptr) const
{
	delete Ptr;
}

void FWorldBLDAssetDependencyServiceDeleter::operator()(FWorldBLDAssetDependencyService* Ptr) const
{
	delete Ptr;
}

DEFINE_LOG_CATEGORY_STATIC(LogWorldBLDAssetDownloadManager, Log, All);

namespace
{
	TAutoConsoleVariable<int32> CVarWorldBLDAssetImportDiagnostics(
		TEXT("WorldBLD.AssetLibrary.ImportDiagnostics"),
		0,
		TEXT("Enables diagnostic logs for WorldBLD Asset Library import/download pipeline.\n")
		TEXT("0: Off\n")
		TEXT("1: Basic\n")
		TEXT("2: Verbose (may load assets)\n"));

	bool IsImportDiagnosticsEnabled(int32 MinLevel)
	{
		return CVarWorldBLDAssetImportDiagnostics.GetValueOnAnyThread() >= MinLevel;
	}

	// Legacy upload packages authored their internal package names under this root.
	// If we import those zips into a different root (e.g. /Game/WorldBLD/Assets), references will break.
	constexpr const TCHAR* LegacyPreparedAssetsRootPrefix = TEXT("/Game/WorldBLD/PreparedAssets/");
	constexpr const TCHAR* AssetsRootPrefix = TEXT("/Game/WorldBLD/Assets/");
	constexpr const TCHAR* KitsRootPrefix = TEXT("/Game/WorldBLD/Kits/");

	struct FWorldBLDDependencyAuditStats
	{
		int32 PackagesScanned = 0;
		int32 TotalDependencies = 0;
		int32 MissingDependencyPackages = 0;
		int32 LegacyPreparedAssetsDependencies = 0;
		int32 InternalDependencies = 0;
		int32 ExternalDependencies = 0;
	};

	static FWorldBLDDependencyAuditStats AuditDependenciesUnderPackageRoot(
		const FString& PackageRoot,
		IAssetRegistry& AssetRegistry,
		const FString& DiagnosticLabel,
		int32 MaxPerAssetWarnings = 8,
		int32 MaxAssetsWithWarnings = 25)
	{
		FWorldBLDDependencyAuditStats Stats;

		if (PackageRoot.IsEmpty())
		{
			return Stats;
		}

		TArray<FAssetData> AssetDataList;
		AssetRegistry.GetAssetsByPath(FName(*PackageRoot), AssetDataList, /*bRecursive=*/true);
		Stats.PackagesScanned = AssetDataList.Num();

		// Note: FAssetRegistryDependencyOptions::Get*Query helpers are not exported from the engine DLL in UE5.6,
		// so calling them from a plugin module causes unresolved externals at link time. For this use-case we
		// want to include all package + manage dependencies, so the default query flags are sufficient.
		const UE::AssetRegistry::EDependencyCategory DependencyCategory =
			UE::AssetRegistry::EDependencyCategory::Package | UE::AssetRegistry::EDependencyCategory::Manage;
		const UE::AssetRegistry::FDependencyQuery DependencyQuery;

		int32 AssetsLogged = 0;

		for (const FAssetData& AD : AssetDataList)
		{
			const FName PackageName = AD.PackageName;
			if (PackageName.IsNone())
			{
				continue;
			}

			TArray<FName> Dependencies;
			AssetRegistry.GetDependencies(PackageName, Dependencies, DependencyCategory, DependencyQuery);
			Stats.TotalDependencies += Dependencies.Num();

			int32 PerAssetWarned = 0;

			for (const FName DepPkgName : Dependencies)
			{
				if (DepPkgName.IsNone())
				{
					continue;
				}

				const FString DepPkg = DepPkgName.ToString();

				// Skip non-game packages (Engine, plugins, /Script/*, etc.) for "missing" checks; we're focused on imported content.
				const bool bIsGamePkg = FPackageName::IsValidLongPackageName(DepPkg) && !FPackageName::IsScriptPackage(DepPkg);
				if (!bIsGamePkg)
				{
					continue;
				}

				if (DepPkg.StartsWith(LegacyPreparedAssetsRootPrefix))
				{
					Stats.LegacyPreparedAssetsDependencies++;
				}

				if (DepPkg.StartsWith(PackageRoot))
				{
					Stats.InternalDependencies++;
				}
				else
				{
					Stats.ExternalDependencies++;
				}

				if (!FPackageName::DoesPackageExist(DepPkg))
				{
					Stats.MissingDependencyPackages++;

					if (IsImportDiagnosticsEnabled(2) && AssetsLogged < MaxAssetsWithWarnings && PerAssetWarned < MaxPerAssetWarnings)
					{
						UE_LOG(LogWorldBLDAssetDownloadManager, Warning,
							TEXT("[ImportDiagnostics][Deps][%s] Missing dependency package. AssetPkg='%s' MissingDep='%s'"),
							*DiagnosticLabel,
							*PackageName.ToString(),
							*DepPkg);
						PerAssetWarned++;
					}
				}
			}

			if (PerAssetWarned > 0)
			{
				AssetsLogged++;
			}
		}

		if (IsImportDiagnosticsEnabled(1))
		{
			UE_LOG(LogWorldBLDAssetDownloadManager, Log,
				TEXT("[ImportDiagnostics][Deps][%s] Root='%s' Packages=%d TotalDeps=%d InternalDeps=%d ExternalDeps=%d MissingPkgs=%d LegacyPreparedDeps=%d"),
				*DiagnosticLabel,
				*PackageRoot,
				Stats.PackagesScanned,
				Stats.TotalDependencies,
				Stats.InternalDependencies,
				Stats.ExternalDependencies,
				Stats.MissingDependencyPackages,
				Stats.LegacyPreparedAssetsDependencies);
		}

		return Stats;
	}

	void FixupRedirectorsUnderPackageRoot(const FString& PackageRoot, IAssetRegistry& AssetRegistry)
	{
		if (PackageRoot.IsEmpty())
		{
			return;
		}

		FARFilter RedirectorFilter;
		RedirectorFilter.bRecursivePaths = true;
		RedirectorFilter.PackagePaths.Add(FName(*PackageRoot));
		RedirectorFilter.ClassPaths.Add(UObjectRedirector::StaticClass()->GetClassPathName());

		TArray<FAssetData> RedirectorAssetData;
		AssetRegistry.GetAssets(RedirectorFilter, RedirectorAssetData);
		if (RedirectorAssetData.Num() == 0)
		{
			return;
		}

		TArray<UObjectRedirector*> Redirectors;
		Redirectors.Reserve(RedirectorAssetData.Num());
		for (const FAssetData& AD : RedirectorAssetData)
		{
			UObject* Obj = AD.GetAsset();
			if (UObjectRedirector* Redirector = Cast<UObjectRedirector>(Obj))
			{
				Redirectors.Add(Redirector);
			}
		}

		if (Redirectors.Num() == 0)
		{
			return;
		}

		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		IAssetTools& AssetTools = AssetToolsModule.Get();

		// Non-interactive: do not prompt for source control checkout dialogs in this pipeline.
		AssetTools.FixupReferencers(Redirectors, /*bCheckoutDialogPrompt=*/false);
	}

	bool TryFindManifestFileUnderExtracted(const FString& ExtractedPath, FString& OutManifestPath)
	{
		OutManifestPath.Reset();

		if (ExtractedPath.IsEmpty() || !IFileManager::Get().DirectoryExists(*ExtractedPath))
		{
			return false;
		}

		TArray<FString> ManifestPaths;
		IFileManager::Get().FindFilesRecursive(
			ManifestPaths,
			*ExtractedPath,
			TEXT("*_manifest.json"),
			/*Files*/ true,
			/*Directories*/ false,
			/*ClearFileNames*/ false);

		if (ManifestPaths.Num() == 0)
		{
			// Fallback: accept any json file under extracted (some legacy packages may not follow *_manifest.json naming).
			IFileManager::Get().FindFilesRecursive(
				ManifestPaths,
				*ExtractedPath,
				TEXT("*.json"),
				/*Files*/ true,
				/*Directories*/ false,
				/*ClearFileNames*/ false);
		}

		if (ManifestPaths.Num() == 0)
		{
			return false;
		}

		ManifestPaths.Sort();
		OutManifestPath = ManifestPaths[0];
		return !OutManifestPath.IsEmpty();
	}

	bool TryResolvePreparedRootFromManifestPath(const FString& ManifestPath, FString& OutPreparedRoot)
	{
		OutPreparedRoot.Reset();

		if (ManifestPath.IsEmpty() || !IFileManager::Get().FileExists(*ManifestPath))
		{
			return false;
		}

		// Typical: <PreparedRoot>/Data/<something>_manifest.json
		// Some legacy/bad zips may place the manifest at the prepared root itself.
		const FString ManifestDir = FPaths::GetPath(ManifestPath);
		if (ManifestDir.IsEmpty())
		{
			return false;
		}

		const FString LeafDir = FPaths::GetCleanFilename(ManifestDir);
		if (LeafDir.Equals(TEXT("Data"), ESearchCase::IgnoreCase))
		{
			const FString CandidateRoot = FPaths::GetPath(ManifestDir); // parent of Data
			if (CandidateRoot.IsEmpty())
			{
				return false;
			}
			OutPreparedRoot = CandidateRoot;
		}
		else
		{
			OutPreparedRoot = ManifestDir;
		}
		return true;
	}

	bool TryResolvePreparedRootUnderExtracted(const FString& ExtractedPath, FString& OutPreparedRoot)
	{
		OutPreparedRoot.Reset();

		FString ManifestPath;
		if (!TryFindManifestFileUnderExtracted(ExtractedPath, ManifestPath))
		{
			return false;
		}

		if (!TryResolvePreparedRootFromManifestPath(ManifestPath, OutPreparedRoot))
		{
			return false;
		}

		return !OutPreparedRoot.IsEmpty() && IFileManager::Get().DirectoryExists(*OutPreparedRoot);
	}

	bool TryReadExtractedManifestInfo(
		const FString& ExtractedPath,
		FString& OutRootAssetId,
		FString& OutPreferredSubdir,
		FString& OutPreferredImportedPackageRoot)
	{
		OutRootAssetId.Reset();
		OutPreferredSubdir.Reset();
		OutPreferredImportedPackageRoot.Reset();

		if (ExtractedPath.IsEmpty())
		{
			return false;
		}

		FString ManifestPath;
		if (!TryFindManifestFileUnderExtracted(ExtractedPath, ManifestPath))
		{
			return false;
		}

		FString JsonString;
		if (!FFileHelper::LoadFileToString(JsonString, *ManifestPath) || JsonString.IsEmpty())
		{
			return false;
		}

		TSharedPtr<FJsonObject> RootObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
		if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
		{
			return false;
		}

		// Root asset ID
		{
			FString RootId;
			if (!RootObject->TryGetStringField(TEXT("RootAssetID"), RootId))
			{
				RootObject->TryGetStringField(TEXT("rootAssetID"), RootId);
			}
			if (!RootId.IsEmpty())
			{
				OutRootAssetId = RootId;
			}
		}

		if (IsImportDiagnosticsEnabled(1))
		{
			UE_LOG(LogWorldBLDAssetDownloadManager, Log,
				TEXT("[ImportDiagnostics] Manifest: %s | RootAssetID: %s"),
				*ManifestPath,
				OutRootAssetId.IsEmpty() ? TEXT("<empty>") : *OutRootAssetId);
		}

		// Infer the package root prefix from dependencies' PackagePath.
		// This allows importing legacy zips into /PreparedAssets so references remain valid.
		//
		// Additionally, infer the exact authored package root (e.g. /Game/WorldBLD/Assets/<Name>_<Guid>).
		// If we import into a different root, *all* internal references will be broken.
		const auto TryInferPreferredPackageRootFromDepsArray =
			[&](const TCHAR* DepsFieldName, const TCHAR* PackagePathFieldName) -> bool
		{
			const TArray<TSharedPtr<FJsonValue>>* DepsArrayPtr = nullptr;
			if (!RootObject->TryGetArrayField(FStringView(DepsFieldName), DepsArrayPtr) || DepsArrayPtr == nullptr)
			{
				return false;
			}

			static const TArray<FString> KnownTypeFolders =
			{
				TEXT("Materials"),
				TEXT("Textures"),
				TEXT("Meshes"),
				TEXT("Blueprints"),
				TEXT("Data")
			};

			for (const TSharedPtr<FJsonValue>& V : *DepsArrayPtr)
			{
				if (!V.IsValid() || V->Type != EJson::Object)
				{
					continue;
				}

				const TSharedPtr<FJsonObject> DepObj = V->AsObject();
				if (!DepObj.IsValid())
				{
					continue;
				}

				FString PackagePath;
				if (!DepObj->TryGetStringField(FStringView(PackagePathFieldName), PackagePath) || PackagePath.IsEmpty())
				{
					continue;
				}

				// PackagePath is typically an object path: "/Game/.../Asset.Asset". Convert to the package path.
				int32 DotIndex = INDEX_NONE;
				if (PackagePath.FindChar(TEXT('.'), DotIndex) && DotIndex > 0)
				{
					PackagePath = PackagePath.Left(DotIndex);
				}

				// Derive the authored root ("/Game/.../<RootFolder>") by stripping the first type-folder segment.
				// This is robust even if assets end up under nested subfolders (e.g. ".../Meshes/Variants/...").
				FString RootCandidate;
				for (const FString& TypeFolder : KnownTypeFolders)
				{
					const FString Needle = TEXT("/") + TypeFolder + TEXT("/");
					int32 FoundIndex = PackagePath.Find(Needle, ESearchCase::CaseSensitive, ESearchDir::FromStart);
					if (FoundIndex != INDEX_NONE)
					{
						RootCandidate = PackagePath.Left(FoundIndex);
						break;
					}
				}

				// Fallback: if the package isn't under a known type folder, use its long package path.
				if (RootCandidate.IsEmpty())
				{
					RootCandidate = FPackageName::GetLongPackagePath(PackagePath);
				}

				if (!RootCandidate.IsEmpty())
				{
					OutPreferredImportedPackageRoot = RootCandidate;
					return true;
				}
			}

			return false;
		};

		auto TryInferFromDepsArray = [&](const TCHAR* DepsFieldName, const TCHAR* PackagePathFieldName) -> bool
		{
			const TArray<TSharedPtr<FJsonValue>>* DepsArrayPtr = nullptr;
			if (!RootObject->TryGetArrayField(FStringView(DepsFieldName), DepsArrayPtr) || DepsArrayPtr == nullptr)
			{
				return false;
			}

			for (const TSharedPtr<FJsonValue>& V : *DepsArrayPtr)
			{
				if (!V.IsValid() || V->Type != EJson::Object)
				{
					continue;
				}

				const TSharedPtr<FJsonObject> DepObj = V->AsObject();
				if (!DepObj.IsValid())
				{
					continue;
				}

				FString PackagePath;
				if (!DepObj->TryGetStringField(FStringView(PackagePathFieldName), PackagePath) || PackagePath.IsEmpty())
				{
					continue;
				}

				if (PackagePath.StartsWith(LegacyPreparedAssetsRootPrefix))
				{
					OutPreferredSubdir = TEXT("PreparedAssets");
					return true;
				}

				if (PackagePath.StartsWith(KitsRootPrefix))
				{
					OutPreferredSubdir = TEXT("Kits");
					return true;
				}

				if (PackagePath.StartsWith(AssetsRootPrefix))
				{
					OutPreferredSubdir = TEXT("Assets");
					return true;
				}
			}

			return false;
		};

		// New uploader uses "Dependencies"/"PackagePath". Backend responses may use lowercase variants, so be lenient.
		(void)TryInferFromDepsArray(TEXT("Dependencies"), TEXT("PackagePath"));
		if (OutPreferredSubdir.IsEmpty())
		{
			(void)TryInferFromDepsArray(TEXT("dependencies"), TEXT("packagePath"));
		}

		(void)TryInferPreferredPackageRootFromDepsArray(TEXT("Dependencies"), TEXT("PackagePath"));
		if (OutPreferredImportedPackageRoot.IsEmpty())
		{
			(void)TryInferPreferredPackageRootFromDepsArray(TEXT("dependencies"), TEXT("packagePath"));
		}

		if (IsImportDiagnosticsEnabled(1))
		{
			UE_LOG(LogWorldBLDAssetDownloadManager, Log,
				TEXT("[ImportDiagnostics] Manifest-derived import info: PreferredSubdir='%s' PreferredImportedPackageRoot='%s'"),
				OutPreferredSubdir.IsEmpty() ? TEXT("<empty>") : *OutPreferredSubdir,
				OutPreferredImportedPackageRoot.IsEmpty() ? TEXT("<empty>") : *OutPreferredImportedPackageRoot);
		}

		return !OutRootAssetId.IsEmpty() || !OutPreferredSubdir.IsEmpty() || !OutPreferredImportedPackageRoot.IsEmpty();
	}
}

void UWorldBLDAssetDownloadCallbackProxy::HandleProgress(float Progress)
{
	if (UserOnProgress.IsBound())
	{
		UserOnProgress.Execute(Progress);
	}

	if (Manager.IsValid())
	{
		Manager->OnDownloadProgress_Internal(AssetId, Progress);
	}
}

void UWorldBLDAssetDownloadCallbackProxy::HandleComplete(bool bWasSuccessful)
{
	if (Manager.IsValid())
	{
		Manager->OnDownloadComplete_Internal(AssetId, bWasSuccessful);
	}

	if (UserOnComplete.IsBound())
	{
		UserOnComplete.Execute(bWasSuccessful);
	}
}

void UWorldBLDAssetDownloadManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	DownloaderLibrary = GEditor ? GEditor->GetEditorSubsystem<UCurlDownloaderLibrary>() : nullptr;
	AssetLibrarySubsystem = GEditor ? GEditor->GetEditorSubsystem<UWorldBLDAssetLibrarySubsystem>() : nullptr;
	AuthSubsystem = GEditor ? GEditor->GetEditorSubsystem<UWorldBLDAuthSubsystem>() : nullptr;
	JobQueue.Reset(new FWorldBLDAssetLibraryJobQueue(TWeakObjectPtr<UWorldBLDAssetDownloadManager>(this)));
	StateStore.Reset(new FWorldBLDAssetStateStore());
	CacheService.Reset(new FWorldBLDAssetCacheService());
	ImportService.Reset(new FWorldBLDAssetImportService());
	DownloadService.Reset(new FWorldBLDAssetDownloadService(DownloaderLibrary));
	DependencyService.Reset(new FWorldBLDAssetDependencyService());

	IFileManager::Get().MakeDirectory(*FWorldBLDAssetStateStore::GetCacheDirectory(), true);

	LoadManifest();

	{
		FScopeLock Lock(&StateMutex);
		for (auto& Pair : AssetStates)
		{
			if (Pair.Value.Status == EWorldBLDAssetStatus::Downloading)
			{
				Pair.Value.Status = EWorldBLDAssetStatus::Owned;
			}
		}
	}

	ValidateStatesForCurrentProject();
	ScanContentForLocalImports();

	if (AuthSubsystem)
	{
		AuthSubsystem->OnLicensesUpdated.AddDynamic(this, &UWorldBLDAssetDownloadManager::HandleLicensesUpdated);
		AuthSubsystem->OnSessionValid.AddDynamic(this, &UWorldBLDAssetDownloadManager::HandleSessionValid);
		AuthSubsystem->OnSessionInvalid.AddDynamic(this, &UWorldBLDAssetDownloadManager::HandleSessionInvalid);
	}

	UE_LOG(LogWorldBLDAssetDownloadManager, Log, TEXT("AssetDownloadManager initialized"));
}

void UWorldBLDAssetDownloadManager::HandleLicensesUpdated(const TArray<FString>& AuthorizedTools)
{
	(void)AuthorizedTools;
}

void UWorldBLDAssetDownloadManager::HandleSessionValid()
{
}

void UWorldBLDAssetDownloadManager::HandleSessionInvalid()
{
	// Conservatively mark subscription as unknown when session becomes invalid.
	bool bShouldBroadcast = false;
	{
		FScopeLock Lock(&StateMutex);
		bShouldBroadcast = (bHasActiveSubscriptionKnown || bHasActiveSubscription);
		bHasActiveSubscriptionKnown = false;
		bHasActiveSubscription = false;
	}

	if (bShouldBroadcast)
	{
		OnSubscriptionStatusChanged.Broadcast(/*bIsKnown=*/false, /*bHasActiveSubscription=*/false);
	}
}

void UWorldBLDAssetDownloadManager::Deinitialize()
{
	SaveManifest();

	JobQueue.Reset();
	StateStore.Reset();
	CacheService.Reset();
	ImportService.Reset();
	DownloadService.Reset();
	DependencyService.Reset();
	DownloaderLibrary = nullptr;
	AssetLibrarySubsystem = nullptr;
	AuthSubsystem = nullptr;

	Super::Deinitialize();
}

FString UWorldBLDAssetDownloadManager::GetCacheDirectory() const
{
	return FWorldBLDAssetStateStore::GetCacheDirectory();
}

FString UWorldBLDAssetDownloadManager::GetManifestPath() const
{
	return FWorldBLDAssetStateStore::GetManifestPath();
}

void UWorldBLDAssetDownloadManager::LoadManifest()
{
	TMap<FString, FWorldBLDAssetDownloadState> LoadedStates;
	if (!FWorldBLDAssetStateStore::LoadStates(LoadedStates))
	{
		return;
	}

	FScopeLock Lock(&StateMutex);
	AssetStates = MoveTemp(LoadedStates);
}

void UWorldBLDAssetDownloadManager::SaveManifest()
{
	TMap<FString, FWorldBLDAssetDownloadState> StatesToSave;
	{
		FScopeLock Lock(&StateMutex);
		StatesToSave = AssetStates;
	}

	(void)FWorldBLDAssetStateStore::SaveStates(StatesToSave);
}

void UWorldBLDAssetDownloadManager::ValidateStatesForCurrentProject()
{
	bool bModified = false;

	{
		FScopeLock Lock(&StateMutex);

		for (auto& Pair : AssetStates)
		{
			FWorldBLDAssetDownloadState& State = Pair.Value;

			if (State.Status == EWorldBLDAssetStatus::Imported)
			{
				// The manifest is user-global, not per-project. Verify that the imported
				// content actually exists in *this* project's Content directory.
				bool bImportedContentExists = false;

				if (!State.ImportedPackageRoot.IsEmpty())
				{
					FString RelativePath = State.ImportedPackageRoot;
					if (RelativePath.RemoveFromStart(TEXT("/Game/")))
					{
						const FString DiskPath = FPaths::Combine(FPaths::ProjectContentDir(), RelativePath);
						bImportedContentExists = IFileManager::Get().DirectoryExists(*DiskPath);
					}
				}

				if (!bImportedContentExists)
				{
					// Downgrade: check if the downloaded cache still exists on disk.
					const bool bCacheExists = !State.CachedPath.IsEmpty()
						&& IFileManager::Get().DirectoryExists(*State.CachedPath);

					State.Status = bCacheExists ? EWorldBLDAssetStatus::Downloaded : EWorldBLDAssetStatus::Owned;
					State.ImportedPath.Reset();
					State.ImportedPackageRoot.Reset();
					State.LastModified = FDateTime::UtcNow();
					bModified = true;
				}
			}
			else if (State.Status == EWorldBLDAssetStatus::Downloaded)
			{
				// Check if the authored content already exists in the current project
				// (e.g. committed via source control, or imported outside the normal flow).
				// If so, upgrade to Imported immediately.
				bool bContentAlreadyExists = false;
				if (!State.AuthoredPackageRoot.IsEmpty())
				{
					FString RelativePath = State.AuthoredPackageRoot;
					if (RelativePath.RemoveFromStart(TEXT("/Game/")))
					{
						const FString DiskPath = FPaths::Combine(FPaths::ProjectContentDir(), RelativePath);
						bContentAlreadyExists = IFileManager::Get().DirectoryExists(*DiskPath);
					}
				}

				if (bContentAlreadyExists)
				{
					State.Status = EWorldBLDAssetStatus::Imported;
					State.ImportedPackageRoot = State.AuthoredPackageRoot;
					State.LastModified = FDateTime::UtcNow();
					bModified = true;
				}
				else
				{
					// Validate that the cached/extracted files still exist on disk.
					const bool bCacheExists = !State.CachedPath.IsEmpty()
						&& IFileManager::Get().DirectoryExists(*State.CachedPath);

					if (!bCacheExists)
					{
						State.Status = EWorldBLDAssetStatus::Owned;
						State.CachedPath.Reset();
						State.CachedManifestPath.Reset();
						State.AuthoredPackageRoot.Reset();
						State.LastModified = FDateTime::UtcNow();
						bModified = true;
					}
				}
			}
		}
	}

	if (bModified)
	{
		SaveManifest();
	}
}

void UWorldBLDAssetDownloadManager::ScanContentForLocalImports()
{
	LocalImportsByRootAssetID.Reset();

	// Scan every known WorldBLD content root for in-project manifest JSONs.
	static const TArray<FString> ContentRoots =
	{
		TEXT("WorldBLD/Assets"),
		TEXT("WorldBLD/Kits"),
		TEXT("WorldBLD/PreparedAssets")
	};

	for (const FString& Root : ContentRoots)
	{
		const FString DiskRoot = FPaths::Combine(FPaths::ProjectContentDir(), Root);
		if (!IFileManager::Get().DirectoryExists(*DiskRoot))
		{
			continue;
		}

		TArray<FString> ManifestFiles;
		IFileManager::Get().FindFilesRecursive(
			ManifestFiles,
			*DiskRoot,
			TEXT("*_manifest.json"),
			/*Files*/ true,
			/*Directories*/ false,
			/*ClearFileNames*/ false);

		for (const FString& ManifestPath : ManifestFiles)
		{
			FString JsonString;
			if (!FFileHelper::LoadFileToString(JsonString, *ManifestPath) || JsonString.IsEmpty())
			{
				continue;
			}

			TSharedPtr<FJsonObject> RootObject;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
			if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
			{
				continue;
			}

			FString RootAssetID;
			if (!RootObject->TryGetStringField(TEXT("RootAssetID"), RootAssetID))
			{
				RootObject->TryGetStringField(TEXT("rootAssetID"), RootAssetID);
			}
			if (RootAssetID.IsEmpty())
			{
				continue;
			}

			FString AuthoredPackageRoot;
			if (!RootObject->TryGetStringField(TEXT("AuthoredPackageRoot"), AuthoredPackageRoot))
			{
				RootObject->TryGetStringField(TEXT("authoredPackageRoot"), AuthoredPackageRoot);
			}
			if (AuthoredPackageRoot.IsEmpty())
			{
				continue;
			}

			LocalImportsByRootAssetID.Add(RootAssetID, AuthoredPackageRoot);
		}
	}
}

void UWorldBLDAssetDownloadManager::ReconcileImportedAssets(const TArray<FWorldBLDAssetInfo>& FetchedAssets)
{
	if (LocalImportsByRootAssetID.Num() == 0)
	{
		return;
	}

	TArray<TPair<FString, EWorldBLDAssetStatus>> StatusChanges;

	{
		FScopeLock Lock(&StateMutex);

		for (const FWorldBLDAssetInfo& Asset : FetchedAssets)
		{
			if (Asset.ID.IsEmpty() || Asset.RootAssetID.IsEmpty())
			{
				continue;
			}

			// Already tracked as Imported — nothing to do.
			if (const FWorldBLDAssetDownloadState* Existing = AssetStates.Find(Asset.ID))
			{
				if (Existing->Status == EWorldBLDAssetStatus::Imported)
				{
					continue;
				}
			}

			const FString* LocalPackageRoot = LocalImportsByRootAssetID.Find(Asset.RootAssetID);
			if (!LocalPackageRoot)
			{
				continue;
			}

			// This API asset matches content that's already on disk in this project.
			FWorldBLDAssetDownloadState& State = AssetStates.FindOrAdd(Asset.ID);
			State.AssetId = Asset.ID;
			State.AssetName = Asset.Name;
			State.Status = EWorldBLDAssetStatus::Imported;
			State.ImportedPackageRoot = *LocalPackageRoot;
			State.LastModified = FDateTime::UtcNow();

			StatusChanges.Emplace(Asset.ID, EWorldBLDAssetStatus::Imported);
		}
	}

	if (StatusChanges.Num() > 0)
	{
		for (const TPair<FString, EWorldBLDAssetStatus>& Change : StatusChanges)
		{
			OnAssetStatusChanged.Broadcast(Change.Key, Change.Value);
		}

		SaveManifest();
	}
}

FWorldBLDAssetDownloadState UWorldBLDAssetDownloadManager::GetOrAddState_Internal(const FString& AssetId)
{
	FScopeLock Lock(&StateMutex);
	if (FWorldBLDAssetDownloadState* Found = AssetStates.Find(AssetId))
	{
		return *Found;
	}

	FWorldBLDAssetDownloadState NewState;
	NewState.AssetId = AssetId;
	NewState.Status = EWorldBLDAssetStatus::NotOwned;
	NewState.LastModified = FDateTime::UtcNow();
	AssetStates.Add(AssetId, NewState);
	return NewState;
}

void UWorldBLDAssetDownloadManager::SetAssetStatus_Internal(const FString& AssetId, EWorldBLDAssetStatus NewStatus)
{
	{
		FScopeLock Lock(&StateMutex);
		FWorldBLDAssetDownloadState& State = AssetStates.FindOrAdd(AssetId);
		State.AssetId = AssetId;
		State.Status = NewStatus;
		State.LastModified = FDateTime::UtcNow();
	}

	OnAssetStatusChanged.Broadcast(AssetId, NewStatus);
}

EWorldBLDAssetStatus UWorldBLDAssetDownloadManager::GetAssetStatus(const FString& AssetId)
{
	FScopeLock Lock(&StateMutex);
	if (const FWorldBLDAssetDownloadState* Found = AssetStates.Find(AssetId))
	{
		return Found->Status;
	}
	return EWorldBLDAssetStatus::NotOwned;
}

bool UWorldBLDAssetDownloadManager::IsDownloadInProgress(const FString& AssetId)
{
	return DownloadService.IsValid() ? DownloadService->IsDownloadInProgress(AssetId) : false;
}

float UWorldBLDAssetDownloadManager::GetDownloadProgress(const FString& AssetId)
{
	return DownloadService.IsValid() ? DownloadService->GetDownloadProgress(AssetId) : -1.0f;
}

void UWorldBLDAssetDownloadManager::DownloadAsset(const FString& AssetId, const FString& AssetName, const FString& DownloadUrl, bool bIsKit, FDOnDownloadProgress OnProgress, FDOnComplete OnComplete)
{
	if (!DownloadService.IsValid())
	{
		OnComplete.ExecuteIfBound(false);
		return;
	}

	if (AssetId.IsEmpty() || DownloadUrl.IsEmpty())
	{
		OnComplete.ExecuteIfBound(false);
		return;
	}

	const FString CacheDir = FPaths::Combine(GetCacheDirectory(), AssetId);
	IFileManager::Get().MakeDirectory(*CacheDir, true);

	const FString ArchivePath = FPaths::Combine(CacheDir, TEXT("Archive.zip"));
	const FString ExtractedPath = FPaths::Combine(CacheDir, TEXT("Extracted"));

	const bool bAlreadyInProgress = DownloadService->IsDownloadInProgress(AssetId);

	{
		FScopeLock Lock(&StateMutex);
		FWorldBLDAssetDownloadState& State = AssetStates.FindOrAdd(AssetId);
		State.AssetId = AssetId;
		State.AssetName = AssetName;
		State.bIsKit = bIsKit;
		State.CachedPath = ExtractedPath;
		State.LastModified = FDateTime::UtcNow();
	}

	if (!bAlreadyInProgress)
	{
		SetAssetStatus_Internal(AssetId, EWorldBLDAssetStatus::Downloading);
		SaveManifest();
	}

	UWorldBLDAssetDownloadCallbackProxy* Proxy = NewObject<UWorldBLDAssetDownloadCallbackProxy>(this);
	Proxy->AssetId = AssetId;
	Proxy->Manager = this;
	Proxy->UserOnProgress = OnProgress;
	Proxy->UserOnComplete = OnComplete;

	FDOnDownloadProgress ProxyProgress;
	ProxyProgress.BindDynamic(Proxy, &UWorldBLDAssetDownloadCallbackProxy::HandleProgress);
	FDOnComplete ProxyComplete;
	ProxyComplete.BindDynamic(Proxy, &UWorldBLDAssetDownloadCallbackProxy::HandleComplete);

	{
		FScopeLock Lock(&StateMutex);
		ActiveCallbacks.Add(AssetId, Proxy);
	}

	DownloadService->RequestFileDownload(AssetId, DownloadUrl, ArchivePath, TEXT(""), ProxyProgress, ProxyComplete);
}

void UWorldBLDAssetDownloadManager::CancelDownload(const FString& AssetId)
{
	if (DownloadService.IsValid())
	{
		DownloadService->CancelDownload(AssetId);
	}

	{
		FScopeLock Lock(&StateMutex);
		ActiveCallbacks.Remove(AssetId);
	}

	SetAssetStatus_Internal(AssetId, EWorldBLDAssetStatus::Owned);
	SaveManifest();
}

void UWorldBLDAssetDownloadManager::OnDownloadProgress_Internal(const FString& AssetId, float Progress)
{
	(void)AssetId;
	(void)Progress;
}

void UWorldBLDAssetDownloadManager::OnDownloadComplete_Internal(const FString& AssetId, bool bSuccess)
{
	{
		FScopeLock Lock(&StateMutex);
		ActiveCallbacks.Remove(AssetId);
	}

	FWorldBLDAssetDownloadState State;
	{
		FScopeLock Lock(&StateMutex);
		if (const FWorldBLDAssetDownloadState* Found = AssetStates.Find(AssetId))
		{
			State = *Found;
		}
	}

	if (!bSuccess)
	{
		SetAssetStatus_Internal(AssetId, EWorldBLDAssetStatus::Owned);
		SaveManifest();
		return;
	}

	const FString CacheDir = FPaths::Combine(GetCacheDirectory(), AssetId);
	const FString ArchivePath = FPaths::Combine(CacheDir, TEXT("Archive.zip"));
	const FString ExtractedPath = FPaths::Combine(CacheDir, TEXT("Extracted"));

	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	if (PF.DirectoryExists(*ExtractedPath))
	{
		PF.DeleteDirectoryRecursively(*ExtractedPath);
	}

	if (!ExtractAssetArchive(ArchivePath, ExtractedPath))
	{
		SetAssetStatus_Internal(AssetId, EWorldBLDAssetStatus::Owned);
		SaveManifest();
		return;
	}

	// Some archives contain a wrapper folder (e.g. <Name>_<Guid>/...) under the extracted root.
	// Resolve the "prepared root" (where Materials/Meshes/.../Data live) so package paths match on import.
	FString PreparedRootPath = ExtractedPath;
	{
		FString ResolvedRoot;
		if (TryResolvePreparedRootUnderExtracted(ExtractedPath, ResolvedRoot))
		{
			PreparedRootPath = ResolvedRoot;
		}
	}

	if (IsImportDiagnosticsEnabled(1) && PreparedRootPath != ExtractedPath)
	{
		UE_LOG(LogWorldBLDAssetDownloadManager, Log,
			TEXT("[ImportDiagnostics] Resolved prepared root under extracted: Extracted='%s' PreparedRoot='%s'"),
			*ExtractedPath,
			*PreparedRootPath);
	}

	ValidateAssetStructure(PreparedRootPath);

	FWorldBLDDownloadedArtifact Artifact;
	const bool bHasArtifact = CacheService.IsValid() && CacheService->TryLoadDownloadedArtifact(PreparedRootPath, Artifact);
	if (!bHasArtifact)
	{
		UE_LOG(LogWorldBLDAssetDownloadManager, Warning,
			TEXT("Downloaded archive extracted, but manifest/artifact could not be loaded. AssetId='%s' Extracted='%s' PreparedRoot='%s'"),
			*AssetId,
			*ExtractedPath,
			*PreparedRootPath);

		SetAssetStatus_Internal(AssetId, EWorldBLDAssetStatus::Owned);
		SaveManifest();
		return;
	}

	{
		FScopeLock Lock(&StateMutex);
		FWorldBLDAssetDownloadState& Mutable = AssetStates.FindOrAdd(AssetId);
		// Cache the *prepared root* so import copies the correct folder structure (no extra wrapper directory).
		Mutable.CachedPath = PreparedRootPath;
		Mutable.CachedManifestPath = Artifact.ManifestPath;
		Mutable.AuthoredPackageRoot = Artifact.AuthoredPackageRoot;
		Mutable.LastModified = FDateTime::UtcNow();
	}

	SetAssetStatus_Internal(AssetId, EWorldBLDAssetStatus::Downloaded);
	SaveManifest();
}

bool UWorldBLDAssetDownloadManager::ExtractAssetArchive(const FString& ArchivePath, const FString& DestinationPath)
{
	return CacheService.IsValid() ? CacheService->ExtractArchive(ArchivePath, DestinationPath) : false;
}

bool UWorldBLDAssetDownloadManager::ValidateAssetStructure(const FString& AssetPath) const
{
	// Validation is for diagnostics only. Real packages may omit empty folders inside zips.
	if (!IsImportDiagnosticsEnabled(1))
	{
		return true;
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*AssetPath))
	{
		return false;
	}

	const TArray<FString> Expected = { TEXT("Materials"), TEXT("Textures"), TEXT("Meshes"), TEXT("Blueprints"), TEXT("Data") };
	for (const FString& Folder : Expected)
	{
		const FString FolderPath = FPaths::Combine(AssetPath, Folder);
		if (!PlatformFile.DirectoryExists(*FolderPath))
		{
			UE_LOG(LogWorldBLDAssetDownloadManager, Warning, TEXT("Missing folder: %s"), *FolderPath);
		}
	}
	return true;
}

bool UWorldBLDAssetDownloadManager::CopyAssetToProject(const FString& SourcePath, const FString& DestinationPath)
{
	return ImportService.IsValid() ? ImportService->CopyPreparedTreeToProjectContent(SourcePath, DestinationPath) : false;
}

bool UWorldBLDAssetDownloadManager::ImportAsset(const FString& AssetId)
{
	if (!JobQueue.IsValid())
	{
		return false;
	}

	JobQueue->Enqueue(MakeShared<FWorldBLDImportJob>(TWeakObjectPtr<UWorldBLDAssetDownloadManager>(this), AssetId));
	return true;
}

bool UWorldBLDAssetDownloadManager::ImportAsset_Immediate(const FString& AssetId)
{
	FWorldBLDAssetDownloadState State;
	{
		FScopeLock Lock(&StateMutex);
		if (const FWorldBLDAssetDownloadState* Found = AssetStates.Find(AssetId))
		{
			State = *Found;
		}
		else
		{
			return false;
		}
	}

	if (State.Status != EWorldBLDAssetStatus::Downloaded)
	{
		return false;
	}

	if (!CacheService.IsValid() || !ImportService.IsValid())
	{
		UE_LOG(LogWorldBLDAssetDownloadManager, Warning, TEXT("Import failed: services unavailable. AssetId='%s'"), *AssetId);
		OnImportComplete.Broadcast(AssetId, false);
		return false;
	}

	FWorldBLDDownloadedArtifact Artifact;
	if (!CacheService->TryLoadDownloadedArtifact(State.CachedPath, Artifact))
	{
		// Recover from stale/incorrect cached path by re-resolving the prepared root from the extracted folder.
		const FString ExtractedPath = FPaths::Combine(GetCacheDirectory(), AssetId, TEXT("Extracted"));
		FString ResolvedRoot;
		if (TryResolvePreparedRootUnderExtracted(ExtractedPath, ResolvedRoot) && ResolvedRoot != State.CachedPath)
		{
			if (CacheService->TryLoadDownloadedArtifact(ResolvedRoot, Artifact))
			{
				FScopeLock Lock(&StateMutex);
				FWorldBLDAssetDownloadState& Mutable = AssetStates.FindOrAdd(AssetId);
				Mutable.CachedPath = ResolvedRoot;
				Mutable.CachedManifestPath = Artifact.ManifestPath;
				Mutable.AuthoredPackageRoot = Artifact.AuthoredPackageRoot;
				Mutable.LastModified = FDateTime::UtcNow();
			}
			else
			{
				UE_LOG(LogWorldBLDAssetDownloadManager, Warning,
					TEXT("Import failed: could not load manifest/artifact. AssetId='%s' CachedPath='%s' ResolvedPath='%s'"),
					*AssetId,
					*State.CachedPath,
					*ResolvedRoot);
				OnImportComplete.Broadcast(AssetId, false);
				return false;
			}
		}
		else
		{
			UE_LOG(LogWorldBLDAssetDownloadManager, Warning,
				TEXT("Import failed: could not load manifest/artifact. AssetId='%s' CachedPath='%s'"),
				*AssetId,
				*State.CachedPath);
			OnImportComplete.Broadcast(AssetId, false);
			return false;
		}
	}

	if (Artifact.Files.Num() == 0)
	{
		UE_LOG(LogWorldBLDAssetDownloadManager, Warning, TEXT("Import failed: artifact has no files. AssetId='%s'"), *AssetId);
		OnImportComplete.Broadcast(AssetId, false);
		return false;
	}

	FString ImportedPackageRoot;
	FString ImportedDiskRoot;
	if (!ImportService->ImportDownloadedArtifact(Artifact, ImportedPackageRoot, ImportedDiskRoot))
	{
		UE_LOG(LogWorldBLDAssetDownloadManager, Warning,
			TEXT("Import failed: could not copy/scan files. AssetId='%s' PreparedRoot='%s' AuthoredPackageRoot='%s'"),
			*AssetId,
			*Artifact.PreparedRootOnDisk,
			*Artifact.AuthoredPackageRoot);
		OnImportComplete.Broadcast(AssetId, false);
		return false;
	}

	{
		FScopeLock Lock(&StateMutex);
		FWorldBLDAssetDownloadState& Mutable = AssetStates.FindOrAdd(AssetId);
		Mutable.ImportedPath = ImportedDiskRoot;
		Mutable.ImportedPackageRoot = ImportedPackageRoot;
		Mutable.LastModified = FDateTime::UtcNow();
	}

	InvalidateLocalAssetCache();
	SetAssetStatus_Internal(AssetId, EWorldBLDAssetStatus::Imported);

	if (State.bIsKit)
	{
		FWorldBLDEditorModule& Module = FModuleManager::GetModuleChecked<FWorldBLDEditorModule>(TEXT("WorldBLDEditor"));
		Module.ScanContentForKitBundles();
	}

	OnImportComplete.Broadcast(AssetId, true);
	SaveManifest();
	return true;
}

bool UWorldBLDAssetDownloadManager::GetImportedPackageRoot(const FString& AssetId, FString& OutImportedPackageRoot) const
{
	OutImportedPackageRoot.Reset();

	if (AssetId.IsEmpty())
	{
		return false;
	}

	FScopeLock Lock(&StateMutex);
	if (const FWorldBLDAssetDownloadState* Found = AssetStates.Find(AssetId))
	{
		OutImportedPackageRoot = Found->ImportedPackageRoot;
	}

	return !OutImportedPackageRoot.IsEmpty();
}

bool UWorldBLDAssetDownloadManager::UninstallAsset(const FString& AssetId, bool bDeleteCache, bool bDeleteImportedContent)
{
	if (!JobQueue.IsValid())
	{
		return false;
	}

	JobQueue->Enqueue(MakeShared<FWorldBLDUninstallJob>(
		TWeakObjectPtr<UWorldBLDAssetDownloadManager>(this),
		AssetId,
		bDeleteCache,
		bDeleteImportedContent));
	return true;
}

bool UWorldBLDAssetDownloadManager::UninstallAsset_Immediate(const FString& AssetId, bool bDeleteCache, bool bDeleteImportedContent)
{
	if (AssetId.IsEmpty())
	{
		return false;
	}

	// Best-effort: cancel any active download for this asset before we start deleting files.
	CancelDownload(AssetId);

	FWorldBLDAssetDownloadState State;
	{
		FScopeLock Lock(&StateMutex);
		if (const FWorldBLDAssetDownloadState* Found = AssetStates.Find(AssetId))
		{
			State = *Found;
		}
		else
		{
			return false;
		}
	}

	bool bDidAnything = false;

	// 1) Delete imported assets (Editor-safe delete) + delete the content folder on disk.
	if (bDeleteImportedContent && !State.ImportedPackageRoot.IsEmpty())
	{
		// Per request: Uninstall Asset should remove content imported under /Game/WorldBLD/Assets only.
		if (State.ImportedPackageRoot.StartsWith(AssetsRootPrefix))
		{
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

			TArray<FAssetData> ImportedAssetDataList;
			AssetRegistry.GetAssetsByPath(FName(*State.ImportedPackageRoot), ImportedAssetDataList, /*bRecursive=*/true);

			if (ImportedAssetDataList.Num() > 0)
			{
				// Delete without extra confirmation: the caller (UI) is expected to prompt the user.
				ObjectTools::DeleteAssets(ImportedAssetDataList, /*bShowConfirmation=*/false);
				bDidAnything = true;
			}

			// Clean up any leftover folder on disk (e.g., empty directories).
			FString RelativeContentPath = State.ImportedPackageRoot;
			RelativeContentPath.RemoveFromStart(TEXT("/Game/"));
			if (!RelativeContentPath.IsEmpty())
			{
				const FString ImportedDiskPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectContentDir(), RelativeContentPath));
				IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
				if (PlatformFile.DirectoryExists(*ImportedDiskPath))
				{
					PlatformFile.DeleteDirectoryRecursively(*ImportedDiskPath);
					bDidAnything = true;
				}
			}

			// Refresh registries/caches so dependency checks and UI state reflect the deletion quickly.
			InvalidateLocalAssetCache();
			AssetRegistry.ScanPathsSynchronous({ State.ImportedPackageRoot }, /*bForceRescan=*/true);
		}
	}

	// 2) Delete cached download/extracted files.
	if (bDeleteCache)
	{
		const FString CacheDir = FPaths::Combine(GetCacheDirectory(), AssetId);
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		if (PlatformFile.DirectoryExists(*CacheDir))
		{
			PlatformFile.DeleteDirectoryRecursively(*CacheDir);
			bDidAnything = true;
		}
	}

	if (!bDidAnything)
	{
		return false;
	}

	// 3) Update state + manifest. This does not change remote ownership; we revert the local status to "Owned"
	// (unless it was never owned).
	{
		FScopeLock Lock(&StateMutex);
		FWorldBLDAssetDownloadState& Mutable = AssetStates.FindOrAdd(AssetId);
		Mutable.CachedPath.Reset();
		Mutable.ImportedPath.Reset();
		Mutable.ImportedPackageRoot.Reset();
		Mutable.LastModified = FDateTime::UtcNow();
	}

	const EWorldBLDAssetStatus NewStatus =
		(State.Status == EWorldBLDAssetStatus::NotOwned) ? EWorldBLDAssetStatus::NotOwned : EWorldBLDAssetStatus::Owned;

	SetAssetStatus_Internal(AssetId, NewStatus);
	SaveManifest();
	return true;
}

void UWorldBLDAssetDownloadManager::RefreshOwnershipStatus()
{
	UE_LOG(LogWorldBLDAssetDownloadManager, Log, TEXT("[RefreshOwnershipStatus] Starting ownership refresh"));

	// If AuthSubsystem is null, try to fetch it on-demand
	if (!AuthSubsystem && GEditor)
	{
		UE_LOG(LogWorldBLDAssetDownloadManager, Warning, TEXT("[RefreshOwnershipStatus] AuthSubsystem is null, fetching on-demand"));
		AuthSubsystem = GEditor->GetEditorSubsystem<UWorldBLDAuthSubsystem>();
	}

	if (!AuthSubsystem)
	{
		UE_LOG(LogWorldBLDAssetDownloadManager, Warning, TEXT("[RefreshOwnershipStatus] AuthSubsystem is null and couldn't be fetched"));
		{
			FScopeLock Lock(&StateMutex);
			bHasActiveSubscription = false;
			bHasActiveSubscriptionKnown = false;
		}
		OnSubscriptionStatusChanged.Broadcast(/*bIsKnown=*/false, /*bHasActiveSubscription=*/false);
		return;
	}

	const FString Token = AuthSubsystem->GetSessionToken();
	if (Token.IsEmpty())
	{
		UE_LOG(LogWorldBLDAssetDownloadManager, Warning, TEXT("[RefreshOwnershipStatus] Token is empty"));
		{
			FScopeLock Lock(&StateMutex);
			bHasActiveSubscription = false;
			bHasActiveSubscriptionKnown = false;
		}
		OnSubscriptionStatusChanged.Broadcast(/*bIsKnown=*/false, /*bHasActiveSubscription=*/false);
		return;
	}

	UE_LOG(LogWorldBLDAssetDownloadManager, Log, TEXT("[RefreshOwnershipStatus] Requesting owned-apps from server"));

	FHttpModule& Http = FHttpModule::Get();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http.CreateRequest();
	Request->SetURL(TEXT("https://worldbld.com/api/auth/desktop/owned-apps"));
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("X-Authorization"), Token);

	Request->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bOk)
	{
		if (!bOk || !Resp.IsValid())
		{
			UE_LOG(LogWorldBLDAssetDownloadManager, Warning, TEXT("[RefreshOwnershipStatus] Request failed or invalid response"));
			{
				FScopeLock Lock(&StateMutex);
				bHasActiveSubscription = false;
				bHasActiveSubscriptionKnown = false;
			}
			OnSubscriptionStatusChanged.Broadcast(/*bIsKnown=*/false, /*bHasActiveSubscription=*/false);
			return;
		}

		int32 ResponseCode = Resp->GetResponseCode();
		UE_LOG(LogWorldBLDAssetDownloadManager, Log, TEXT("[RefreshOwnershipStatus] Response code: %d"), ResponseCode);

		if (ResponseCode != 200)
		{
			UE_LOG(LogWorldBLDAssetDownloadManager, Warning, TEXT("[RefreshOwnershipStatus] Non-200 response code"));
			{
				FScopeLock Lock(&StateMutex);
				bHasActiveSubscription = false;
				bHasActiveSubscriptionKnown = false;
			}
			OnSubscriptionStatusChanged.Broadcast(/*bIsKnown=*/false, /*bHasActiveSubscription=*/false);
			return;
		}

		TSharedPtr<FJsonObject> JsonObject;
		const FString Body = Resp->GetContentAsString();

		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
		if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
		{
			UE_LOG(LogWorldBLDAssetDownloadManager, Error, TEXT("[RefreshOwnershipStatus] Failed to parse JSON"));
			{
				FScopeLock Lock(&StateMutex);
				bHasActiveSubscription = false;
				bHasActiveSubscriptionKnown = false;
			}
			OnSubscriptionStatusChanged.Broadcast(/*bIsKnown=*/false, /*bHasActiveSubscription=*/false);
			return;
		}

		TSet<FString> Owned;
		bool bNewHasActiveSubscription = false;

		// Subscription parsing: tolerate backend schema changes.
		// Supported shapes:
		// - { "hasActiveSubscription": true/false }
		// - { "subscriptions": [ ... ] } where entries may be strings or objects
		// - { "activeSubscriptions": [ ... ] } (alternate naming)
		{
			bool bPayloadBool = false;
			if (JsonObject->TryGetBoolField(TEXT("hasActiveSubscription"), bPayloadBool)
				|| JsonObject->TryGetBoolField(TEXT("activeSubscription"), bPayloadBool)
				|| JsonObject->TryGetBoolField(TEXT("subscriptionActive"), bPayloadBool))
			{
				bNewHasActiveSubscription = bPayloadBool;
			}
			else
			{
				const TArray<TSharedPtr<FJsonValue>>* SubsPtr = nullptr;
				if (JsonObject->TryGetArrayField(TEXT("subscriptions"), SubsPtr)
					|| JsonObject->TryGetArrayField(TEXT("activeSubscriptions"), SubsPtr))
				{
					if (SubsPtr && SubsPtr->Num() > 0)
					{
						for (const TSharedPtr<FJsonValue>& V : *SubsPtr)
						{
							if (!V.IsValid())
							{
								continue;
							}

							// Strings are treated as active subscriptions (back-compat with older payloads).
							if (V->Type == EJson::String)
							{
								bNewHasActiveSubscription = true;
								Owned.Add(V->AsString());
								continue;
							}

							// Objects: look for non-trial active entries, but be permissive if we can't inspect.
							if (V->Type == EJson::Object)
							{
								const TSharedPtr<FJsonObject> SubObj = V->AsObject();
								if (!SubObj.IsValid())
								{
									continue;
								}

								bool bIsTrial = false;
								(void)(SubObj->TryGetBoolField(TEXT("isTrial"), bIsTrial)
									|| SubObj->TryGetBoolField(TEXT("trial"), bIsTrial));

								FString Status;
								SubObj->TryGetStringField(TEXT("status"), Status);
								const bool bStatusActive =
									Status.IsEmpty()
									|| Status.Equals(TEXT("active"), ESearchCase::IgnoreCase)
									|| Status.Equals(TEXT("current"), ESearchCase::IgnoreCase);

								if (bStatusActive && !bIsTrial)
								{
									bNewHasActiveSubscription = true;
								}

								// Also try to collect any app/asset identifiers included in the subscription payload.
								FString AppId;
								if (SubObj->TryGetStringField(TEXT("appId"), AppId)
									|| SubObj->TryGetStringField(TEXT("assetId"), AppId)
									|| SubObj->TryGetStringField(TEXT("id"), AppId))
								{
									if (!AppId.IsEmpty())
									{
										Owned.Add(AppId);
									}
								}
								continue;
							}

							// Unknown entry type but non-empty array: assume a subscription exists.
							bNewHasActiveSubscription = true;
						}
					}
				}
			}
		}

		if (JsonObject->HasField(TEXT("purchased")))
		{
			const TArray<TSharedPtr<FJsonValue>>& Purchased = JsonObject->GetArrayField(TEXT("purchased"));
			UE_LOG(LogWorldBLDAssetDownloadManager, Log, TEXT("[RefreshOwnershipStatus] Found %d purchased items"), Purchased.Num());

			for (const TSharedPtr<FJsonValue>& V : Purchased)
			{
				if (!V.IsValid() || V->Type != EJson::Object) continue;
				const TSharedPtr<FJsonObject> Obj = V->AsObject();
				if (!Obj.IsValid()) continue;
				// Use assetId instead of patchKitAppId
				if (Obj->HasField(TEXT("assetId")))
				{
					FString AssetId = Obj->GetStringField(TEXT("assetId"));
					UE_LOG(LogWorldBLDAssetDownloadManager, Log, TEXT("[RefreshOwnershipStatus] Adding owned asset: %s"), *AssetId);
					Owned.Add(AssetId);
				}
			}
		}

		UE_LOG(LogWorldBLDAssetDownloadManager, Log, TEXT("[RefreshOwnershipStatus] Total owned assets: %d"), Owned.Num());

		TArray<TPair<FString, EWorldBLDAssetStatus>> Changes;
		bool bShouldBroadcastSubscription = false;
		bool bBroadcastKnown = false;
		bool bBroadcastActive = false;
		{
			FScopeLock Lock(&StateMutex);
			const bool bPrevKnown = bHasActiveSubscriptionKnown;
			const bool bPrevActive = bHasActiveSubscription;

			bHasActiveSubscription = bNewHasActiveSubscription;
			// If the server succeeded but didn't include any subscription signal, we still treat it as "known"
			// (conservatively false) so UI doesn't stay stuck in the "unknown" state forever.
			bHasActiveSubscriptionKnown = true;

			bShouldBroadcastSubscription =
				(bPrevKnown != bHasActiveSubscriptionKnown) || (bPrevActive != bHasActiveSubscription);
			bBroadcastKnown = bHasActiveSubscriptionKnown;
			bBroadcastActive = bHasActiveSubscription;
			for (auto& Pair : AssetStates)
			{
				FWorldBLDAssetDownloadState& State = Pair.Value;
				const bool bOwned = Owned.Contains(State.AssetId);

				if (State.Status == EWorldBLDAssetStatus::Downloaded || State.Status == EWorldBLDAssetStatus::Imported)
				{
					continue;
				}

				const EWorldBLDAssetStatus NewStatus = bOwned ? EWorldBLDAssetStatus::Owned : EWorldBLDAssetStatus::NotOwned;
				if (State.Status != NewStatus)
				{
					State.Status = NewStatus;
					State.LastModified = FDateTime::UtcNow();
					Changes.Add({ State.AssetId, NewStatus });
				}
			}

			for (const FString& PatchKitId : Owned)
			{
				FWorldBLDAssetDownloadState& State = AssetStates.FindOrAdd(PatchKitId);
				State.AssetId = PatchKitId;
				if (State.Status == EWorldBLDAssetStatus::NotOwned)
				{
					State.Status = EWorldBLDAssetStatus::Owned;
					State.LastModified = FDateTime::UtcNow();
					Changes.Add({ PatchKitId, EWorldBLDAssetStatus::Owned });
				}
			}
		}

		for (const TPair<FString, EWorldBLDAssetStatus>& Change : Changes)
		{
			OnAssetStatusChanged.Broadcast(Change.Key, Change.Value);
		}

		if (bShouldBroadcastSubscription)
		{
			OnSubscriptionStatusChanged.Broadcast(bBroadcastKnown, bBroadcastActive);
		}

		SaveManifest();
	});

	Request->ProcessRequest();
}

bool UWorldBLDAssetDownloadManager::HasActiveSubscription() const
{
	FScopeLock Lock(&StateMutex);
	return bHasActiveSubscriptionKnown && bHasActiveSubscription;
}

void UWorldBLDAssetDownloadManager::BuildLocalAssetCache(TMap<FString, TPair<FString, FString>>& OutCache) const
{
	if (DependencyService.IsValid())
	{
		DependencyService->BuildLocalAssetCache(OutCache);
	}
	else
	{
		OutCache.Reset();
	}
}

bool UWorldBLDAssetDownloadManager::FindAssetByUniqueID(const FString& UniqueID, FString& OutPackagePath, FString& OutSHA1Hash) const
{
	return DependencyService.IsValid() ? DependencyService->FindAssetByUniqueID(UniqueID, OutPackagePath, OutSHA1Hash) : false;
}

void UWorldBLDAssetDownloadManager::CheckLocalDependencies(const FWorldBLDAssetInfo& AssetInfo,
	TArray<FWorldBLDAssetDependency>& OutMissingDeps,
	TArray<FWorldBLDAssetDependency>& OutExistingDeps)
{
	OutMissingDeps.Empty();
	OutExistingDeps.Empty();

	if (AssetInfo.Manifest.Dependencies.Num() == 0)
	{
		UE_LOG(LogWorldBLDAssetDownloadManager, Warning, TEXT("[CheckLocalDependencies] Asset '%s' has no dependencies in manifest"), *AssetInfo.Name);
		return;
	}

	UE_LOG(LogWorldBLDAssetDownloadManager, Log, TEXT("[CheckLocalDependencies] Checking %d dependencies for asset '%s'"),
		AssetInfo.Manifest.Dependencies.Num(), *AssetInfo.Name);

	// Build local asset cache
	TMap<FString, TPair<FString, FString>> LocalCache;
	BuildLocalAssetCache(LocalCache);

	// Check each dependency
	for (const FWorldBLDAssetDependency& Dependency : AssetInfo.Manifest.Dependencies)
	{
		if (const TPair<FString, FString>* Found = LocalCache.Find(Dependency.UniqueID))
		{
			// Asset exists locally
			FWorldBLDAssetDependency ExistingDep = Dependency;
			ExistingDep.PackagePath = Found->Key; // Update with actual local path

			// Check SHA1 match
			if (Found->Value != Dependency.SHA1Hash && !Found->Value.IsEmpty() && !Dependency.SHA1Hash.IsEmpty())
			{
				UE_LOG(LogWorldBLDAssetDownloadManager, Warning,
					TEXT("[CheckLocalDependencies] SHA1 mismatch for '%s': Local='%s', Remote='%s'"),
					*Dependency.AssetName, *Found->Value, *Dependency.SHA1Hash);
			}

			OutExistingDeps.Add(ExistingDep);
		}
		else
		{
			// Asset missing locally
			UE_LOG(LogWorldBLDAssetDownloadManager, VeryVerbose,
				TEXT("[CheckLocalDependencies] Missing dependency: '%s' (ID: %s)"),
				*Dependency.AssetName, *Dependency.UniqueID);

			OutMissingDeps.Add(Dependency);
		}
	}

	UE_LOG(LogWorldBLDAssetDownloadManager, Log,
		TEXT("[CheckLocalDependencies] Result: %d existing, %d missing dependencies"),
		OutExistingDeps.Num(), OutMissingDeps.Num());
}

void UWorldBLDAssetDownloadManager::DownloadAssetWithDependencies(const FWorldBLDAssetInfo& AssetInfo,
	bool bCheckLocalFirst,
	FDOnDownloadProgress OnProgress,
	FDOnComplete OnComplete)
{
	UE_LOG(LogWorldBLDAssetDownloadManager, Log, TEXT("[DownloadAssetWithDependencies] Starting download for '%s'"), *AssetInfo.Name);

	if (AssetInfo.DownloadURL.IsEmpty())
	{
		UE_LOG(LogWorldBLDAssetDownloadManager, Error, TEXT("[DownloadAssetWithDependencies] No download URL for asset '%s'"), *AssetInfo.Name);
		OnComplete.ExecuteIfBound(false);
		return;
	}

	// No dependencies - just download the asset directly
	if (AssetInfo.Manifest.Dependencies.Num() == 0)
	{
		UE_LOG(LogWorldBLDAssetDownloadManager, Log, TEXT("[DownloadAssetWithDependencies] No dependencies, downloading asset directly"));
		DownloadAsset(AssetInfo.ID, AssetInfo.Name, AssetInfo.DownloadURL, false, OnProgress, OnComplete);
		return;
	}

	// Check local dependencies if requested
	if (!bCheckLocalFirst)
	{
		UE_LOG(LogWorldBLDAssetDownloadManager, Log, TEXT("[DownloadAssetWithDependencies] Skipping dependency check, downloading main asset"));
		DownloadAsset(AssetInfo.ID, AssetInfo.Name, AssetInfo.DownloadURL, false, OnProgress, OnComplete);
		return;
	}

	// Check which dependencies exist locally
	TArray<FWorldBLDAssetDependency> MissingDeps;
	TArray<FWorldBLDAssetDependency> ExistingDeps;
	CheckLocalDependencies(AssetInfo, MissingDeps, ExistingDeps);

	UE_LOG(LogWorldBLDAssetDownloadManager, Log,
		TEXT("[DownloadAssetWithDependencies] Dependency check: %d existing, %d missing"),
		ExistingDeps.Num(), MissingDeps.Num());

	// Check for SHA1 mismatches in existing dependencies
	TArray<FWorldBLDAssetDependency> MismatchedDeps;
	for (const FWorldBLDAssetDependency& Dep : ExistingDeps)
	{
		// Find the expected SHA1 from manifest
		const FWorldBLDAssetDependency* ManifestDep = AssetInfo.Manifest.Dependencies.FindByPredicate(
			[&Dep](const FWorldBLDAssetDependency& Other) { return Other.UniqueID == Dep.UniqueID; });

		if (ManifestDep && !ManifestDep->SHA1Hash.IsEmpty() && !Dep.SHA1Hash.IsEmpty())
		{
			if (ManifestDep->SHA1Hash != Dep.SHA1Hash)
			{
				FWorldBLDAssetDependency MismatchDep = *ManifestDep;
				MismatchDep.PackagePath = Dep.PackagePath; // Keep local path for display
				MismatchedDeps.Add(MismatchDep);
			}
		}
	}

	// Log SHA1 mismatches (dialog will be shown from UI layer)
	if (MismatchedDeps.Num() > 0)
	{
		UE_LOG(LogWorldBLDAssetDownloadManager, Warning,
			TEXT("[DownloadAssetWithDependencies] Found %d dependencies with SHA1 mismatches. ")
			TEXT("User should be prompted to replace or keep local versions."),
			MismatchedDeps.Num());
	}

	// Log missing dependencies warning
	if (MissingDeps.Num() > 0)
	{
		UE_LOG(LogWorldBLDAssetDownloadManager, Warning,
			TEXT("[DownloadAssetWithDependencies] Asset '%s' has %d missing dependencies. ")
			TEXT("Import may fail if these dependencies are required at runtime."),
			*AssetInfo.Name, MissingDeps.Num());
	}

	// Download the main asset
	UE_LOG(LogWorldBLDAssetDownloadManager, Log, TEXT("[DownloadAssetWithDependencies] Downloading main asset: %s"), *AssetInfo.Name);
	DownloadAsset(AssetInfo.ID, AssetInfo.Name, AssetInfo.DownloadURL, false, OnProgress, OnComplete);
}

void UWorldBLDAssetDownloadManager::InvalidateLocalAssetCache()
{
	if (DependencyService.IsValid())
	{
		DependencyService->Invalidate();
	}
}
