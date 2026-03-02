#include "LevelTemplate/SWorldBLDLevelTemplateWindow.h"
#include "LevelTemplate/SWorldBLDLevelTemplateTile.h"
#include "LevelTemplate/WorldBLDLevelTemplateWindow.h"
#include "AssetLibrary/WorldBLDAssetLibraryWindow.h"
#include "WorldBLDEditorModule.h"

#include "WorldBLDLevelTemplateBundle.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

#include "Modules/ModuleManager.h"

#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "WorldBLDLevelTemplateWindow"

void SWorldBLDLevelTemplateWindow::Construct(const FArguments& InArgs)
{
	(void)InArgs;

	{
		FWorldBLDEditorModule& EditorModule = FModuleManager::LoadModuleChecked<FWorldBLDEditorModule>(*FWorldBLDEditorModule::GetModuleName());
		LevelTemplateBundlesChangedHandle = EditorModule.OnAvailableLevelTemplateBundlesChanged.AddSP(this, &SWorldBLDLevelTemplateWindow::HandleAvailableLevelTemplateBundlesChanged);
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		.Padding(0.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
				.Padding(FMargin(12.0f, 10.0f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Header", "WorldBLD Level Templates"))
						.Font(FAppStyle::GetFontStyle("NormalFontBold"))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SButton)
						.Text(LOCTEXT("GetMore", "Get More"))
						.OnClicked(this, &SWorldBLDLevelTemplateWindow::HandleGetMoreClicked)
					]
				]
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
				.Padding(FMargin(12.0f, 10.0f))
				[
					SNew(SOverlay)
					+ SOverlay::Slot()
					[
						SAssignNew(ScrollBox, SScrollBox)
						+ SScrollBox::Slot()
						[
							SAssignNew(WrapBox, SWrapBox)
							.UseAllottedSize(true)
							.InnerSlotPadding(FVector2D(10.0f, 10.0f))
						]
					]
					+ SOverlay::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SAssignNew(EmptyStateText, STextBlock)
						.Text(LOCTEXT("EmptyState", "No level templates found."))
						.Visibility_Lambda([this]()
						{
							return TemplateBundles.Num() > 0 ? EVisibility::Collapsed : EVisibility::Visible;
						})
					]
				]
			]
		]
	];

	RefreshTemplateList();
	RebuildGrid();
}

SWorldBLDLevelTemplateWindow::~SWorldBLDLevelTemplateWindow()
{
	if (LevelTemplateBundlesChangedHandle.IsValid() && FModuleManager::Get().IsModuleLoaded(*FWorldBLDEditorModule::GetModuleName()))
	{
		FWorldBLDEditorModule& EditorModule = FModuleManager::GetModuleChecked<FWorldBLDEditorModule>(*FWorldBLDEditorModule::GetModuleName());
		EditorModule.OnAvailableLevelTemplateBundlesChanged.Remove(LevelTemplateBundlesChangedHandle);
	}
	LevelTemplateBundlesChangedHandle.Reset();
}

void SWorldBLDLevelTemplateWindow::HandleAvailableLevelTemplateBundlesChanged()
{
	RefreshTemplateList();
	RebuildGrid();
}

void SWorldBLDLevelTemplateWindow::RefreshTemplateList()
{
	TemplateBundles.Reset();

	FWorldBLDEditorModule& EditorModule = FModuleManager::LoadModuleChecked<FWorldBLDEditorModule>(*FWorldBLDEditorModule::GetModuleName());
	for (const TWeakObjectPtr<UWorldBLDLevelTemplateBundle>& WeakBundle : EditorModule.GetAvailableLevelTemplateBundles())
	{
		if (WeakBundle.IsValid())
		{
			TemplateBundles.AddUnique(WeakBundle);
		}
	}

	TemplateBundles.Sort([](const TWeakObjectPtr<UWorldBLDLevelTemplateBundle>& A, const TWeakObjectPtr<UWorldBLDLevelTemplateBundle>& B)
	{
		const FString NameA = A.IsValid() ? A->TemplateName : FString();
		const FString NameB = B.IsValid() ? B->TemplateName : FString();
		return NameA < NameB;
	});

	if (EmptyStateText.IsValid())
	{
		EmptyStateText->Invalidate(EInvalidateWidgetReason::Layout);
	}
}

void SWorldBLDLevelTemplateWindow::RebuildGrid()
{
	if (!WrapBox.IsValid())
	{
		return;
	}

	WrapBox->ClearChildren();

	for (const TWeakObjectPtr<UWorldBLDLevelTemplateBundle>& WeakBundle : TemplateBundles)
	{
		UWorldBLDLevelTemplateBundle* Bundle = WeakBundle.Get();
		if (!IsValid(Bundle))
		{
			continue;
		}

		WrapBox->AddSlot()
		[
			SNew(SWorldBLDLevelTemplateTile)
			.Bundle(Bundle)
			.OnClicked(FOnWorldBLDLevelTemplateTileClicked::CreateSP(this, &SWorldBLDLevelTemplateWindow::HandleTemplateTileClicked))
		];
	}
}

FReply SWorldBLDLevelTemplateWindow::HandleGetMoreClicked()
{
	FWorldBLDAssetLibraryWindow::OpenAssetLibrary();
	return FReply::Handled();
}

FReply SWorldBLDLevelTemplateWindow::HandleTemplateTileClicked(UWorldBLDLevelTemplateBundle* Bundle)
{
	if (!IsValid(Bundle))
	{
		return FReply::Handled();
	}

	SelectedTemplate = Bundle;
	FWorldBLDLevelTemplateWindow::OnLevelTemplateSelected().Broadcast(Bundle);

	if (Bundle->CompatibleLightingPresets.Num() > 0)
	{
		UE_LOG(LogWorldBuild, Log, TEXT("LevelTemplate selected: %s (has %d lighting presets)"), *Bundle->GetPathName(), Bundle->CompatibleLightingPresets.Num());
	}
	else
	{
		UE_LOG(LogWorldBuild, Log, TEXT("LevelTemplate selected: %s (no lighting presets)"), *Bundle->GetPathName());
	}

	return FReply::Handled();
}

FReply SWorldBLDLevelTemplateWindow::CloseHostWindow()
{
	TSharedPtr<SWindow> HostWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
	if (HostWindow.IsValid())
	{
		HostWindow->RequestDestroyWindow();
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
