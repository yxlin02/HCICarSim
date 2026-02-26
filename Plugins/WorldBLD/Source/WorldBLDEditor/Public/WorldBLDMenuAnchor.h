// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/MenuAnchor.h"
#include "WorldBLDMenuAnchor.generated.h"

UCLASS()
class WORLDBLDEDITOR_API UWorldBLDMenuAnchor : public UMenuAnchor
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBLD")
	UWidget* MenuContent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Anchor")
	FMargin Padding;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Anchor")
	bool bApplyWidgetStyleToMenu {true};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Anchor")
	bool bShowMenuBackground {false};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu Anchor")
	bool bUseApplicationMenuStack {true};

protected:

	virtual TSharedRef<SWidget> RebuildWidget() override;
	
};
