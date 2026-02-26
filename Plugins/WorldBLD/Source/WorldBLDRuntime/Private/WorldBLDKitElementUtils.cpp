// Copyright WorldBLD LLC. All rights reserved.

#include "WorldBLDKitElementUtils.h"
#include "IWorldBLDKitElementInterface.h"
#include "WorldBLDRuntimeUtilsLibrary.h"
#include "WorldBLDKitElementComponent.h"
#include "WorldBLDElementSnapPointComponent.h"
#include "WorldBLDRuntimeSettings.h"

#include "UObject/Package.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "GameFramework/Volume.h"
#include "Framework/Application/SlateApplication.h"

#if WITH_EDITOR
# include "Editor.h"
# include "Editor/GroupActor.h"
# include "UnrealEdGlobals.h"
# include "Editor/UnrealEdEngine.h"
# include "Subsystems/EditorActorSubsystem.h"
# include "Subsystems/UnrealEditorSubsystem.h"
# include "Subsystems/AssetEditorSubsystem.h"
# include "Engine/Selection.h"
# include "LevelEditor.h"
# include "LevelEditorViewport.h"
# include "SLevelViewport.h"
# include "MouseDeltaTracker.h"
# include "Elements/Framework/EngineElementsLibrary.h"
# include "Layers/LayersSubsystem.h"
# include "Misc/SlowTask.h"
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////

DEFINE_LOG_CATEGORY(LogWorldBLDKitTiming);

UE_DEFINE_GAMEPLAY_TAG(Tag_WorldBLD_Detail_Edge,					TEXT("WorldBLD.Detail.Edge"));
UE_DEFINE_GAMEPLAY_TAG(Tag_WorldBLD_Detail_Edge_Centerline,		TEXT("WorldBLD.Detail.Edge.Centerline"));
UE_DEFINE_GAMEPLAY_TAG(Tag_WorldBLD_Detail_Edge_Border,			TEXT("WorldBLD.Detail.Edge.Border"));
UE_DEFINE_GAMEPLAY_TAG(Tag_WorldBLD_Detail_Edge_Border_Left,		TEXT("WorldBLD.Detail.Edge.Border.Left"));
UE_DEFINE_GAMEPLAY_TAG(Tag_WorldBLD_Detail_Edge_Border_Right,	TEXT("WorldBLD.Detail.Edge.Border.Right"));
UE_DEFINE_GAMEPLAY_TAG(Tag_WorldBLD_Detail_Point,				TEXT("WorldBLD.Detail.Point"));

UE_DEFINE_GAMEPLAY_TAG(Tag_WorldBLD_Detail_Enter,				TEXT("WorldBLD.Detail.Enter"));
UE_DEFINE_GAMEPLAY_TAG(Tag_WorldBLD_Detail_Exit,					TEXT("WorldBLD.Detail.Exit"));
UE_DEFINE_GAMEPLAY_TAG(Tag_WorldBLD_Detail_Junction,				TEXT("WorldBLD.Detail.Junction"));

///////////////////////////////////////////////////////////////////////////////////////////////////

DECLARE_CYCLE_STAT(TEXT("WorldBLDKitElement Subsystem Tick"), STAT_WorldBLDKitElementSubsystem_TickTime, STATGROUP_Game);

/* @TODO - Some ideas:
     o  Track all WorldBLDKitElement actors and what they are used for
	 o  Have a debug overlay we can render showing all of the different kinds
	    OR we can just a overview of what is in a given level
*/

bool UWorldBLDKitElementSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// Supported for every world type
	return true;	
}

void UWorldBLDKitElementSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
#if WITH_EDITOR
	if ((GetWorld()->WorldType == EWorldType::Editor) && GEngine)
	{
		GEngine->OnLevelActorAdded().AddUObject(this, &UWorldBLDKitElementSubsystem::OnActorSpawned);
		GEngine->OnLevelActorDeleted().AddUObject(this, &UWorldBLDKitElementSubsystem::OnActorDeleted);
		GEngine->OnLevelActorListChanged().AddUObject(this, &UWorldBLDKitElementSubsystem::OnLevelActorListChanged);
		if (GEditor)
		{
			GEditor->RegisterForUndo(this);
		}
	}
#endif
	// In some cases EndPlay is not guaranteed to be called when level is removed.
	GetWorld()->OnLevelsChanged().AddUObject(this, &UWorldBLDKitElementSubsystem::OnLevelsChanged);
}

UWorldBLDKitElementSubsystem* UWorldBLDKitElementSubsystem::GetActiveWorldBLDKitElementSubsystem()
{
	UWorldBLDKitElementSubsystem* Subsystem {nullptr};
	if (GEngine)
	{
		if (UWorld* World = GEngine->GetCurrentPlayWorld())
		{
			Subsystem = World->GetSubsystem<UWorldBLDKitElementSubsystem>();
		}
	}
#if WITH_EDITOR
	if ((Subsystem == nullptr) && GEditor)
	{
		UUnrealEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>();
		UWorld* EditorWorld = EditorSubsystem->GetEditorWorld();
		Subsystem = EditorWorld->GetSubsystem<UWorldBLDKitElementSubsystem>();
	}
#endif
	return Subsystem;
}

UWorldBLDKitElementSubsystem* UWorldBLDKitElementSubsystem::Get(UObject* WorldContextObject)
{
	UWorldBLDKitElementSubsystem* Subsystem {nullptr};
	if (WorldContextObject)
	{
		if (UWorld* World = WorldContextObject->GetWorld())
		{
			Subsystem = World->GetSubsystem<UWorldBLDKitElementSubsystem>();
		}
	}
	return Subsystem;
}

ETickableTickType UWorldBLDKitElementSubsystem::GetTickableTickType() const
{
	return IsTemplate() ? ETickableTickType::Never : ETickableTickType::Always;
}

TStatId UWorldBLDKitElementSubsystem::GetStatId() const
{
	return GET_STATID(STAT_WorldBLDKitElementSubsystem_TickTime);
}

void UWorldBLDKitElementSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if (bPendingFullRebuild)
	{
		bPendingFullRebuild = false;
		RebuildActorLists();
	}
}

void UWorldBLDKitElementSubsystem::RebuildActorLists()
{
	TraceIgnoreList.Empty();
	TraceIgnoreList.Add(AVolume::StaticClass());
	if (auto RuntimeSettings = UWorldBLDRuntimeSettings::Get())
	{
		for (TSoftClassPtr<AActor> ActorClassRef : RuntimeSettings->TraceIgnoredActorClasses)
		{
			TSubclassOf<AActor> ActorClass = ActorClassRef.LoadSynchronous();
			if (IsValid(ActorClass))
			{
				TraceIgnoreList.AddUnique(ActorClass);
			}
		}
	}

	TArray<AActor*> ActorList;
#if WITH_EDITOR
	if (GEditor)
	{
		UUnrealEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>();
		UWorld* EditorWorld = EditorSubsystem->GetEditorWorld();
		if (!IsValid(EditorWorld))
		{
			return;
		}

		if (UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>())
		{
			ActorList = EditorActorSubsystem->GetAllLevelActors();
		}
	}
#else
	/// @TODO: Handle this in a normal play world?
#endif
	
	ToolIgnoredActors.Empty();
	for (AActor* Actor : ActorList)
	{
		CheckAddIgnorableActor(Actor);
	}
}

void UWorldBLDKitElementSubsystem::Deinitialize()
{
#if WITH_EDITOR
	if ((GetWorld()->WorldType == EWorldType::Editor) && GEngine)
	{
		GEngine->OnLevelActorAdded().RemoveAll(this);
		GEngine->OnLevelActorDeleted().RemoveAll(this);
		GEngine->OnLevelActorListChanged().RemoveAll(this);
		if (GEditor)
		{
			GEditor->UnregisterForUndo(this);
		}
	}
#endif
	GetWorld()->OnLevelsChanged().RemoveAll(this);

	Super::Deinitialize();
}

