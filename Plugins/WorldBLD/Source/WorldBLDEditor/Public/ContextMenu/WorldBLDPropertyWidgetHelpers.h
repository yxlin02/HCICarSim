// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "CoreMinimal.h"

class FScopedTransaction;

/**
 * Helper to detect mixed values across a multi-selection.
 * - If CurrentValue is unset, it is set to InValue and returns true.
 * - If CurrentValue is set and differs from InValue, CurrentValue is reset (mixed) and returns false.
 */
template <typename T>
bool UpdateMultipleValue(TOptional<T>& CurrentValue, T InValue)
{
	if (!CurrentValue.IsSet())
	{
		CurrentValue = InValue;
	}
	else if (CurrentValue.GetValue() != InValue)
	{
		CurrentValue.Reset();
		return false;
	}

	return true;
}

/** Begins a new property transaction (caller must EndPropertyTransaction). */
WORLDBLDEDITOR_API FScopedTransaction* BeginPropertyTransaction(const FText& TransactionName);

/** Ends (deletes) a transaction created by BeginPropertyTransaction. */
WORLDBLDEDITOR_API void EndPropertyTransaction(FScopedTransaction* Transaction);

/** Extracts a tooltip string from UPROPERTY metadata ("ToolTip") on the given class property. */
WORLDBLDEDITOR_API FText GetPropertyTooltipFromMetadata(UClass* Class, FName PropertyName);


