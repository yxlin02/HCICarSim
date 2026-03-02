// Copyright WorldBLD LLC. All rights reserved.

#include "ContextMenu/WorldBLDPropertyWidgetHelpers.h"

#include "ScopedTransaction.h"
#include "UObject/UnrealType.h"

FScopedTransaction* BeginPropertyTransaction(const FText& TransactionName)
{
	return new FScopedTransaction(TransactionName);
}

void EndPropertyTransaction(FScopedTransaction* Transaction)
{
	delete Transaction;
}

FText GetPropertyTooltipFromMetadata(UClass* Class, FName PropertyName)
{
	if (!IsValid(Class) || PropertyName.IsNone())
	{
		return FText::GetEmpty();
	}

	if (FProperty* Property = FindFProperty<FProperty>(Class, PropertyName))
	{
		const FString ToolTip = Property->GetMetaData(TEXT("ToolTip"));
		if (!ToolTip.IsEmpty())
		{
			return FText::FromString(ToolTip);
		}
	}

	return FText::GetEmpty();
}