void UWorldBLDKitElementSubsystem::OnActorSpawned(AActor* InActor)
{
	CheckAddIgnorableActor(InActor);
}

void UWorldBLDKitElementSubsystem::OnActorDeleted(AActor* InActor)
{
	CheckRemoveIgnorableActor(InActor);
}

void UWorldBLDKitElementSubsystem::OnLevelsChanged()
{
	bPendingFullRebuild = true;
}

#if WITH_EDITOR

void UWorldBLDKitElementSubsystem::OnLevelActorListChanged()
{
	bPendingFullRebuild = true;
}

void UWorldBLDKitElementSubsystem::PostUndo(bool bSuccess)
{
	bPendingFullRebuild = true;
}

void UWorldBLDKitElementSubsystem::PostRedo(bool bSuccess)
{
	bPendingFullRebuild = true;
}

#endif // WITH_EDITOR

void UWorldBLDKitElementSubsystem::BeginTimingBlock(const FString& Context, UObject* ContextObject)
{
	UWorldBLDKitElementSubsystem* Subsystem = UWorldBLDKitElementSubsystem::GetActiveWorldBLDKitElementSubsystem();
	if (IsValid(Subsystem))
	{
		FElementTimingEntry& Entry = Subsystem->TimingStack.AddDefaulted_GetRef();
		Entry.Context = Context;
		Entry.ContextObject = ContextObject;
		Entry.CapturedTime = FPlatformTime::Seconds();
		Entry.CapturedDuration = 0.0f;

		FString Indent = FString::ChrN((Subsystem->TimingStack.Num() - 1) * 2, (TCHAR)' ');
		UE_LOG(LogWorldBLDKitTiming, Verbose, TEXT("%s%s - BEGIN (%s)"), *Indent, *Context, 
				*UKismetSystemLibrary::GetDisplayName(ContextObject));
	}
}

void UWorldBLDKitElementSubsystem::ContinueTimingBlock(const FString& NewContext, UObject* ContextObject)
{
	UWorldBLDKitElementSubsystem* Subsystem = UWorldBLDKitElementSubsystem::GetActiveWorldBLDKitElementSubsystem();
	if (IsValid(Subsystem))
	{
		check(Subsystem->TimingStack.Num() > 0);
		if (Subsystem->TimingStack.Num() > 0)
		{
			double Now = FPlatformTime::Seconds();
			FElementTimingEntry& Entry = Subsystem->TimingStack.Last();
			Entry.CapturedDuration = Now - Entry.CapturedTime;
			FString Indent = FString::ChrN((Subsystem->TimingStack.Num() - 1) * 2, (TCHAR)' ');
			UE_LOG(LogWorldBLDKitTiming, Verbose, TEXT("%s%s - END (%s) - %.3fs"), *Indent, *Entry.Context, 
					*UKismetSystemLibrary::GetDisplayName(Entry.ContextObject), (float)Entry.CapturedDuration);
			
			Entry.Context = NewContext;
			Entry.CapturedTime = Now;
			Entry.CapturedDuration = 0.0f;
			if (IsValid(ContextObject))
			{
				Entry.ContextObject = ContextObject;
			}

			UE_LOG(LogWorldBLDKitTiming, Verbose, TEXT("%s%s - BEGIN (%s)"), *Indent, *Entry.Context, 
					*UKismetSystemLibrary::GetDisplayName(Entry.ContextObject));
		}
	}
}

void UWorldBLDKitElementSubsystem::EndTimingBlock()
{
	UWorldBLDKitElementSubsystem* Subsystem = UWorldBLDKitElementSubsystem::GetActiveWorldBLDKitElementSubsystem();
	if (IsValid(Subsystem))
	{
		check(Subsystem->TimingStack.Num() > 0);
		if (Subsystem->TimingStack.Num() > 0)
		{
			FElementTimingEntry& Entry = Subsystem->TimingStack.Last();
			Entry.CapturedDuration = FPlatformTime::Seconds() - Entry.CapturedTime;
			FString Indent = FString::ChrN((Subsystem->TimingStack.Num() - 1) * 2, (TCHAR)' ');
			UE_LOG(LogWorldBLDKitTiming, Verbose, TEXT("%s%s - END (%s) - %.3fs"), *Indent, *Entry.Context, 
					*UKismetSystemLibrary::GetDisplayName(Entry.ContextObject), (float)Entry.CapturedDuration);
			Subsystem->TimingStack.Pop();
		}
	}
}

TSharedPtr<struct FSlowTask> UWorldBLDKitElementSubsystem::ActiveSlowTask;

void UWorldBLDKitElementSubsystem::BeginSlowTask(FText TaskTitle, int32 UnitsOfWork)
{
#if WITH_EDITOR
	if (ActiveSlowTask != nullptr)
	{
		ActiveSlowTask->Destroy();
		ActiveSlowTask = nullptr;
	}
	ActiveSlowTask = MakeShared<FSlowTask>(UnitsOfWork, TaskTitle, true, *GWarn);
	ActiveSlowTask->Initialize();
	ActiveSlowTask->MakeDialog(true);
#endif
}

void UWorldBLDKitElementSubsystem::UpdateSlowTask(int32 WorkCompleted)
{
#if WITH_EDITOR
	if (ActiveSlowTask != nullptr)
	{
		ActiveSlowTask->EnterProgressFrame(WorkCompleted);
	}
#endif
}

bool UWorldBLDKitElementSubsystem::UserRequestedCancelSlowTask()
{
#if WITH_EDITOR
	return GWarn->ReceivedUserCancel();
#else
	return false;
#endif
}

void UWorldBLDKitElementSubsystem::EndSlowTask()
{
#if WITH_EDITOR
	if (ActiveSlowTask != nullptr)
	{
		ActiveSlowTask->Destroy();
		ActiveSlowTask = nullptr;
	}
#endif
}

void UWorldBLDKitElementSubsystem::GetAllToolIgnorableActors(TArray<AActor*>& Array)
{
	for (TWeakObjectPtr<AActor> IgnoredActor : ToolIgnoredActors)
	{
		if (IgnoredActor.IsValid())
		{
			Array.AddUnique(IgnoredActor.Get());
		}
	}
}

void UWorldBLDKitElementSubsystem::CheckAddIgnorableActor(AActor* Actor)
{
	if (IsValid(Actor))
	{
		for (auto ActorClass : TraceIgnoreList)
		{
			if (Actor->IsA(ActorClass))
			{
				ToolIgnoredActors.AddUnique(Actor);
				break;
			}
		}
	}
}

void UWorldBLDKitElementSubsystem::CheckRemoveIgnorableActor(AActor* Actor)
{
	ToolIgnoredActors.Remove(Actor);
}

///////////////////////////////////////////////////////////////////////////////////////////////////


TArray<AActor*> UWorldBLDKitElementUtils::WorldBLDKitElement_GetSubChildrenHelper(TArray<AActor*> ChildrenToExpand)
{
	TArray<AActor*> ExpandedChildren;

	for (AActor* Child : ChildrenToExpand)
	{
		if (IsValid(Child))
		{
			ExpandedChildren.AddUnique(Child);
			if (UObject* Implementor = FindWorldBLDKitElementImplementor(Child))
			{
				ExpandedChildren.Append(IWorldBLDKitElementInterface::Execute_WorldBLDKitElement_Children(Implementor, true));
			}
		}
	}

	return ExpandedChildren;
}

TArray<AActor*> UWorldBLDKitElementUtils::TryGetExpandedWorldBLDKitElementChildren(AActor* Element)
{
	TArray<AActor*> Result;
	if (IsValid(Element))
	{
		Result.Add(Element);
		if (UObject* Implementor = FindWorldBLDKitElementImplementor(Element))
		{
			Result.Append(IWorldBLDKitElementInterface::Execute_WorldBLDKitElement_Children(Implementor, /* Sub-Children? */ true));
		}
	}
	return Result;
}

