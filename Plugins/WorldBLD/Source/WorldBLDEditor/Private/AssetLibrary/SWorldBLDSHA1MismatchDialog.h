// Copyright WorldBLD LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "AssetLibrary/WorldBLDAssetLibraryTypes.h"

DECLARE_DELEGATE_OneParam(FOnSHA1MismatchResolved, bool /* bReplaceAll */);

/**
 * Dialog displayed when downloaded asset has dependencies with SHA1 mismatches.
 * Allows user to choose whether to replace local versions with backend versions.
 */
class SWorldBLDSHA1MismatchDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorldBLDSHA1MismatchDialog) {}
		SLATE_ARGUMENT(TArray<FWorldBLDAssetDependency>, MismatchedDependencies)
		SLATE_EVENT(FOnSHA1MismatchResolved, OnResolved)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FReply HandleReplaceAll();
	FReply HandleKeepLocal();
	FReply HandleCancel();

	TSharedRef<ITableRow> GenerateDependencyRow(TSharedPtr<FWorldBLDAssetDependency> Item, const TSharedRef<STableViewBase>& OwnerTable);

private:
	TArray<TSharedPtr<FWorldBLDAssetDependency>> DependencyItems;
	FOnSHA1MismatchResolved OnResolved;
};
