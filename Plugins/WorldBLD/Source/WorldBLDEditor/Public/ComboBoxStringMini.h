// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityWidgetComponents.h"
#include "ComboBoxStringMini.generated.h"

/**
 * 
 */
UCLASS()
class WORLDBLDEDITOR_API UComboBoxStringMini : public UEditorUtilityComboBoxString
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD")
	bool bHideComboBoxContent {true};

	UFUNCTION()
	void HideContent();

	UFUNCTION()
	void HideContentOnSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

protected:

	virtual TSharedRef<SWidget> RebuildWidget() override;
	
};
