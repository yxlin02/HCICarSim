// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "AssetRegistry/AssetData.h"
#include "Types/SlateEnums.h"

DECLARE_DELEGATE_OneParam(FOnFloatPropertyChanged, float);
DECLARE_DELEGATE_TwoParams(FOnFloatPropertyCommitted, float, ETextCommit::Type);
DECLARE_DELEGATE_OneParam(FOnAssetPropertyChanged, const FAssetData&);

/** Callback for when a minimizable panel changes its minimized state. */
DECLARE_DELEGATE_OneParam(FOnMinimizedChanged, bool /*bIsMinimized*/);


