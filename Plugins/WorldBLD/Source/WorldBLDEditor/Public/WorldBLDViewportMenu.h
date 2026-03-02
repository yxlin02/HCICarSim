// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EditorUtilityWidget.h"
#include "Components/Button.h"
#include "Kismet/KismetSystemLibrary.h"
#include "WorldBLDViewportMenu.generated.h"

class UPanelWidget;
class UButton;
class UTextBlock;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMenuItemClicked, int32, Index);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMenuItemSet, TScriptInterface<UWorldBLDViewportMenuItemInterface>, Item, int32, Index);

UINTERFACE(MinimalAPI, BlueprintType)
class UWorldBLDViewportMenuItemInterface : public UInterface { GENERATED_BODY() };

USTRUCT(BlueprintType)
struct FWorldBLDViewportMenuItemData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD") FText Title;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD") FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD") bool bIsLicensed = false;

	/** Set in BP: when true, the UI should display an "update available" alert for this item. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD") bool bNewVersionAvailable = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD") UTexture2D* Icon = nullptr;
};

class WORLDBLDEDITOR_API IWorldBLDViewportMenuItemInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "WorldBLD")
	void SetIndex(int32 InIndex);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "WorldBLD")
	void SetData(const FWorldBLDViewportMenuItemData& Data);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "WorldBLD")
	void SetPluginData(const FPluginToolData& Data);

protected:
	virtual void SetIndex_Implementation(int32 InIndex) {};	
	virtual void SetData_Implementation(const FWorldBLDViewportMenuItemData& Data) {};
	virtual void SetPluginData_Implementation(const FPluginToolData& Data) {};
};

UCLASS(Blueprintable, Abstract)
class WORLDBLDEDITOR_API UWorldBLDViewportMenuItem : public UUserWidget, public IWorldBLDViewportMenuItemInterface
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD", meta = (BindWidget))
	UButton* Button;
	
	UPROPERTY(BlueprintReadOnly, Category = "WorldBLD")
	int32 Index {-1};		

	// NOTE: Intentionally not named "Data" to avoid collisions with Blueprint variables.
	UPROPERTY(BlueprintReadOnly, Category = "WorldBLD")
	FWorldBLDViewportMenuItemData MenuItemData;

	UPROPERTY()
	FOnMenuItemClicked OnMenuItemClicked;
	
protected:

	UFUNCTION()
	void OnButtonClicked() { OnMenuItemClicked.Broadcast(Index); };

	virtual void SetIndex_Implementation(int32 InIndex) { Index = InIndex; };
	virtual void SetData_Implementation(const FWorldBLDViewportMenuItemData& InData) override;
	virtual TSharedRef<SWidget> RebuildWidget() override;	
};

UCLASS()
class WORLDBLDEDITOR_API UWorldBLDViewportMenu : public UUserWidget
{
	GENERATED_BODY()

public:

	UPROPERTY(meta = (BindWidget))
	UPanelWidget* ItemContainer;

	UPROPERTY(BlueprintAssignable, Category = "WorldBLD")
	FOnMenuItemClicked OnMenuItemClicked;

	UPROPERTY(BlueprintAssignable, Category = "WorldBLD")
	FOnMenuItemSet OnMenuItemSet;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD", meta = (MustImplementInterface = "WorldBLDViewportMenuItemInterface"))
	TSubclassOf<UWidget> ItemClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD")
	int32 NumItems{ 0 };

	UFUNCTION(BlueprintCallable, Category = "WorldBLD")
	void SetItems(int32 Count);

	UFUNCTION()
	void OnItemClicked(int32 Index)
	{
		OnMenuItemClicked.Broadcast(Index);
	}

protected:
	virtual void SynchronizeProperties() override;
	virtual void NativePreConstruct() override { Super::NativePreConstruct(); };
	virtual void OnItemClicked_Implementation(int32 Index) { OnMenuItemClicked.Broadcast(Index); };		

};