FWorldBLDKitElementProperties UWorldBLDKitElementUtils::TryGetWorldBLDKitElementProperties(AActor* Element)
{
	FWorldBLDKitElementProperties Props;
	Props.bValidElement = false;
	if (UObject* Implementor = FindWorldBLDKitElementImplementor(Element))
	{
		Props = IWorldBLDKitElementInterface::Execute_WorldBLDKitElement_Properties(Implementor);
	}
	return Props;
}

UObject* UWorldBLDKitElementUtils::FindWorldBLDKitElementImplementor(UObject* Source)
{
	UObject* Implementor = nullptr;
	if (AActor* Actor = Cast<AActor>(Source))
	{
		if (UKismetSystemLibrary::DoesImplementInterface(Actor, UWorldBLDKitElementInterface::StaticClass()))
		{
			Implementor = Actor;
		}
		else if (UWorldBLDKitElementComponent* Comp = Actor->FindComponentByClass<UWorldBLDKitElementComponent>())
		{
			Implementor = Comp;
		}
	}
	return Implementor;
}

void UWorldBLDKitElementUtils::WorldBLDKitElement_DemolishAndDestroy(AActor* Actor)
{
	if (UObject* Implementor = FindWorldBLDKitElementImplementor(Actor))
	{
		IWorldBLDKitElementInterface::Execute_WorldBLDKitElement_Demolish(Implementor);
	}

	if (IsValid(Actor))
	{
		SafeDeleteActor(Actor);
	}
}

void UWorldBLDKitElementUtils::WorldBLDKitElement_DemolishChildren(AActor* Actor)
{
	TArray<AActor*> Children = TryGetExpandedWorldBLDKitElementChildren(Actor);
	Children.Remove(Actor);
	for (AActor* Child : Children)
	{
		if (IsValid(Child))
		{
			WorldBLDKitElement_DemolishAndDestroy(Child);
		}
	}
}

void UWorldBLDKitElementUtils::WorldBLDKitElement_UpdateRebuild_Helper(AActor* Actor, bool bRerunConstructionScript)
{
	if (UObject* Implementor = FindWorldBLDKitElementImplementor(Actor))
	{
		IWorldBLDKitElementInterface::Execute_WorldBLDKitElement_UpdateRebuild(Implementor, bRerunConstructionScript);
	}
}

bool FElementFilterSpec::ConditionsMet(const FGameplayTagContainer& Tags) const
{
	bool bValid = RequiredTags.IsEmpty() || (Tags.Filter(RequiredTags).Num() == RequiredTags.Num());
	if (bValid)
	{
		bValid = IgnoredTags.IsEmpty() || (IgnoredTags.Filter(Tags).Num() == 0);
	}
	if (bValid)
	{
		bValid = RequestedTags.IsEmpty() || (Tags.Filter(RequestedTags).Num() > 0);
	}

	return bValid;
}

void UWorldBLDKitElementUtils::WorldBLDKitElement_GetEdges(AActor* Actor, FElementFilterSpec Filter, TArray<FWorldBLDKitElementEdge>& OutEdges)
{
	if (UObject* Implementor = FindWorldBLDKitElementImplementor(Actor))
	{
		IWorldBLDKitElementInterface::Execute_WorldBLDKitElement_RetrieveEdges(Implementor, OutEdges);
	}

	for (int32 Idx = 0; Idx < OutEdges.Num(); Idx += 1)
	{
		if (!Filter.ConditionsMet(OutEdges[Idx].Tags))
		{
			OutEdges.RemoveAtSwap(Idx);
			Idx -= 1;
		}
	}
}

void UWorldBLDKitElementUtils::WorldBLDKitElement_GetPoints(AActor* Actor, FElementFilterSpec Filter, TArray<FWorldBLDKitElementPoint>& OutPoints)
{
	TGuardValue<bool> AutoRestore(GAllowActorScriptExecutionInEditor, true);

	OutPoints.Empty();

	if (UObject* Implementor = FindWorldBLDKitElementImplementor(Actor))
	{
		IWorldBLDKitElementInterface::Execute_WorldBLDKitElement_RetrievePoints(Implementor, OutPoints);
	}

	WorldBLDKitElement_GatherPlacedSnapPoints(Actor, OutPoints, /* Empty First? */ false);

	for (int32 Idx = 0; Idx < OutPoints.Num(); Idx += 1)
	{
		if (!Filter.ConditionsMet(OutPoints[Idx].Tags))
		{
			OutPoints.RemoveAtSwap(Idx);
			Idx -= 1;
		}
	}
}

void UWorldBLDKitElementUtils::CreateElementPointTransformFromSplinePoint(
	class USplineComponent* Spline, 
	int32 SplinePoint, 
	ESplineCoordinateSpace::Type CoordinateSpace, 
	FTransform& Transform, 
	float& TangentMagnitude)
{
	FTransform Result = FTransform::Identity;
	float Tangent = 1.0f;
	if (IsValid(Spline) && (Spline->GetNumberOfSplinePoints() > 0))
	{
		int32 Idx = SplinePoint % Spline->GetNumberOfSplinePoints();
		if (Idx < Spline->GetNumberOfSplinePoints())
		{
			Result = FTransform(
				Spline->GetRotationAtSplinePoint(SplinePoint, CoordinateSpace), 
				Spline->GetLocationAtSplinePoint(SplinePoint, CoordinateSpace),
				Spline->GetScaleAtSplinePoint(SplinePoint));
			Tangent = Spline->GetTangentAtSplinePoint(SplinePoint, CoordinateSpace).Length();
		}
	}
	Transform = Result;
	TangentMagnitude = Tangent;
}

bool UWorldBLDKitElementUtils::GetElementPointWorldBasis(
	const FWorldBLDKitElementPoint& Point, 
	float DefaultTangent, 
	FVector& OutLocation, 
	FVector& OutForward, 
	FVector& OutRight, 
	FVector& OutTangent)
{
	bool bPointIsValid = IsElementPointValid(Point);

	OutLocation = FVector::ZeroVector;
	OutForward = FVector::ForwardVector;
	OutRight = FVector::RightVector;

	if (bPointIsValid)
	{
		const FTransform& OwnerXform = Point.Owner->GetActorTransform();
		const FTransform& PointXform = Point.LocalTransform;

		OutLocation = OwnerXform.TransformPosition(PointXform.TransformPosition(OutLocation));
		OutForward = OwnerXform.TransformVector(PointXform.TransformVector(OutForward)).GetSafeNormal();
		OutRight = OwnerXform.TransformVector(PointXform.TransformVector(OutRight)).GetSafeNormal();

		float TangentLen = FMath::IsNearlyZero(Point.TangentMagnitude) ? (FMath::IsNearlyZero(DefaultTangent) ? 1.0f : DefaultTangent) : Point.TangentMagnitude;
		OutTangent = OutForward * TangentLen;
	}
	return bPointIsValid;
}

bool UWorldBLDKitElementUtils::GetElementPointWorldTransform(const FWorldBLDKitElementPoint& Point, FTransform& OutTransform)
{
	bool bPointIsValid = IsElementPointValid(Point);
	
	if (bPointIsValid)
	{
		const FTransform& OwnerXform = Point.Owner->GetActorTransform();
		const FTransform& PointXform = Point.LocalTransform;

		OutTransform = OwnerXform * PointXform;
	}
	else
	{
		OutTransform = FTransform::Identity;
	}

	return bPointIsValid;
}

bool UWorldBLDKitElementUtils::IsElementPointValid(const FWorldBLDKitElementPoint& Point)
{
	return (Point.Owner != nullptr) && !Point.Id.IsNone();
}

void UWorldBLDKitElementUtils::WorldBLDKitElement_GatherPlacedSnapPoints(AActor* Actor, TArray<FWorldBLDKitElementPoint>& OutPoints, bool bEmptyFirst)
{
	if (bEmptyFirst)
	{
		OutPoints.Empty();
	}

	if (!IsValid(Actor))
	{
		return;
	}
	
	TArray<UWorldBLDElementSnapPointComponent*> SnapPoints;
	Actor->GetComponents<UWorldBLDElementSnapPointComponent>(SnapPoints);
	for (UWorldBLDElementSnapPointComponent* SnapPoint : SnapPoints)
	{
		if (ensure(IsValid(SnapPoint)))
		{
			OutPoints.AddUnique(SnapPoint->GetPoint());
		}
	}
}

