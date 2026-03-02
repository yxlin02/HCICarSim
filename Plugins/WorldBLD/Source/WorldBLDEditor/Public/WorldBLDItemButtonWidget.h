// Copyright WorldBLD. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WorldBLDItemButtonWidget.generated.h"

class UButton;
class UTexture2D;

DECLARE_DELEGATE_OneParam(FOnItemClicked, UClass*);

/**
 * UWorldBLDItemButtonWidget - Generic individual clickable item widget
 * Displays a single item as a clickable button with a thumbnail image.
 * Uses virtual methods to allow subclasses to customize how thumbnails and tooltips are extracted from target classes.
 */
UCLASS()
class WORLDBLDEDITOR_API UWorldBLDItemButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	
	/** Button that displays the item image in its style for all states */
	UPROPERTY(meta=(BindWidget))
	UButton* ItemButton;

	/** The item class associated with this widget */
	UPROPERTY(BlueprintReadOnly, Category="WorldBLD Item")
	UClass* ItemClass;

	/** Delegate called when this item is clicked */
	FOnItemClicked OnItemClicked;

	/**
	 * Configure this widget with an item class.
	 * Updates the displayed image and stores the item class reference.
	 */
	UFUNCTION(BlueprintCallable, Category="WorldBLD Item")
	void SetItem(UClass* InItemClass);

protected:
	
	virtual void NativeConstruct() override;

	/**
	 * Override this method to extract the thumbnail image from the target class.
	 * Default implementation returns nullptr.
	 * 
	 * @param InClass - The class to extract the thumbnail from
	 * @return The thumbnail texture, or nullptr if not available
	 */
	UFUNCTION(BlueprintNativeEvent, Category="WorldBLD Item")
	UTexture2D* GetThumbnailFromClass(UClass* InClass);
	virtual UTexture2D* GetThumbnailFromClass_Implementation(UClass* InClass);

	/**
	 * Override this method to extract the tooltip text from the target class.
	 * Default implementation returns empty text.
	 * 
	 * @param InClass - The class to extract the tooltip from
	 * @return The tooltip text
	 */
	UFUNCTION(BlueprintNativeEvent, Category="WorldBLD Item")
	FText GetTooltipTextFromClass(UClass* InClass);
	virtual FText GetTooltipTextFromClass_Implementation(UClass* InClass);

private:
	
	UFUNCTION()
	void OnButtonClicked();
};

