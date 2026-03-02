// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "WorldBLDViewportToolbarButtons.generated.h"

/// @TODO: WIP (Do not use yet) - Expose Unreal's builtin Viewport Buttons in our Widget BP's

///////////////////////////////////////////////////////////////////////////////////////////////////

UCLASS() // (BlueprintType, Blueprintable)
class UEditorViewportToolbarMenu : public UUserWidget
{
    GENERATED_BODY()

public:
   
   /// @TODO: WIP

protected:
	//~ UWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void SynchronizeProperties() override;
	//~ End of UWidget interface

	//~ UVisual interface
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End of UVisual interface

    TSharedPtr<class SEditorViewportToolbarMenu> MyMenu;
};

///////////////////////////////////////////////////////////////////////////////////////////////////