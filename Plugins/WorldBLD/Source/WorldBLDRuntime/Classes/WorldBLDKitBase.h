// Copyright WorldBLD LLC. All rights reserved. 

#pragma once

#include "GameFramework/Actor.h"
#include "Engine/DataAsset.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "WorldBLDKitBase.generated.h"

///////////////////////////////////////////////////////////////////////////////////////////////////

WORLDBLDRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WorldBLD_Style);
WORLDBLDRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WorldBLD_Style_Street);
WORLDBLDRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WorldBLD_Style_KitRoad);
WORLDBLDRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WorldBLD_Style_KitRoadModule);
WORLDBLDRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_WorldBLD_Style_RoadPreset);

///////////////////////////////////////////////////////////////////////////////////////////////////

// Used to reference or search for a particular style.
USTRUCT(BlueprintType)
struct FWorldBLDKitStyleReference
{
	GENERATED_BODY()

	/////////////////////////////////
	
	// The type of the style.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Style", Meta=(Categories="WorldBLD.Style"))
	FGameplayTag Type;
	
	// The class of the style.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Style")
	TSubclassOf<class UWorldBLDKitStyleBase> Class;
	
	// The name of the style
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Style")
	FString Name {TEXT("")};

	// Whether or not the target style is a sub-style (nested style).
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Style")
	bool bIsSubStyle {false};

	////////////////////////////////

	// The unique id of the style we are referencing.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Style")
	FGuid Guid;
};

UCLASS(BlueprintType, Blueprintable, Abstract, EditInlineNew, DefaultToInstanced)
class WORLDBLDRUNTIME_API UWorldBLDKitStyleBase : public UObject
{
	GENERATED_BODY()
public:
	UWorldBLDKitStyleBase();

	virtual void Serialize(FArchive& Ar) override;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Setup", Meta=(Categories="WorldBLD.Style"))
	FGameplayTag StyleType;

	// The human-readable (localizable) name of this style.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Setup")
	FText StyleText {FText::FromString(TEXT("New Style"))};

	// Internal name used for lookups (if you aren't using the UniqueId). 
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Setup")
	FString StyleName {TEXT("New Style")};

	// The unique id of this style.
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, AdvancedDisplay, Category="Setup")
	FGuid UniqueId;

	// Whether the id is actually shared with an existing style object.
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, AdvancedDisplay, Category="Setup")
	bool bIdIsShared {false};

	///////////////////////////////////////////////////////////////////////////////////////////////

	UFUNCTION(BlueprintPure, Category="WorldBLD")
	FWorldBLDKitStyleReference CreateReference() const;

	UFUNCTION(BlueprintPure, BlueprintNativeEvent, Category="Style")
	TArray<UWorldBLDKitStyleBase*> SubStyles() const;

	UFUNCTION(BlueprintCallable, Category="WorldBLD")
	UWorldBLDKitStyleBase* Duplicate(UObject* NewOwner, bool bSharedUniqueId = true) const;

	// Assigns a new UniqueId
	UFUNCTION(BlueprintCallable, Category="Style")
	void MakeUnique(bool bForce = false);

	//////////////////////////////////////////////////////////////////////////////////////////////

protected:
	virtual TArray<UWorldBLDKitStyleBase*> SubStyles_Implementation() const { return {}; }
};

// A primary (root-level) style.
UCLASS(BlueprintType, Blueprintable, Abstract)
class WORLDBLDRUNTIME_API UWorldBLDKitPrimaryStyleBase : public UWorldBLDKitStyleBase
{
	GENERATED_BODY()
};

// A sub (nested) style.
UCLASS(BlueprintType, Blueprintable, Abstract)
class WORLDBLDRUNTIME_API UWorldBLDKitSubStyleBase : public UWorldBLDKitStyleBase
{
	GENERATED_BODY()
};

///////////////////////////////////////////////////////////////////////////////////////////////////

// An actor placed in a map that lets you customize generation settings.
UCLASS(BlueprintType, Blueprintable, Abstract, meta=(DeprecatedNode, DeprecationMessage="Kits are no longer in use, assets should be directly discoverable instead."))
class WORLDBLDRUNTIME_API AWorldBLDKitBase : public AActor
{
	GENERATED_BODY()
public:
	///////////////////////////////////////////////////////////////////////////////////////////////