void UWorldBLDKitElementUtils::MarkObjectPackageDirty(UObject* Object)
{
	if (IsValid(Object))
	{
		(void)Object->MarkPackageDirty();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

AActor* UWorldBLDKitElementUtils::SpawnActorFromClassInMainWorld(
	TSubclassOf<class AActor> ActorClass,
	FVector Location,
	FRotator Rotation,
	bool bTransient,
	bool bTemporaryEditorActor, 
	bool bHideFromSceneOutliner,
	bool bIncludeInTransactionBuffer)
{
	if (UWorld* PlayWorld = GEngine->GetCurrentPlayWorld())
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.bAllowDuringConstructionScript = true;
		SpawnParams.bNoFail = true;
#if WITH_EDITOR
		SpawnParams.bTemporaryEditorActor = bTemporaryEditorActor;
		SpawnParams.bHideFromSceneOutliner = bHideFromSceneOutliner;
#endif
		if (bTransient)
		{
			SpawnParams.ObjectFlags |= RF_Transient;
		}
		if (!bIncludeInTransactionBuffer)
		{
			SpawnParams.ObjectFlags &= ~RF_Transactional;
		}
		return PlayWorld->SpawnActor<AActor>(ActorClass, Location, Rotation, SpawnParams);
	}
#if WITH_EDITOR
	{
		// NOTE: Expanded version of UEditorActorSubsystem::SpawnActorFromClass()
		TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);
		UUnrealEditorSubsystem* UnrealEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>() : nullptr;
		UObject* ObjToUse = ActorClass.Get();
		const TCHAR* MessageName = TEXT("SpawnActorFromClass");

		if (!UnrealEditorSubsystem)
		{
			return nullptr;
		}

		if (!ObjToUse)
		{
			UE_LOG(LogWorldBLD, Error, TEXT("%s. ObjToUse is not valid."), MessageName);
			return nullptr;
		}

		UWorld* World = UnrealEditorSubsystem->GetEditorWorld();
		if (!World)
		{
			UE_LOG(LogWorldBLD, Error, TEXT("%s. Can't spawn the actor because there is no world."), MessageName);
			return nullptr;
		}

		ULevel* DesiredLevel = World->GetCurrentLevel();
		if (!DesiredLevel)
		{
			UE_LOG(LogWorldBLD, Error, TEXT("%s. Can't spawn the actor because there is no Level."), MessageName);
			return nullptr;
		}

		GEditor->ClickLocation = Location;
		GEditor->ClickPlane = FPlane(Location, FVector::UpVector);

		EObjectFlags NewObjectFlags = RF_NoFlags;
		
		if (bIncludeInTransactionBuffer)
		{
			NewObjectFlags |= RF_Transactional;
		}

		if (bTransient)
		{
			NewObjectFlags |= RF_Transient;
		}

		UActorFactory* FactoryToUse = nullptr;
		// @FIXME: Getting "bHideFromSceneOutliner" to work is not really exposed at this level. FSetActorHiddenInSceneOutliner is not
		//         available to call for us by default form the engine, and TryPlacingActorFromObject doesn't let us change the spawn parameters.
		//         We might be able to get this working if we sub-class UActorFactory and provide that to TryPlacingActorFromObject().
		TArray<AActor*> Actors = FLevelEditorViewportClient::TryPlacingActorFromObject(DesiredLevel, ObjToUse, 
				/* Select Actors? */ false, NewObjectFlags, FactoryToUse);

		if (Actors.Num() == 0 || Actors[0] == nullptr)
		{
			UE_LOG(LogWorldBLD, Warning, TEXT("%s. No actor was spawned."), MessageName);
			return nullptr;
		}

		for (AActor* Actor : Actors)
		{
			if (Actor)
			{
				Actor->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
				Actor->bIsEditorPreviewActor = bTemporaryEditorActor;
			}
		}

		return Actors[0];
	}
#else
	return nullptr;	
#endif
}

void UWorldBLDKitElementUtils::TryForceRunConstructionScipts(TArray<AActor*> Actors)
{
#if WITH_EDITOR
	for (AActor* Actor : Actors)
	{
		if (IsValid(Actor))
		{
			Actor->UnregisterAllComponents();
			Actor->RerunConstructionScripts();
			Actor->RegisterAllComponents();
		}
	}
#endif
}

void UWorldBLDKitElementUtils::RunConstructionScipt(AActor* Actor)
{
	TryForceRunConstructionScipts({Actor});
}

void UWorldBLDKitElementUtils::RunConstructionScipts(TArray<AActor*> Actors)
{
	TryForceRunConstructionScipts(Actors);
}

void UWorldBLDKitElementUtils::SafeDeleteActor(AActor* Actor)
{
	if (IsValid(Actor))
	{
		if (UWorld* PlayWorld = GEngine->GetCurrentPlayWorld())
		{
			Actor->Destroy();
		}
#if WITH_EDITOR
		else if (GUnrealEd && !GIsPlayInEditorWorld && GEditor)
		{
#if 1
			// SEE: GUnrealEd->DeleteActors() ... the following is a simplified version of that function
			//      The main issue is that DeleteActors() performs garbage collection at each deletion which
			//      is very expensive.
			
			UTypedElementSelectionSet* InSelectionSet = GUnrealEd->GetSelectedActors()->GetElementSelectionSet();

			// Fire ULevel::LevelDirtiedEvent when falling out of scope.
			FScopedLevelDirtied LevelDirtyCallback;
			
			// If the actor about to be deleted is in a group, be sure to remove it from the group
			AGroupActor* ActorParentGroup = Cast<AGroupActor>(Actor->GroupActor);
			if (ActorParentGroup)
			{
				ActorParentGroup->Remove(*Actor);
			}

			// Remove actor from all asset editors
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->RemoveAssetFromAllEditors(Actor);

			// Mark the actor's level as dirty.
			(void)Actor->MarkPackageDirty();
			LevelDirtyCallback.Request();

			const FTypedElementSelectionOptions SelectionOptions = FTypedElementSelectionOptions()
				.SetAllowHidden(true)
				.SetAllowGroups(false)
				.SetWarnIfLocked(false)
				.SetChildElementInclusionMethod(ETypedElementChildInclusionMethod::Recursive);

			ULayersSubsystem* LayersSubsystem = GEditor->GetEditorSubsystem<ULayersSubsystem>();

			// Deselect the Actor.
			if (InSelectionSet)
			{
				if (FTypedElementHandle ActorHandle = UEngineElementsLibrary::AcquireEditorActorElementHandle(Actor, /*bAllowCreate*/false))
				{
					InSelectionSet->DeselectElement(ActorHandle, SelectionOptions);
				}
			}

			// Destroy actor and clear references.
			LayersSubsystem->DisassociateActorFromLayers(Actor);
			bool WasDestroyed = Actor->GetWorld()->EditorDestroyActor(Actor, false);
			checkf(WasDestroyed, TEXT("Failed to destroy Actor %s (%s)"), *Actor->GetClass()->GetName(), *Actor->GetActorLabel());
#else
			UWorld* WorldToApplyTo = Actor->GetWorld();
			if (UUnrealEditorSubsystem* UnrealEditorSubsystem = GEditor->GetEditorSubsystem<UUnrealEditorSubsystem>())
			{
				WorldToApplyTo = UnrealEditorSubsystem->GetEditorWorld();
			}
			GUnrealEd->DeleteActors({Actor}, WorldToApplyTo, GUnrealEd->GetSelectedActors()->GetElementSelectionSet(),
					/* Verify? */ true, /* Warn about references ? */ false, /* Warn about references ? */ false);
#endif
		}
#endif
	}
}

