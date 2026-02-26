// Copyright WorldBLD LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class AActor;
class SBorder;

class WORLDBLDEDITOR_API SWorldBLDContextMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorldBLDContextMenu)
		: _Title(FText::GetEmpty())
		, _Content(nullptr)
		, _TargetActor(nullptr)
		, _TargetActors()
		, _ShowTitleBar(true)
	{
	}

		SLATE_ARGUMENT(FText, Title)
		SLATE_ARGUMENT(TSharedPtr<SWidget>, Content)
		SLATE_ARGUMENT(AActor*, TargetActor)
		SLATE_ARGUMENT(TArray<AActor*>, TargetActors)
		/** Shows the faux title bar (the lighter gray header area). */
		SLATE_ARGUMENT(bool, ShowTitleBar)
		/** Optional: if bound, displays a close button and triggers this when clicked. */
		SLATE_EVENT(FSimpleDelegate, OnRequestClose)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetContent(TSharedRef<SWidget> NewContent);
	void SetTitle(const FText& NewTitle);
	FText GetTitle() const { return TitleText; }

	//~ Begin SWidget interface
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	//~ End SWidget interface

	/** Shows an error banner (non-fatal) at the top of the menu content area. */
	void SetErrorMessage(const FText& InErrorMessage);
	void ClearErrorMessage();
	bool HasErrorMessage() const;

private:
	EVisibility GetErrorBannerVisibility() const;
	FText GetErrorBannerText() const;
	FReply OnDismissErrorClicked();
	FReply OnCloseClicked();
	EVisibility GetCloseButtonVisibility() const;

private:
	TSharedPtr<SBorder> ContentBorder;
	TSharedPtr<SWidget> ContentWidget;
	TSharedPtr<SBorder> ErrorBannerBorder;

	FText TitleText;
	FText ErrorMessage;
	FSimpleDelegate OnRequestClose;
	bool bShowTitleBar = true;

	TWeakObjectPtr<AActor> TargetActor;
	TArray<TWeakObjectPtr<AActor>> TargetActors;
};