	// The name that displays in the list of Kits
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="WorldBLD", Meta=(DeprecatedProperty, DeprecationMessage="Kits are no longer in use, assets should be directly discoverable instead."))
	FText KitMenuDisplayName {FText::FromString(TEXT("Untitled Kit"))};

	// The name of the tool plugin that this Kit is compatible with
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="WorldBLD", Meta=(DeprecatedProperty, DeprecationMessage="Kits are no longer in use, assets should be directly discoverable instead."))
	FText TargetTool {FText::FromString(TEXT("Untitled Tool"))};

	
	// Extensions assigned to this Kit.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="WorldBLD", Meta=(TitleProperty="{UserDisplayName}", DeprecatedProperty, DeprecationMessage="Kits are no longer in use, assets should be directly discoverable instead."))
	TArray<class UWorldBLDKitExtension*> Extensions;
	

	///////////////////////////////////////////////////////////////////////////////////////////////

	// >> AActor
	//This function still references CityBLD, we need to move the class firs
	//virtual void OnConstruction(const FTransform& Transform) override;
	// << AActor

#if WITH_EDITOR
	UE_DEPRECATED(5.0, "Kits are no longer in use, assets should be directly discoverable instead.")
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	UE_DEPRECATED(5.0, "Kits are no longer in use, assets should be directly discoverable instead.")
	bool bDirtyStyleCache {true};

	UPROPERTY(Transient, DuplicateTransient, Meta=(DeprecatedProperty, DeprecationMessage="Kits are no longer in use, assets should be directly discoverable instead."))
	TArray<UWorldBLDKitStyleBase*> CachedAllStyles;

	UPROPERTY(Transient, DuplicateTransient, Meta=(DeprecatedProperty, DeprecationMessage="Kits are no longer in use, assets should be directly discoverable instead."))
	TArray<UWorldBLDKitStyleBase*> CachedAllStylesDeep;
};

// An extension to a kit.
UCLASS(BlueprintType, Blueprintable, meta=(DeprecatedNode, DeprecationMessage="Kits are no longer in use, assets should be directly discoverable instead."))
class WORLDBLDRUNTIME_API UWorldBLDKitExtension : public UDataAsset
{
	GENERATED_BODY()
public:
	// Human-readable name display in interfaces.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="WorldBLD", Meta=(DeprecatedProperty, DeprecationMessage="Kits are no longer in use, assets should be directly discoverable instead."))
	FText UserDisplayName;

	// The styles to add to an owning kit.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Instanced, Category="WorldBLD", Meta=(TitleProperty="[{StyleType}] {StyleText}", DeprecatedProperty, DeprecationMessage="Kits are no longer in use, assets should be directly discoverable instead."))
	TArray<UWorldBLDKitPrimaryStyleBase*> Styles;
};

///////////////////////////////////////////////////////////////////////////////////////////////////

// A reference wrapper around a Kit. This is so you have some meta-data about a kit before you
// have to fully load it.
USTRUCT(BlueprintType, meta=(DeprecatedNode, DeprecationMessage="Kits are no longer in use, assets should be directly discoverable instead."))
struct FWorldBLDKitBundleItem
{
	GENERATED_BODY()

	// Human-readable name display in interfaces.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="WorldBLD", Meta=(DeprecatedProperty, DeprecationMessage="Kits are no longer in use, assets should be directly discoverable instead."))
	FText UserDisplayName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="WorldBLD", Meta=(DeprecatedProperty, DeprecationMessage="Kits are no longer in use, assets should be directly discoverable instead."))
	FText KitDescription;

	// Soft reference to the Kit class.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="WorldBLD", Meta=(Untracked, DeprecatedProperty, DeprecationMessage="Kits are no longer in use, assets should be directly discoverable instead."))
	TSoftClassPtr<class AWorldBLDKitBase> WorldBLDKitClass;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "WorldBLD", Meta=(DeprecatedProperty, DeprecationMessage="Kits are no longer in use, assets should be directly discoverable instead."))
	FText TargetTool;
};

// A reference wrapper around a Kit. This is so you have some meta-data about a kit before you
// have to fully load it.
USTRUCT(BlueprintType, meta=(DeprecatedNode, DeprecationMessage="Kits are no longer in use, assets should be directly discoverable instead."))
struct FWorldBLDKitExtensionBundleItem
{
	GENERATED_BODY()

	// Human-readable name display in interfaces.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="WorldBLD", Meta=(DeprecatedProperty, DeprecationMessage="Kits are no longer in use, assets should be directly discoverable instead."))
	FText UserDisplayName;

