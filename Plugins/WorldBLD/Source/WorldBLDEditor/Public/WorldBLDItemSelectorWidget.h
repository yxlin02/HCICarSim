// Copyright WorldBLD. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WorldBLDItemSelectorWidget.generated.h"

class UScrollBox;
class UWrapBox;
class UWorldBLDItemButtonWidget;

/**
 * UWorldBLDItemSelectorWidget - Generic container widget that displays a gallery of items
 * Populates a WrapBox with clickable item images and triggers blueprint events on selection.
 * The WrapBox is nested inside a ScrollBox to allow scrolling when there are many items.
 * Uses virtual methods to allow customization for different item types.
 */
UCLASS()
class WORLDBLDEDITOR_API UWorldBLDItemSelectorWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	
	/** ScrollBox that contains the WrapBox for vertical scrolling */
	UPROPERTY(meta=(BindWidget))
	UScrollBox* ItemScrollBox;
	
	/** WrapBox container that holds all item widgets */
	UPROPERTY(meta=(BindWidget))
	UWrapBox* ItemContainer;

	/** Widget class to instantiate for each item */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="WorldBLD Item")
	TSubclassOf<UWorldBLDItemButtonWidget> ItemWidgetClass;

	/**
	 * Populates the gallery with the given array of item classes.
	 * Clears existing items and creates a new widget for each item.
	 */
	UFUNCTION(BlueprintCallable, Category="WorldBLD Item")
	void SetItems(const TArray<UClass*>& InItemClasses);

	/**
	 * Blueprint implementable event called when an item is clicked.
	 * Override this in blueprint or subclasses to handle item selection.
	 */
	UFUNCTION(BlueprintNativeEvent, Category="WorldBLD Item")
	void OnItemClicked(UClass* ClickedItemClass);
	virtual void OnItemClicked_Implementation(UClass* ClickedItemClass);

	/**
	 * Helper function to validate the widget is properly configured.
	 * Returns true if ready to use, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category="WorldBLD Item")
	bool IsWidgetConfigured() const;

protected:
	
	virtual void NativeConstruct() override;

	/** Internal handler for when an item widget is clicked */
	void HandleItemClicked(UClass* ItemClass);

	/** Array of created item widgets for cleanup */
	UPROPERTY(Transient)
	TArray<UWorldBLDItemButtonWidget*> ItemWidgets;
};

