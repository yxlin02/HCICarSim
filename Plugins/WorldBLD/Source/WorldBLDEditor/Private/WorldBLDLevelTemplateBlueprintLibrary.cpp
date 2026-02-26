#include "WorldBLDLevelTemplateBlueprintLibrary.h"

#include "GameFramework/Actor.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/Level.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "ScopedTransaction.h"

#include "LevelTemplate/WorldBLDLevelTemplateWindow.h"
#endif

void UWorldBLDLevelTemplateBlueprintLibrary::OpenLevelTemplateWindow()
{
#if WITH_EDITOR
	FWorldBLDLevelTemplateWindow::OpenLevelTemplateWindow();
#endif
}

void UWorldBLDLevelTemplateBlueprintLibrary::LoadLightingPresetFromSoftWorld(UWorld* TargetWorld, TSoftObjectPtr<UWorld> PresetWorld)
{
#if WITH_EDITOR
	if (!IsValid(TargetWorld) || PresetWorld.IsNull())
	{
		return;
	}

	// Pin the loaded preset world for the duration of application to prevent GC during actor iteration.
	const TStrongObjectPtr<UWorld> LoadedPresetWorld(PresetWorld.LoadSynchronous());
	if (!LoadedPresetWorld.IsValid())
	{
		return;
	}

	UWorldBLDLevelTemplateBlueprintLibrary::LoadLightingPreset(TargetWorld, LoadedPresetWorld.Get());
#endif
}

void UWorldBLDLevelTemplateBlueprintLibrary::LoadLightingPreset(UWorld* TargetWorld, UWorld* PresetWorld)
{
#if WITH_EDITOR
	if (!IsValid(TargetWorld) || !IsValid(PresetWorld))
	{
		return;
	}
	if (!IsValid(TargetWorld->PersistentLevel) || !IsValid(PresetWorld->PersistentLevel))
	{
		return;
	}

	TUniquePtr<FScopedTransaction> Transaction;
	if (GEditor)
	{
		Transaction = MakeUnique<FScopedTransaction>(NSLOCTEXT("WorldBLD", "LoadLightingPreset", "Load Lighting Preset"));
	}

	TargetWorld->Modify();
	if (!IsValid(TargetWorld->PersistentLevel))
	{
		return;
	}
	TargetWorld->PersistentLevel->Modify();

	TSet<UClass*> PresetActorClasses;
	for (TActorIterator<AActor> It(PresetWorld); It; ++It)
	{
		AActor* PresetActor = *It;
		if (!IsValid(PresetActor))
		{
			continue;
		}
		if (PresetActor->IsA<AWorldSettings>())
		{
			continue;
		}
		if (PresetActor->GetClass() == nullptr)
		{
			continue;
		}
		PresetActorClasses.Add(PresetActor->GetClass());
	}

	static const FName LightingPresetFolderPath(TEXT("LightingPreset"));
	TArray<AActor*> ActorsToDestroy;
	for (TActorIterator<AActor> It(TargetWorld); It; ++It)
	{
		AActor* TargetActor = *It;
		if (!IsValid(TargetActor) || TargetActor->IsA<AWorldSettings>())
		{
			continue;
		}

		if (TargetActor->GetFolderPath() == LightingPresetFolderPath)
		{
			ActorsToDestroy.Add(TargetActor);
			continue;
		}

		if (PresetActorClasses.Contains(TargetActor->GetClass()))
		{
			ActorsToDestroy.Add(TargetActor);
			continue;
		}
	}

	for (AActor* ActorToDestroy : ActorsToDestroy)
	{
		if (!IsValid(ActorToDestroy))
		{
			continue;
		}
		ActorToDestroy->Modify();
		ActorToDestroy->Destroy();
	}

	TArray<AActor*> ActorsToDuplicate;
	for (TActorIterator<AActor> It(PresetWorld); It; ++It)
	{
		AActor* PresetActor = *It;
		if (!IsValid(PresetActor))
		{
			continue;
		}
		if (PresetActor->IsA<AWorldSettings>())
		{
			continue;
		}
		ActorsToDuplicate.Add(PresetActor);
	}

	TMap<AActor*, AActor*> PresetToTargetActorMap;
	PresetToTargetActorMap.Reserve(ActorsToDuplicate.Num());

	for (AActor* PresetActor : ActorsToDuplicate)
	{
		if (!IsValid(PresetActor))
		{
			continue;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.OverrideLevel = TargetWorld->PersistentLevel;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.bDeferConstruction = true;

		AActor* NewActor = TargetWorld->SpawnActor<AActor>(PresetActor->GetClass(), PresetActor->GetActorTransform(), SpawnParams);
		if (!IsValid(NewActor))
		{
			continue;
		}

		NewActor->Modify();
		UEditorEngine::CopyPropertiesForUnrelatedObjects(PresetActor, NewActor);
		NewActor->FinishSpawning(PresetActor->GetActorTransform());
		NewActor->SetFolderPath(LightingPresetFolderPath);
		PresetToTargetActorMap.Add(PresetActor, NewActor);
	}

	for (const TPair<AActor*, AActor*>& Pair : PresetToTargetActorMap)
	{
		AActor* PresetActor = Pair.Key;
		AActor* NewActor = Pair.Value;
		if (!IsValid(PresetActor) || !IsValid(NewActor))
		{
			continue;
		}

		AActor* PresetParent = PresetActor->GetAttachParentActor();
		AActor* const* NewParentPtr = PresetToTargetActorMap.Find(PresetParent);
		if (NewParentPtr && IsValid(*NewParentPtr))
		{
			NewActor->AttachToActor(*NewParentPtr, FAttachmentTransformRules::KeepWorldTransform);
		}
	}
#endif
}