	// Soft reference to the Kit Extension class.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="WorldBLD", Meta=(Untracked, DeprecatedProperty, DeprecationMessage="Kits are no longer in use, assets should be directly discoverable instead."))
	TSoftObjectPtr<class UWorldBLDKitExtension> Extension;
};

// The kinds of tutorial map
UENUM(BlueprintType, meta=(DeprecatedNode, DeprecationMessage="Kits are no longer in use, assets should be directly discoverable instead."))
enum class EWorldBLDKitExampleType : uint8
{
	Demo = 0,
	Tutorial,
	Misc,
};

// A reference wrapper to a map associated with the Kits in the bundle. These will get populated
// in the "Welcome to WorldBLD" window when you open up the tool.
USTRUCT(BlueprintType, meta=(DeprecatedNode, DeprecationMessage="Kits are no longer in use, assets should be directly discoverable instead."))
struct FWorldBLDKitExampleBundleItem
{
	GENERATED_BODY()

	// The type of example map.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="WorldBLD", Meta=(DeprecatedProperty, DeprecationMessage="Kits are no longer in use, assets should be directly discoverable instead."))
	EWorldBLDKitExampleType Type {EWorldBLDKitExampleType::Misc};

	// The title of the map
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="WorldBLD", Meta=(DeprecatedProperty, DeprecationMessage="Kits are no longer in use, assets should be directly discoverable instead."))
	FText Title;

	// The description of the map
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="WorldBLD", Meta=(DeprecatedProperty, DeprecationMessage="Kits are no longer in use, assets should be directly discoverable instead."))
	FText Description;

	// Tile image for the level preview
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="WorldBLD", Meta=(Untracked, DeprecatedProperty, DeprecationMessage="Kits are no longer in use, assets should be directly discoverable instead."))
	TSoftObjectPtr<class UTexture2D> LevelPreview;

	// The map to load
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="WorldBLD", Meta=(Untracked, DeprecatedProperty, DeprecationMessage="Kits are no longer in use, assets should be directly discoverable instead."))
	TSoftObjectPtr<UWorld> LevelReference;

	// Whether or not this example shows up in the list of examples.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="WorldBLD", Meta=(DeprecatedProperty, DeprecationMessage="Kits are no longer in use, assets should be directly discoverable instead."))
	bool bVisible {true};

	// Whether or not the user can actually load the example.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="WorldBLD", Meta=(DeprecatedProperty, DeprecationMessage="Kits are no longer in use, assets should be directly discoverable instead."))
	bool bCanLoad {true};
};

///////////////////////////////////////////////////////////////////////////////////////////////////

// A collection of Kits that a particular plugin provides.
UCLASS(BlueprintType, meta=(DeprecatedNode, DeprecationMessage="Kits are no longer in use, assets should be directly discoverable instead."))
class WORLDBLDRUNTIME_API UWorldBLDKitBundle : public UDataAsset
{
	GENERATED_BODY()
public:
	
	// The list of kits and metadata for it.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="WorldBLD", Meta=(Untracked, DeprecatedProperty, DeprecationMessage="Kits are no longer in use, assets should be directly discoverable instead."))
	TArray<FWorldBLDKitBundleItem> WorldBLDKits;

	// List of kit extensions.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="WorldBLD", Meta=(Untracked, DeprecatedProperty, DeprecationMessage="Kits are no longer in use, assets should be directly discoverable instead."))
	TArray<FWorldBLDKitExtensionBundleItem> WorldBLDKitExtensions;

	// The example maps associated with this collection of kits.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="WorldBLD", Meta=(Untracked, DeprecatedProperty, DeprecationMessage="Kits are no longer in use, assets should be directly discoverable instead."))
	TArray<FWorldBLDKitExampleBundleItem> ExampleMaps;
};

///////////////////////////////////////////////////////////////////////////////////////////////////

// These settings are loaded directly from the Plugin's INI file.
UCLASS(Config=WorldBLDKit, meta=(DeprecatedNode, DeprecationMessage="Kits are no longer in use, assets should be directly discoverable instead."))
class WORLDBLDRUNTIME_API UWorldBLDKitPluginSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
	
	// The kit BPs available from this plugin that should be loaded.
	UPROPERTY(Config, VisibleAnywhere, BlueprintReadOnly, Category="WorldBLD", Meta=(Untracked, DeprecatedProperty, DeprecationMessage="Kits are no longer in use, assets should be directly discoverable instead."))
	TSoftObjectPtr<UWorldBLDKitBundle> WorldBLDKitBundle;

};

///////////////////////////////////////////////////////////////////////////////////////////////////
