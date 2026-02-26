// Copyright WorldBLD LLC. All rights reserved. 

#pragma once

#include "WorldBLDEditController.h"
#include "WorldBLDDefaultEditController.generated.h"

// Restores the EdMode to standard engine selection behavior.
UCLASS()
class WORLDBLDEDITOR_API UWorldBLDDefaultEditController : public UWorldBLDEditController
{
    GENERATED_BODY()
protected:
    virtual bool AllowStandardSelectionControls_Implementation() const override {return true;}
};
