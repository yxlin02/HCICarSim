
#include "WorldBLDToolPaletteWidget.h"
#include "WorldBLDEditorToolkit.h"
#include "WorldBLDEditController.h"

void UWorldBLDToolPaletteWidgetExtended::ToolPaletteActivate_Implementation()
{
    if (IsValid(MainSubtoolClass))
    {        
        MainSubtool = CreateWidget<UWorldBLDEditController>(this, MainSubtoolClass.Get());
        if (IsValid(MainSubtool))
        {
            EdMode->PushEditController(MainSubtool);
        }
    }
}

void UWorldBLDToolPaletteWidgetExtended::ToolPaletteDeactivate_Implementation()
{
    if (IsValid(MainSubtool))
    {
        EdMode->PopEditController(MainSubtool);
    }
}