bool UWorldBLDKitElementUtils::UserIsDraggingGizmoInViewport()
{
	bool bDragging = false;
#if WITH_EDITOR
	FLevelEditorModule* LevelEditorModule = static_cast<FLevelEditorModule*>(FModuleManager::Get().GetModule("LevelEditor"));
	if (LevelEditorModule != nullptr)
	{
		if (TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule->GetFirstLevelEditor())
		{
			for (TSharedPtr<SLevelViewport> LevelViewport : LevelEditor->GetViewports())
			{
				if (LevelViewport.IsValid())
				{
					if (TSharedPtr<FEditorViewportClient> Viewport = LevelViewport->GetViewportClient())
					{
						if (FMouseDeltaTracker* DeltaTracker = Viewport->GetMouseDeltaTracker())
						{
							auto WidgetMode = DeltaTracker->GetTrackingWidgetMode();
							if (((int32)WidgetMode > (int32)UE::Widget::EWidgetMode::WM_None) && ((int32)WidgetMode < (int32)UE::Widget::EWidgetMode::WM_Max))
							{
								bDragging = bDragging || !DeltaTracker->GetRawDelta().IsNearlyZero();
							}
						}
					}
				}
			}
		}
	}
#endif
	return bDragging && FSlateApplication::Get().GetPressedMouseButtons().Contains(EKeys::LeftMouseButton);
}

int32 UWorldBLDKitElementUtils::GetBlueprintMaximumLoopIterationCount()
{
	return IsValid(GEngine) ? GEngine->MaximumLoopIterationCount : -1;
}

UObject* UWorldBLDKitElementUtils::GetMutableClassConfig(UClass* ConfigClass)
{
	UObject* Object {nullptr};
	if (IsValid(ConfigClass))
	{
		Object = ConfigClass->GetDefaultObject();
		if (IsValid(Object))
		{
			Object->LoadConfig();
		}
	}
	return Object;
}

