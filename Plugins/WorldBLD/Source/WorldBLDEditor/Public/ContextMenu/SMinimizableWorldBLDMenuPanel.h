// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ContextMenu/WorldBLDContextMenuDelegates.h"
#include "Widgets/SCompoundWidget.h"

struct FSlateBrush;

/**
 * A small panel shell that matches WorldBLD context menu styling and can collapse/expand its content.
 * Intended for tool-owned viewport overlays (not selection-driven context menus).
 */
class WORLDBLDEDITOR_API SMinimizableWorldBLDMenuPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMinimizableWorldBLDMenuPanel)
		: _Title(FText::GetEmpty())
		, _Content()
		, _bInitiallyMinimized(false)
	{
	}

		SLATE_ARGUMENT(FText, Title)
		SLATE_ARGUMENT(TSharedPtr<SWidget>, Content)
		SLATE_ARGUMENT(bool, bInitiallyMinimized)
		SLATE_EVENT(FOnMinimizedChanged, OnMinimizedChanged)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FReply OnMinimizeClicked();
	const FSlateBrush* GetMinimizeIcon() const;
	EVisibility GetContentVisibility() const;

	// SWidget interface
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	FText TitleText;
	TSharedPtr<SWidget> ContentWidget;
	bool bMinimized = false;
	FOnMinimizedChanged OnMinimizedChanged;
};

