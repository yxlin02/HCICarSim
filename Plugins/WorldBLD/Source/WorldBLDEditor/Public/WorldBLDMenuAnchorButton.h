// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "EditorUtilityWidgetComponents.h"
#include "ViewportButtonBase.h"
#include "WorldBLDMenuAnchorButton.generated.h"

class UWorldBLDMenuAnchor;
class UWidget;
class UUserWidget;

UCLASS()
class WORLDBLDEDITOR_API UWorldBLDMenuAnchorButton : public UViewportImageButton
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WorldBLD")
	TSubclassOf<UUserWidget> MenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD")
	UWidget* MenuContent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD")
	TEnumAsByte<EMenuPlacement> MenuPlacement;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD")
	FMargin MenuOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "WorldBLD")
	FName ContentSlotName {NAME_None};

	UPROPERTY(BlueprintReadWrite, Category = "WorldBLD")
	UUserWidget* MenuWidget;

	UPROPERTY(Instanced, BlueprintReadWrite, Category = "WorldBLD")
	UWorldBLDMenuAnchor* MenuAnchor;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "WorldBLD")
	void HandleOnClicked();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "WorldBLD")
	UUserWidget* HandleGetUserMenuContentEvent();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "WorldBLD")
	void HandleMenuOpenChanged(bool IsOpen);

	UWorldBLDMenuAnchorButton(const FObjectInitializer& ObjectInitializer);
	UWidget* FindChildByNameRecursive(UWidget* InWidget, FString& Name);

protected:

	void HandleOnClicked_Implementation();
	virtual UUserWidget* HandleGetUserMenuContentEvent_Implementation();
	virtual void HandleMenuOpenChanged_Implementation(bool IsOpen) {};

	TSharedRef<SWidget> RebuildWidget() override;
	virtual void SynchronizeProperties() override;
};