void UWorldBLDKitElementUtils::SaveConfigObject(UObject* ConfigObject, bool bClearDirtyFlag)
{
	if (IsValid(ConfigObject) && ConfigObject->HasAnyFlags(RF_ClassDefaultObject))
	{
		ConfigObject->SaveConfig();
		if (bClearDirtyFlag)
		{
			ConfigObject->GetPackage()->ClearDirtyFlag();
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void FWorldBLDKitElementEdge::AddPoint(const FVector& Location, const FQuat& Rotation, const FVector& Scale, bool bLinear)
{
	UWorldBLDRuntimeUtilsLibrary::SplineCurves_AddPoint(SplineCurve, Location, Rotation, Scale, bLinear);
}

bool FWorldBLDKitElementEdge::operator==(const FWorldBLDKitElementEdge& RHS) const
{
	return (Owner == RHS.Owner) && (Id == RHS.Id);
}

bool FWorldBLDKitElementEdge::operator==(const struct FWorldBLDKitElementEdgeRef& RHS) const
{
	return (Owner == RHS.Owner) && (Id == RHS.Id);
}

bool FWorldBLDKitElementEdgeRef::IsValid() const
{
	return ::IsValid(Owner) && !Id.IsNone();
}

FWorldBLDKitElementEdge* FWorldBLDKitElementEdgeRef::Get() const
{
	FWorldBLDKitElementEdge* TheEdge = nullptr;
	
	if (IsValid())
	{
		if (UWorldBLDKitElementComponent* Comp = Owner->FindComponentByClass<UWorldBLDKitElementComponent>())
		{
			if (!Comp->bUseOwningActorTraits)
			{
				for (auto& Edge : Comp->ElementEdges)
				{
					if (Edge.Id == Id)
					{
						TheEdge = &Edge;
						break;
					}
				}
			}
		}

		if (TheEdge == nullptr)
		{
			/// @TODO: Error: not supported pathway
		}
	}

	return TheEdge;
}

bool FWorldBLDKitElementEdgeRef::operator==(const FWorldBLDKitElementEdge& RHS) const
{
	return (Owner == RHS.Owner) && (Id == RHS.Id);
}

bool FWorldBLDKitElementEdgeRef::operator==(const FWorldBLDKitElementEdgeRef& RHS) const
{
	return (Owner == RHS.Owner) && (Id == RHS.Id);
}

bool FWorldBLDKitElementPoint::operator==(const FWorldBLDKitElementPoint& RHS) const
{
	return (Owner == RHS.Owner) && (Id == RHS.Id);
}

FWorldBLDKitElementEdgeRef UWorldBLDKitElementUtils::MakeEdgeReference(const struct FWorldBLDKitElementEdge& InEdge)
{
	return FWorldBLDKitElementEdgeRef(InEdge);
}

void UWorldBLDKitElementUtils::ResolveEdgeReference(FWorldBLDKitElementEdgeRef Reference, bool& bValid, struct FWorldBLDKitElementEdge& OutEdge)
{
	bValid = false;
	if (FWorldBLDKitElementEdge* EdgeRef = Reference.Get())
	{
		bValid = true;
		OutEdge = *EdgeRef;
	}

	if (!bValid)
	{
		OutEdge = FWorldBLDKitElementEdge();
	}
}

void UWorldBLDKitElementUtils::AddPointToElementEdge(struct FWorldBLDKitElementEdge& InEdge, const FVector& Location, const FQuat& Rotation, const FVector& Scale, bool bLinear)
{
	UWorldBLDRuntimeUtilsLibrary::SplineCurves_AddPoint(InEdge.SplineCurve, Location, Rotation, Scale, bLinear);
}

void UWorldBLDKitElementUtils::AssignElementEdgeToSpline(const struct FWorldBLDKitElementEdge& Edge, class USplineComponent* Spline)
{
	if (IsValid(Spline))
	{
		if (IsValid(Edge.Spline))
		{
			Spline->SplineCurves = Edge.Spline->SplineCurves;
			Spline->SetComponentToWorld(Edge.Spline->GetComponentToWorld());
		}
		else
		{
			Spline->SplineCurves = Edge.SplineCurve;
			Spline->SetComponentToWorld(Edge.SplineCurveToWorldTransform);
		}
		Spline->UpdateSpline();
	}
}

FVector UWorldBLDKitElementUtils::GetLocationAtInputKeyAlongElementEdge(const struct FWorldBLDKitElementEdge& Edge, double IKValue)
{
	FSplineCurves SplineCurves = Edge.SplineCurve;
	// To get position at a specific input parameter (0.0 to 1.0)
	FVector Location = SplineCurves.Position.Eval(IKValue, FVector::ZeroVector);

	return Location;
}

TArray<FVector> UWorldBLDKitElementUtils::SampleElementEdge(const struct FWorldBLDKitElementEdge& Edge, int32 SampleCount, int32 SamplePrecision)
{
	// Get evenly spaced points along the entire curve by distance
	const int32 NumPoints = SampleCount;
	TArray<FVector> Points;
	Points.Reserve(NumPoints);
	FSplineCurves SplineCurves = Edge.SplineCurve;

	// Get the actual min and max input keys from the curve
	float MinInputKey = SplineCurves.Position.Points.Num() > 0 ? 
	    SplineCurves.Position.Points[0].InVal : 0.0f;
	float MaxInputKey = SplineCurves.Position.Points.Num() > 0 ? 
	    SplineCurves.Position.Points[SplineCurves.Position.Points.Num() - 1].InVal : 1.0f;

	// First, we need to approximate the total length of the curve
	const int32 LengthSamples = SamplePrecision; // More samples = more accurate length approximation
	float TotalLength = 0.0f;
	FVector PrevPos = SplineCurves.Position.Eval(MinInputKey, FVector::ZeroVector);

	TArray<float> DistanceTable;
	TArray<float> KeyTable;
	DistanceTable.Reserve(LengthSamples + 1);
	KeyTable.Reserve(LengthSamples + 1);
	DistanceTable.Add(0.0f);
	KeyTable.Add(MinInputKey);

	// Build a distance table
	for (int32 i = 1; i <= LengthSamples; i++)
	{
	    float Alpha = static_cast<float>(i) / static_cast<float>(LengthSamples);
	    float CurrentKey = FMath::Lerp(MinInputKey, MaxInputKey, Alpha);
	    FVector CurrentPos = SplineCurves.Position.Eval(CurrentKey, FVector::ZeroVector);
	    float SegmentLength = (CurrentPos - PrevPos).Size();
	    TotalLength += SegmentLength;
	    DistanceTable.Add(TotalLength);
	    KeyTable.Add(CurrentKey);
	    PrevPos = CurrentPos;
	}

	// Now sample points at equal distances
	for (int32 i = 0; i < NumPoints; i++)
	{
	    float TargetDistance = (static_cast<float>(i) / static_cast<float>(NumPoints - 1)) * TotalLength;
	    
	    // Find the right section in our distance table
	    int32 LowerIndex = 0;
	    for (int32 j = 0; j < DistanceTable.Num() - 1; j++)
	    {
	        if (DistanceTable[j] <= TargetDistance && TargetDistance <= DistanceTable[j + 1])
	        {
	            LowerIndex = j;
	            break;
	        }
	    }
	    
	    // Interpolate between the two closest distance table entries
	    float LowerDistance = DistanceTable[LowerIndex];
	    float UpperDistance = DistanceTable[LowerIndex + 1];
	    float Ratio = 0.0f;
	    if (UpperDistance > LowerDistance)
	    {
	        Ratio = (TargetDistance - LowerDistance) / (UpperDistance - LowerDistance);
	    }
	    
	    float LowerKey = KeyTable[LowerIndex];
	    float UpperKey = KeyTable[LowerIndex + 1];
	    float InterpolatedKey = FMath::Lerp(LowerKey, UpperKey, Ratio);
	    
	    // Get the position at this distance
	    FVector Position = SplineCurves.Position.Eval(InterpolatedKey, FVector::ZeroVector);
	    Points.Add(Position);
	}

	return Points;
}

TArray<FTransform> UWorldBLDKitElementUtils::SampleElementEdgeToTransforms(const struct FWorldBLDKitElementEdge& Edge,
	int32 SampleCount, int32 SamplePrecision)
{
	const int32 NumPoints = SampleCount;
	FSplineCurves SplineCurves = Edge.SplineCurve;
	 TArray<FTransform> Transforms;
    Transforms.Reserve(NumPoints);

    // Get the actual min and max input keys from the curve
    float MinInputKey = SplineCurves.Position.Points.Num() > 0 ? 
        SplineCurves.Position.Points[0].InVal : 0.0f;
    float MaxInputKey = SplineCurves.Position.Points.Num() > 0 ? 
        SplineCurves.Position.Points[SplineCurves.Position.Points.Num() - 1].InVal : 1.0f;

    // First, we need to approximate the total length of the curve
    const int32 LengthSamples = SamplePrecision; // More samples = more accurate length approximation
    float TotalLength = 0.0f;
    FVector PrevPos = SplineCurves.Position.Eval(MinInputKey, FVector::ZeroVector);

    TArray<float> DistanceTable;
    TArray<float> KeyTable;
    DistanceTable.Reserve(LengthSamples + 1);
    KeyTable.Reserve(LengthSamples + 1);
    DistanceTable.Add(0.0f);
    KeyTable.Add(MinInputKey);

    // Build a distance table
    for (int32 i = 1; i <= LengthSamples; i++)
    {
        float Alpha = static_cast<float>(i) / static_cast<float>(LengthSamples);
        float CurrentKey = FMath::Lerp(MinInputKey, MaxInputKey, Alpha);
        FVector CurrentPos = SplineCurves.Position.Eval(CurrentKey, FVector::ZeroVector);
        float SegmentLength = (CurrentPos - PrevPos).Size();
        TotalLength += SegmentLength;
        DistanceTable.Add(TotalLength);
        KeyTable.Add(CurrentKey);
        PrevPos = CurrentPos;
    }

    // Now sample transforms at equal distances
    for (int32 i = 0; i < NumPoints; i++)
    {
        float TargetDistance = (static_cast<float>(i) / static_cast<float>(NumPoints - 1)) * TotalLength;
        
        // Find the right section in our distance table
        int32 LowerIndex = 0;
        for (int32 j = 0; j < DistanceTable.Num() - 1; j++)
        {
            if (DistanceTable[j] <= TargetDistance && TargetDistance <= DistanceTable[j + 1])
            {
                LowerIndex = j;
                break;
            }
        }
        
        // Interpolate between the two closest distance table entries
        float LowerDistance = DistanceTable[LowerIndex];
        float UpperDistance = DistanceTable[LowerIndex + 1];
        float Ratio = 0.0f;
        if (UpperDistance > LowerDistance)
        {
            Ratio = (TargetDistance - LowerDistance) / (UpperDistance - LowerDistance);
        }
        
        float LowerKey = KeyTable[LowerIndex];
        float UpperKey = KeyTable[LowerIndex + 1];
        float InterpolatedKey = FMath::Lerp(LowerKey, UpperKey, Ratio);
        
        // Get the position, rotation, and scale at this point
        FVector Position = SplineCurves.Position.Eval(InterpolatedKey, FVector::ZeroVector);
        
        // Get the tangent and create a rotation from it
        FVector Tangent = SplineCurves.Position.EvalDerivative(InterpolatedKey, FVector::ZeroVector);
        FQuat Rotation;
        
        // If we have a rotation curve, use it
        if (SplineCurves.Rotation.Points.Num() > 0)
        {
            Rotation = SplineCurves.Rotation.Eval(InterpolatedKey, FQuat::Identity);
        }
        else
        {
            // Otherwise, build rotation from tangent
            // Normalize the tangent to get the forward direction
            if (!Tangent.IsNearlyZero())
            {
                Tangent.Normalize();
                // Create a rotation that points along the tangent
                Rotation = FQuat::FindBetweenVectors(FVector::ForwardVector, Tangent);
            }
            else
            {
                Rotation = FQuat::Identity;
            }
        }
        
        // Get the scale at this point
        FVector Scale = SplineCurves.Scale.Eval(InterpolatedKey, FVector::OneVector);
        
        // Create the transform
        FTransform Transform(Rotation, Position, Scale);
        Transforms.Add(Transform);
    }
    
    return Transforms;
}


bool UWorldBLDKitElementUtils::FindMatchingEdge(const TArray<struct FWorldBLDKitElementEdge>& Edges, AActor* Actor, FName Id, struct FWorldBLDKitElementEdge& OutEdge)
{
	bool bFound = false;

	OutEdge = FWorldBLDKitElementEdge();
	for (const auto& Edge : Edges)
	{
		if ((Edge.Owner == Actor) && ((Id == NAME_None) || (Edge.Id == Id)))
		{
			OutEdge = Edge;
			bFound = true;
			break;
		}
	}

	return bFound;
}

FVector UWorldBLDKitElementUtils::FindMidpointOfEdge(const struct FWorldBLDKitElementEdge& Edge)
{
	TArray<FTransform> EdgeSamples = SampleElementEdgeToTransforms(Edge, 15);

	int32 Length = EdgeSamples.Num();
	
	FVector Midpoint = EdgeSamples[Length/2].GetLocation();
	
	return Midpoint;
}

bool UWorldBLDKitElementUtils::FindMatchingEdgeFromReference(
	const TArray<struct FWorldBLDKitElementEdge>& Edges, 
	FWorldBLDKitElementEdgeRef Reference, 
	struct FWorldBLDKitElementEdge& OutEdge)
 {
	bool bFound = false;

	OutEdge = FWorldBLDKitElementEdge();
	for (const auto& Edge : Edges)
	{
		if ((Edge.Owner == Reference.Owner) && ((Reference.Id == NAME_None) || (Edge.Id == Reference.Id)))
		{
			OutEdge = Edge;
			bFound = true;
			break;
		}
	}

	return bFound;
 }

 bool UWorldBLDKitElementUtils::FindMatchingEdgeReference(
	const TArray<struct FWorldBLDKitElementEdgeRef>& References, 
	FWorldBLDKitElementEdgeRef Reference,
	int32& OutIndex)
 {
	bool bFound = false;
	OutIndex = INDEX_NONE;

	for (int32 Idx = 0; Idx < References.Num(); Idx += 1)
	{
		const FWorldBLDKitElementEdgeRef& Ref = References[Idx];
		if (Ref == Reference)
		{
			bFound = true;
			OutIndex = Idx;
			break;
		}
	}
	return bFound;
 }

void UWorldBLDKitElementUtils::AutoExpandEdgeLoop(
	const UObject* WorldContext, 
	FAutoExpandEdgeLoopParams Params, 
	const TArray<FWorldBLDKitElementEdge>& InEdges, 
	TArray<FWorldBLDKitElementEdge>& OutFoundEdges)
{
	OutFoundEdges.Empty();
	if (!IsValid(WorldContext) || (InEdges.Num() == 0) || (Params.SphereTraceRadius <= 0.0f))
	{
		return;
	}

	const TArray<TEnumAsByte<EObjectTypeQuery> > ObjectTypes { 
		UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic),
		UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldDynamic),
	};

	TArray<FWorldBLDKitElementEdge> EdgesToTrace = InEdges;
	const float TraceLenSq = Params.SphereTraceRadius * Params.SphereTraceRadius;

	while (EdgesToTrace.Num() > 0)
	{
		FWorldBLDKitElementEdge CurrentEdge = EdgesToTrace[0];
		EdgesToTrace.RemoveAtSwap(0);

		if (CurrentEdge.SplineCurve.Position.Points.Num() == 0)
		{
			continue;
		}
		TArray<FVector, TFixedAllocator<2>> TestPoints = {
			CurrentEdge.SplineCurveToWorldTransform.TransformPosition(CurrentEdge.SplineCurve.Position.Points[0].OutVal),
			CurrentEdge.SplineCurveToWorldTransform.TransformPosition(CurrentEdge.SplineCurve.Position.Points.Last().OutVal),
		};

		for (const FVector& TestPoint : TestPoints)
		{
			TArray<AActor*> ActorsToIgnore;
			TArray<FHitResult> OutHits;
			UKismetSystemLibrary::SphereTraceMultiForObjects(WorldContext, TestPoint, TestPoint, Params.SphereTraceRadius, ObjectTypes, /* Trace Complex? */ false,
					ActorsToIgnore, EDrawDebugTrace::None, OutHits, /* Ignore Self? */ true);

			for (const FHitResult& Hit : OutHits)
			{
				TArray<FWorldBLDKitElementEdge> ActorEdges;
				UWorldBLDKitElementUtils::WorldBLDKitElement_GetEdges(Hit.GetActor(), Params.EdgeFilter, ActorEdges);
				for (const FWorldBLDKitElementEdge& ActorEdge : ActorEdges)
				{
					if (ActorEdge.SplineCurve.Position.Points.Num() == 0)
					{
						continue;
					}
					FVector Start = ActorEdge.SplineCurveToWorldTransform.TransformPosition(ActorEdge.SplineCurve.Position.Points[0].OutVal);
					FVector End = ActorEdge.SplineCurveToWorldTransform.TransformPosition(ActorEdge.SplineCurve.Position.Points.Last().OutVal);

					if (((Start - TestPoint).SquaredLength() <= TraceLenSq) || ((End - TestPoint).SquaredLength() <= TraceLenSq))
					{
						FWorldBLDKitElementEdge DummyEdge;
						if (!UWorldBLDKitElementUtils::FindMatchingEdge(InEdges, ActorEdge.Owner, ActorEdge.Id, DummyEdge) && 
								!UWorldBLDKitElementUtils::FindMatchingEdge(OutFoundEdges, ActorEdge.Owner, ActorEdge.Id, DummyEdge) &&
								!UWorldBLDKitElementUtils::FindMatchingEdge(EdgesToTrace, ActorEdge.Owner, ActorEdge.Id, DummyEdge))
						{
							OutFoundEdges.Add(ActorEdge);
							EdgesToTrace.Add(ActorEdge);
						}
					}
				}
			}
		}
	}
}

void UWorldBLDKitElementUtils::CalculateEdgeAlongPolylinePath(
	const TArray<FTransform>& LocalSweepPath, 
	UCurveFloat* SweepShape, 
	float Scale, 
	bool bAlongMax, 
	FWorldBLDKitElementEdge& OutEdge,
	bool bFlatten,
	const FCleanupSplineParams* OverrideCleanupParams,
	const FSelfIntersectionParams* OverrideSelfIntersectionParams,
	bool bIsClosedLoop)
{
	OutEdge.SplineCurve = FSplineCurves();

	float SampleTime = 0.0f;
	float SampleHeight = 0.0f;

	if (IsValid(SweepShape))
	{
		double MinTime, MaxTime;
		SweepShape->GetTimeRange(MinTime, MaxTime);
		SampleTime = bAlongMax ? MaxTime : MinTime;
		SampleHeight = SweepShape->FloatCurve.Eval(SampleTime);
		if (bFlatten)
		{
			SampleHeight = 0.0f;
		}
	}
	
#if 0
	FTransform SweepStart {FTransform::Identity}, SweepEnd {FTransform::Identity};
	if (LocalSweepPath.Num() > 0)
	{
		SweepStart = LocalSweepPath[0];
		SweepEnd = LocalSweepPath.Last();
	}
	FPlane StartCutPlane = FPlane(SweepStart.GetLocation(), SweepStart.GetUnitAxis(EAxis::X));
    FPlane EndCutPlane = FPlane(SweepEnd.GetLocation(), SweepEnd.GetUnitAxis(EAxis::X));
#endif

	const FVector LocalPoint = Scale * (FVector::RightVector * SampleTime + FVector::UpVector * SampleHeight);
	TOptional<FVector> LastPoint;
	TOptional<FVector> LastCenter;
	for (int32 Idx = 0; Idx < LocalSweepPath.Num(); Idx += 1)
	{
		const FTransform& SweepXform = LocalSweepPath[Idx];
		FVector CenterPoint = SweepXform.GetTranslation();
		FVector Point = SweepXform.TransformPosition(LocalPoint);
		bool bAddThisPoint = true;
		if (LastPoint.IsSet())
		{
			bool bNearStart = (FVector::Dist(CenterPoint, LocalSweepPath[0].GetLocation()) < 100.0f);
			bool bNearEnd = (FVector::Dist(CenterPoint, LocalSweepPath.Last().GetLocation()) < 100.0f);

			// This isn't perfect, but addresses some issues with the spline points going weird at the ends of a spline as we sample.
			if (LastCenter.IsSet() && 
					((Idx != 0) && (Idx < LocalSweepPath.Num() - 1)) &&
					(bNearStart || bNearEnd))
			{
				FVector Travel = (Point - *LastPoint).GetUnsafeNormal();
				FVector CenterTravel = (CenterPoint - *LastCenter).GetUnsafeNormal();
				bAddThisPoint = (FVector::DotProduct(CenterTravel, Travel) > 0.0f);
#if 0
				if (bNearStart)
				{
					bAddThisPoint = bAddThisPoint && (StartCutPlane.PlaneDot(Point) >= 0.0f);
				}
				if (bNearEnd)
				{
					bAddThisPoint = bAddThisPoint && (EndCutPlane.PlaneDot(Point) <= 0.0f);
				}
#endif
			}
		}

		LastCenter = CenterPoint;

		if (bAddThisPoint)
		{
			LastPoint = Point;
			OutEdge.AddPoint(Point, SweepXform.GetRotation());
		}
	}

	if (OutEdge.SplineCurve.Position.Points.Num() > 2)
	{
		// It's possible the swept edge is self-intersecting. Try and fix that up.
		FCleanupSplineParams CleanupParams;
		if (OverrideCleanupParams)
		{
			CleanupParams = *OverrideCleanupParams;
		}
		else
		{
			CleanupParams.bRemoveRedundantLinearPoints = true;
			CleanupParams.LinearPointDeflectionThreshold = 0.8f;
		}

		FSelfIntersectionParams FixupParams;
		if (OverrideSelfIntersectionParams)
		{
			FixupParams = *OverrideSelfIntersectionParams;
		}
		else
		{
			FixupParams.ZHeightThreshold = 300.0f;
		}

		FSplineSpitShineCleanupParameters SpitShineParams;
		SpitShineParams.bAutoCloseLoop = true;
		SpitShineParams.CleanupParams = CleanupParams;
		SpitShineParams.CleanupParams.bCollapseNearbyPoints = true;
		SpitShineParams.CleanupParams.CollapseRadius = 8.0f;
		SpitShineParams.IntersectionParams = FixupParams;
		SpitShineParams.bAutoCloseLoop = false;
		UWorldBLDRuntimeUtilsLibrary::SpitShineMySplineCurves(OutEdge.SplineCurve, SpitShineParams);
	}
}

TArray<FTransform> UWorldBLDKitElementUtils::ConvertVectorsToTransforms(const TArray<FVector>& Locations)
{
	{
		// Create an output array to store our transforms
		TArray<FTransform> Transforms;

		// Reserve space in our output array to avoid reallocations
		Transforms.Reserve(Locations.Num());

		// Create our default rotation and scale
		const FRotator DefaultRotation = FRotator::ZeroRotator;
		const FVector DefaultScale = FVector(1.0f, 1.0f, 1.0f);

		// Iterate through each location and create a transform
		for (const FVector& Location : Locations)
		{
			// Create a new transform with our default values
			FTransform NewTransform(
				DefaultRotation,  // Rotation set to (0,0,0)
				Location,        // Use the input location
				DefaultScale     // Scale set to (1,1,1)
			);

			// Add the transform to our output array
			Transforms.Add(NewTransform);
		}

		return Transforms;
	}
}

bool UWorldBLDKitElementUtils::FindMatchingPoint(const TArray<struct FWorldBLDKitElementPoint>& Points, AActor* Actor, FName Id, struct FWorldBLDKitElementPoint& OutPoint)
{
	bool bFound = false;

	OutPoint = FWorldBLDKitElementPoint();
	for (const auto& Point : Points)
	{
		if ((Point.Owner == Actor) && ((Id == NAME_None) || (Point.Id == Id)))
		{
			OutPoint = Point;
			bFound = true;
			break;
		}
	}

	return bFound;
}

void UWorldBLDKitElementUtils::AssignActorsToCityFolderPath(TArray<AActor*> Actors)
{
#if WITH_EDITOR
	/// @TODO: Make this configurable by the user?
	for (AActor* Actor : Actors)
	{
		if (IsValid(Actor) && (Actor->GetFolderPath() == NAME_None))
		{
			FString Path = TEXT("City");
			FWorldBLDKitElementProperties Props = TryGetWorldBLDKitElementProperties(Actor);
			if (Props.bValidElement)
			{
				if (Props.bIsRoad || Props.bIsRoadProp)
				{
					Path += TEXT("/Roads");
				}
				else if (Props.bIsBuilding)
				{
					Path += TEXT("/Structures");
				}
				else if (Props.bIsBoundary)
				{
					Path += TEXT("/Lots");
				}
				else if (Props.bIsPlaceholder)
				{
					Path += TEXT("/Preview");
				}
			}
			if (Actor->IsA<AWorldBLDKitBase>())
			{
				Path += TEXT("/Kits");
			}
			Actor->SetFolderPath(FName(*Path));
		}
	}
#endif
}

void UWorldBLDKitElementUtils::FilterEdges(TArray<FWorldBLDKitElementEdge>& Edges, FElementFilterSpec Filter)
{
	for (int i = Edges.Num() - 1; i >= 0; i--)
	{
		if (!Filter.ConditionsMet(Edges[i].Tags))
		{			
			Edges.RemoveAt(i);
		}
	}
}

void UWorldBLDKitElementUtils::FilterPoints(TArray<FWorldBLDKitElementPoint>& Points, FElementFilterSpec Filter)
{
	for (int i = Points.Num() - 1; i >= 0; i--)
	{
		if (!Filter.ConditionsMet(Points[i].Tags))
		{
			Points.RemoveAt(i);
		}
	}
}

FVector UWorldBLDKitElementUtils::FindElementEdgeClosestPoint(const FVector& WorldLocation, const FWorldBLDKitElementEdge& Edge, float& DistanceSq, float& InputKey)
{
	FVector ClosestPoint = FVector::ZeroVector;
	if (IsValid(Edge.Spline))
	{		
		InputKey = Edge.Spline->FindInputKeyClosestToWorldLocation(WorldLocation);
		ClosestPoint = Edge.Spline->GetLocationAtSplineInputKey(InputKey, ESplineCoordinateSpace::World);
	}
	else
	{
		if (IsValid(Edge.Owner))
		{
			FTransform ActorTransform = Edge.Owner->GetActorTransform();
			const FVector LocalLocation = ActorTransform.InverseTransformPosition(WorldLocation);						
			InputKey = Edge.SplineCurve.Position.FindNearest(LocalLocation, DistanceSq);			
			ClosestPoint = Edge.SplineCurve.Position.Eval(InputKey, FVector::ZeroVector);
			ClosestPoint = ActorTransform.TransformPosition(ClosestPoint);
		}
	}
	return ClosestPoint;
}

int32 UWorldBLDKitElementUtils::FindClosestEdge(
	const FVector& Location, 
	UPARAM(ref)TArray<FWorldBLDKitElementEdge>& Edges, 	
	FVector& EdgePoint,	
	float& IK)
{		
	float MinDist = TNumericLimits<float>::Max();
	int Idx = -1;
	for (int i = 0; i < Edges.Num(); i++)
	{
		FWorldBLDKitElementEdge& Edge = Edges[i];
		float Dist = 0.0;
		float CurrIK = 0.0;
		FVector Point= FindElementEdgeClosestPoint(Location, Edge, Dist, CurrIK);
		if (Dist < MinDist)
		{
			Idx = i;
			EdgePoint = Point;
			IK = CurrIK;
			MinDist = Dist;
		}
	}
	return Idx;
}

void UWorldBLDKitElementUtils::FilterEdgesByDistanceToLocation(
	const FVector& Location, 
	UPARAM(ref)TArray<FWorldBLDKitElementEdge>& Edges, 
	float MinDistance, 
	TArray<FVector>& OutPoints,
	TArray<float>& OutDistances,
	bool bSort)
{
	OutDistances.Reserve(Edges.Num());
	OutPoints.Reserve(Edges.Num());
	
	for (int i = 0; i < Edges.Num(); i++)	
	{
		FWorldBLDKitElementEdge& Edge = Edges[i];
		float DistanceSq = TNumericLimits<float>::Max();
		float IK = 0.0;
		FVector ClosestPoint = FindElementEdgeClosestPoint(Location, Edge, DistanceSq, IK);
		OutDistances.Push(DistanceSq);
		OutPoints.Push(ClosestPoint);		
	}

	float MinDistSq = MinDistance * MinDistance;
	for (int i = Edges.Num() - 1; i >= 0; i--)
	{
		if (OutDistances[i] > MinDistSq)
		{
			Edges.RemoveAt(i);
			OutDistances.RemoveAt(i);
			OutPoints.RemoveAt(i);
		}
	}

	if (bSort)
	{
		int Num = Edges.Num();
		for (int i = 1; i < Num; i++) 
		{
			bool bSorted = true;
			for (int j = 0; j < Num - i; j++) 
			{
				if (OutDistances[j] > OutDistances[j + 1])
				{
					bSorted = false;
					Swap(Edges[j], Edges[j + 1]);
					Swap(OutDistances[j], OutDistances[j + 1]);
					Swap(OutPoints[j], OutPoints[j + 1]);
				}
			}
			if (bSorted) break;		
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
