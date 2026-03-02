// Copyright WorldBLD LLC. All rights reserved.

#include "WorldBLDViewportToolbarButtons.h"

#include "SEditorViewportToolBarMenu.h"

///////////////////////////////////////////////////////////////////////////////////////////////////

TSharedRef<SWidget> UEditorViewportToolbarMenu::RebuildWidget()
{
	MyMenu = SNew(SEditorViewportToolbarMenu);
	/// @TODO
#if 0
	TSharedRef<SHorizontalBox> LeftToolbar = SNew(SHorizontalBox)
	+ SHorizontalBox::Slot()
	.AutoWidth()
	.Padding(2.0f, 2.0f)
	[
		SNew(SEditorViewportToolbarMenu)
		.ParentToolBar(SharedThis(this))
		.Label(this, &SCustomizableObjectEditorViewportToolBar::GetCameraMenuLabel)
		.LabelIcon(this, &SCustomizableObjectEditorViewportToolBar::GetCameraMenuLabelIcon)
		.OnGetMenuContent(this, &SCustomizableObjectEditorViewportToolBar::GenerateViewportTypeMenu)
	]
#endif
	return MyMenu.ToSharedRef();
}

void UEditorViewportToolbarMenu::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	// @TODO?
}

void UEditorViewportToolbarMenu::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyMenu.Reset();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

#if 0
{
	TSharedPtr< FExtender > InExtenders;
	FToolBarBuilder ToolbarBuilder(Viewport.Pin()->GetCommandList(), FMultiBoxCustomization::None, InExtenders);

	// Use a custom style
	FName ToolBarStyle = "EditorViewportToolBar";
	ToolbarBuilder.SetStyle(&FAppStyle::Get(), ToolBarStyle);
	ToolbarBuilder.SetLabelVisibility(EVisibility::Collapsed);

	// Transform controls cannot be focusable as it fights with the press space to change transform mode feature
	ToolbarBuilder.SetIsFocusable(false);

	ToolbarBuilder.BeginSection("Transform");
	ToolbarBuilder.BeginBlockGroup();
	{
		// Move Mode
		static FName TranslateModeName = FName(TEXT("TranslateMode"));
		ToolbarBuilder.AddToolBarButton(FCustomizableObjectEditorViewportLODCommands::Get().TranslateMode, NAME_None, TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), TranslateModeName);

		// Rotate Mode
		static FName RotateModeName = FName(TEXT("RotateMode"));
		ToolbarBuilder.AddToolBarButton(FCustomizableObjectEditorViewportLODCommands::Get().RotateMode, NAME_None, TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), RotateModeName);

		// Scale Mode
		static FName ScaleModeName = FName(TEXT("ScaleMode"));
		ToolbarBuilder.AddToolBarButton(FCustomizableObjectEditorViewportLODCommands::Get().ScaleMode, NAME_None, TAttribute<FText>(), TAttribute<FText>(), TAttribute<FSlateIcon>(), ScaleModeName);

	}
	ToolbarBuilder.EndBlockGroup();
	ToolbarBuilder.EndSection();

	ToolbarBuilder.BeginSection("RotationGridSnap");
	{
		// Grab the existing UICommand 
		FUICommandInfo* Command = FCustomizableObjectEditorViewportLODCommands::Get().RotationGridSnap.Get();

		static FName RotationSnapName = FName(TEXT("RotationSnap"));

		// Setup a GridSnapSetting with the UICommand
		ToolbarBuilder.AddWidget(SNew(SViewportToolBarComboMenu)
			.Cursor(EMouseCursor::Default)
			.Style(ToolBarStyle)
			.IsChecked(this, &SCustomizableObjectEditorViewportToolBar::IsRotationGridSnapChecked)
			.OnCheckStateChanged(this, &SCustomizableObjectEditorViewportToolBar::HandleToggleRotationGridSnap)
			.Label(this, &SCustomizableObjectEditorViewportToolBar::GetRotationGridLabel)
			.OnGetMenuContent(this, &SCustomizableObjectEditorViewportToolBar::FillRotationGridSnapMenu)
			.ToggleButtonToolTip(Command->GetDescription())
			.MenuButtonToolTip(LOCTEXT("RotationGridSnap_ToolTip", "Set the Rotation Grid Snap value"))
			.Icon(Command->GetIcon())
			.ParentToolBar(SharedThis(this))
			, RotationSnapName);
	}

	ToolbarBuilder.EndSection();

	ToolbarBuilder.SetIsFocusable(true);

	return ToolbarBuilder.MakeWidget();
}
#endif
