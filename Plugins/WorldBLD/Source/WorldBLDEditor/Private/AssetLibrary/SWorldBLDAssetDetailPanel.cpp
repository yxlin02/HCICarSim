// Copyright WorldBLD LLC. All Rights Reserved.

#include "AssetLibrary/SWorldBLDAssetDetailPanel.h"

#include "AssetLibrary/WorldBLDAssetDownloadManager.h"
#include "AssetLibrary/WorldBLDAssetLibraryImageLoader.h"
#include "AssetLibrary/WorldBLDAssetLibrarySubsystem.h"
#include "Authorization/WorldBLDAuthSubsystem.h"
#include "ContextMenu/WorldBLDContextMenuFont.h"
#include "ContextMenu/WorldBLDContextMenuStyle.h"

#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Modules/ModuleManager.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformProcess.h"
#include "Misc/MessageDialog.h"
#include "Interfaces/IPluginManager.h"
#include "WorldBLDEditorToolkit.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Notifications/SProgressBar.h"

#include "Widgets/SWorldBLDCoverImage.h"

namespace
{
	static FSlateFontInfo MakeAppFont(const FName& FontStyleName, int32 Size)
	{
		FSlateFontInfo Font = FAppStyle::Get().GetFontStyle(FontStyleName);
		Font.Size = Size;
		return Font;
	}

	static FString ExtractAssetTypeLabelFromFeatures(const TArray<FString>& Features)
	{
		if (Features.Num() <= 0)
		{
			return FString();
		}

		const FString First = Features[0].TrimStartAndEnd();
		if (First.IsEmpty())
		{
			return FString();
		}

		static const FString Prefix = TEXT("Asset Type:");
		if (!First.StartsWith(Prefix, ESearchCase::IgnoreCase))
		{
			return FString();
		}

		FString TypeLabel = First.Mid(Prefix.Len()).TrimStartAndEnd();
		return TypeLabel;
	}

	static FString GetTypeBlurbForAssetTypeLabel(const FString& TypeLabel)
	{
		const FString Trimmed = TypeLabel.TrimStartAndEnd();
		if (Trimmed.IsEmpty())
		{
			return FString();
		}

		if (Trimmed.Equals(TEXT("Static Mesh"), ESearchCase::IgnoreCase))
		{
			return TEXT("A professionally crafted, game-ready static mesh asset, built to AAA optimization standards.");
		}

		if (Trimmed.Equals(TEXT("WorldBLD Assembly"), ESearchCase::IgnoreCase))
		{
			return TEXT("A complete drag-and-drop assembly, saved as a WorldBLD Prefab Actor to maximize performance.");
		}

		if (Trimmed.Equals(TEXT("CityBLD Prefab"), ESearchCase::IgnoreCase))
		{
			return TEXT("A complete assembly for use with CityBLD tools, built to maximize performance.");
		}

		if (Trimmed.Equals(TEXT("CityBLD Building Style"), ESearchCase::IgnoreCase))
		{
			return TEXT("A fully configured Building Style for use with CityBLD. Just select this Style in any building's context menu to use it!");
		}

		if (Trimmed.Equals(TEXT("CityBLD District"), ESearchCase::IgnoreCase))
		{
			return TEXT("A fully configured District for use with CityBLD. Just select this District when you create a new City Block to use it!");
		}

		if (Trimmed.Equals(TEXT("RoadBLD Road Preset"), ESearchCase::IgnoreCase))
		{
			return TEXT("A fully configured Road Preset for use with RoadBLD.");
		}

		return FString();
	}

	static FSlateFontInfo GetCreditsLabelFont()
	{
		return WorldBLDContextMenuFont::GetThin(12.0f, 0);
	}

	static FSlateFontInfo GetCreditsValueFont()
	{
		return MakeAppFont(TEXT("BoldFont"), 12);
	}

	static FText CreditsToText(int32 Credits)
	{
		return FText::FromString(FString::Printf(TEXT("%d Credits"), Credits));
	}

	static FText StatusToText(EWorldBLDAssetStatus Status)
	{
		switch (Status)
		{
		case EWorldBLDAssetStatus::NotOwned:
			return FText::FromString(TEXT("Not Owned"));
		case EWorldBLDAssetStatus::Owned:
			return FText::FromString(TEXT("Owned"));
		case EWorldBLDAssetStatus::Downloading:
			return FText::FromString(TEXT("Downloading"));
		case EWorldBLDAssetStatus::Downloaded:
			return FText::FromString(TEXT("Downloaded"));
		case EWorldBLDAssetStatus::Imported:
			return FText::FromString(TEXT("Imported"));
		default:
			return FText::FromString(TEXT("Unknown"));
		}
	}

	static EWorldBLDAssetStatus GetStatusForSelection(TWeakObjectPtr<UWorldBLDAssetDownloadManager> DownloadManager, const FString& AssetId)
	{
		if (!DownloadManager.IsValid() || AssetId.IsEmpty())
		{
			return EWorldBLDAssetStatus::NotOwned;
		}
		return DownloadManager->GetAssetStatus(AssetId);
	}

	static int32 ParseSemVerToSortableInt(const FString& InVersion)
	{
		// Converts "Major.Minor.Patch" into an integer for easy comparison.
		// We intentionally clamp to 3 components; any additional suffixes are ignored (e.g. "1.2.3-preview" -> 1.2.3).
		int32 Major = 0;
		int32 Minor = 0;
		int32 Patch = 0;

		const FString Clean = InVersion.TrimStartAndEnd();
		if (Clean.IsEmpty())
		{
			return 0;
		}

		TArray<FString> Parts;
		Clean.ParseIntoArray(Parts, TEXT("."), /*bCullEmpty=*/true);
		if (Parts.Num() > 0)
		{
			Major = FCString::Atoi(*Parts[0]);
		}
		if (Parts.Num() > 1)
		{
			Minor = FCString::Atoi(*Parts[1]);
		}
		if (Parts.Num() > 2)
		{
			// Strip any non-digit suffix from patch (e.g. "3-alpha" -> "3")
			FString PatchToken = Parts[2];
			int32 DigitCount = 0;
			while (DigitCount < PatchToken.Len() && FChar::IsDigit(PatchToken[DigitCount]))
			{
				++DigitCount;
			}
			if (DigitCount > 0)
			{
				PatchToken = PatchToken.Left(DigitCount);
				Patch = FCString::Atoi(*PatchToken);
			}
		}

		// Sortable representation. (1.0.5 => 10005, 1.0.10 => 10010)
		return (Major * 10000) + (Minor * 100) + Patch;
	}

	static bool TryExtractToolIdFromDownloadLink(const FString& DownloadLink, FString& OutToolId)
	{
		OutToolId.Reset();

		// Expected shape (from logs): https://worldbld.com/api/tools/<toolId>/download
		static const FString Marker = TEXT("/api/tools/");
		const int32 MarkerIndex = DownloadLink.Find(Marker, ESearchCase::IgnoreCase, ESearchDir::FromStart);
		if (MarkerIndex == INDEX_NONE)
		{
			return false;
		}

		const int32 AfterMarker = MarkerIndex + Marker.Len();
		if (AfterMarker >= DownloadLink.Len())
		{
			return false;
		}

		const int32 NextSlashIndex = DownloadLink.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromStart, AfterMarker);
		if (NextSlashIndex == INDEX_NONE || NextSlashIndex <= AfterMarker)
		{
			return false;
		}

		OutToolId = DownloadLink.Mid(AfterMarker, NextSlashIndex - AfterMarker).TrimStartAndEnd();
		return !OutToolId.IsEmpty();
	}

	static FString ResolveToolNameFromToolId(const FString& ToolId, const TArray<FString>* AuthorizedTools)
	{
		const FString TrimmedId = ToolId.TrimStartAndEnd();
		if (TrimmedId.IsEmpty())
		{
			return FString();
		}

		// If the user is logged in, the licenses endpoint returns strings like:
		// "696d3c5a283db15c8d0ac174 | CityBLD"
		// This also includes WorldBLD, which the tools list intentionally filters out in the UI.
		if (AuthorizedTools != nullptr)
		{
			for (const FString& Entry : *AuthorizedTools)
			{
				FString Left;
				FString Right;
				if (Entry.Split(TEXT("|"), &Left, &Right))
				{
					const FString EntryId = Left.TrimStartAndEnd();
					const FString EntryName = Right.TrimStartAndEnd();
					if (!EntryId.IsEmpty() && EntryId == TrimmedId && !EntryName.IsEmpty())
					{
						return EntryName;
					}
				}
			}
		}

		// Local fallback: if this machine has installed a tool (including via trial), the installer persists a mapping
		// from the server tool id to the local plugin name. Prefer this over license-only sources.
		if (GConfig)
		{
			FString MappedPluginName;
			if (GConfig->GetString(TEXT("WorldBLD.ToolIdMap"), *TrimmedId, MappedPluginName, GEditorPerProjectIni))
			{
				MappedPluginName = MappedPluginName.TrimStartAndEnd();
				if (!MappedPluginName.IsEmpty())
				{
					return MappedPluginName;
				}
			}
		}

		// Prefer the server tool list (public endpoint) cached on the WorldBLD EdMode when available.
		if (UWorldBLDEdMode* EdMode = UWorldBLDEdMode::GetWorldBLDEdMode())
		{
			// WorldBLD itself is intentionally filtered out of AvailableTools (UI), but we still cache its tool id.
			// If this required tool matches that cached id, resolve it to "WorldBLD" so gating works as intended.
			const FString& WorldBLDToolId = EdMode->GetServerWorldBLDToolId();
			if (!WorldBLDToolId.IsEmpty() && WorldBLDToolId.Equals(TrimmedId, ESearchCase::IgnoreCase))
			{
				return TEXT("WorldBLD");
			}

			for (const FPluginToolData& ToolData : EdMode->AvailableTools)
			{
				FString ParsedId;
				if (TryExtractToolIdFromDownloadLink(ToolData.DownloadLink, ParsedId) && ParsedId == TrimmedId)
				{
					return ToolData.ToolName;
				}
			}
		}

		// If the backend ever sends plugin names directly (instead of ids), accept them.
		if (IPluginManager::Get().FindPlugin(TrimmedId).IsValid())
		{
			return TrimmedId;
		}

		// Fallback: return the raw id (not ideal, but better than an empty string).
		return TrimmedId;
	}

	static bool IsPluginMountedAndEnabled(const FString& PluginName)
	{
		const FString TrimmedName = PluginName.TrimStartAndEnd();
		if (TrimmedName.IsEmpty())
		{
			return false;
		}

		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TrimmedName);
		if (!Plugin.IsValid())
		{
			return false;
		}

		// If it's not enabled, its content won't be mounted.
		return Plugin->IsEnabled();
	}

	static FString GetPluginVersionString(const FString& PluginName)
	{
		const FString TrimmedName = PluginName.TrimStartAndEnd();
		if (TrimmedName.IsEmpty())
		{
			return FString();
		}

		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TrimmedName);
		if (!Plugin.IsValid())
		{
			return FString();
		}

		const FString VersionName = Plugin->GetDescriptor().VersionName.TrimStartAndEnd();
		if (!VersionName.IsEmpty())
		{
			return VersionName;
		}

		const int32 Version = Plugin->GetDescriptor().Version;
		return Version > 0 ? FString::FromInt(Version) : FString();
	}
}

SWorldBLDAssetDetailPanel::~SWorldBLDAssetDetailPanel()
{
	if (ApplicationReactivatedHandle.IsValid())
	{
		FCoreDelegates::ApplicationHasReactivatedDelegate.Remove(ApplicationReactivatedHandle);
		ApplicationReactivatedHandle.Reset();
	}
}

void SWorldBLDAssetDetailPanel::Construct(const FArguments& InArgs)
{
	bIsKit = InArgs._bIsKit;
	CurrentAsset = InArgs._AssetInfo;
	CurrentKit = InArgs._KitInfo;
	OnCloseRequested = InArgs._OnCloseRequested;

	AssetLibrarySubsystem = GEditor ? GEditor->GetEditorSubsystem<UWorldBLDAssetLibrarySubsystem>() : nullptr;
	DownloadManager = GEditor ? GEditor->GetEditorSubsystem<UWorldBLDAssetDownloadManager>() : nullptr;
	AuthSubsystem = GEditor ? GEditor->GetEditorSubsystem<UWorldBLDAuthSubsystem>() : nullptr;

	// We want credits-based purchase gating to work without forcing the user to manually refresh.
	// ValidateSession() populates CachedUserCredits on success.
	if (AuthSubsystem.IsValid() && !AuthSubsystem->GetSessionToken().IsEmpty())
	{
		AuthSubsystem->ValidateSession();
	}

	// Keep credits reasonably fresh while the panel exists, and refresh after returning from an external browser flow.
	if (!CreditsRefreshTimerHandle.IsValid())
	{
		CreditsRefreshTimerHandle = RegisterActiveTimer(1.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SWorldBLDAssetDetailPanel::HandleCreditsRefreshTick));
	}
	if (!ApplicationReactivatedHandle.IsValid())
	{
		ApplicationReactivatedHandle = FCoreDelegates::ApplicationHasReactivatedDelegate.AddSP(this, &SWorldBLDAssetDetailPanel::HandleApplicationHasReactivated);
	}

	// 300ms cubic in/out animation to match a modern "drawer" feel.
	ExpandCollapseAnimation = FCurveSequence(0.0f, 0.3f);
	ExpandCurve = ExpandCollapseAnimation.AddCurve(0.0f, 0.3f, ECurveEaseFunction::CubicInOut);

	// The panel is always present, but HeightOverride drives visibility/space.
	// We keep an overlay so the panel can have its own background.
	ChildSlot
	[
		SNew(SBox)
		.HeightOverride(this, &SWorldBLDAssetDetailPanel::GetAnimatedHeight)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
			// Title bar removed; reduce top padding so content aligns to the top edge.
			.Padding(FMargin(12.0f, 0.0f, 12.0f, 10.0f))
			.Clipping(EWidgetClipping::ClipToBoundsAlways)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(0.6f)
						.Padding(FMargin(0.0f, 0.0f, 12.0f, 0.0f))
						[
							SNew(SOverlay)
							+ SOverlay::Slot()
							[
								SNew(SBorder)
								.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.GroupBorder"))
								.Padding(0.0f)
								.Clipping(EWidgetClipping::ClipToBounds)
								[
									SNew(SBox)
									.MaxDesiredWidth(1280.0f)
									.MaxDesiredHeight(720.0f)
									.HAlign(HAlign_Fill)
									.VAlign(VAlign_Fill)
									[
										// Fill all available space vertically and crop as needed ("cover" behavior).
										SAssignNew(PreviewImageWidget, SWorldBLDCoverImage)
										.Image(this, &SWorldBLDAssetDetailPanel::GetCurrentPreviewBrush)
									]
								]
							]
							+ SOverlay::Slot()
							.HAlign(HAlign_Left)
							.VAlign(VAlign_Center)
							.Padding(FMargin(8.0f, 0.0f))
							[
								SNew(SButton)
								.ButtonStyle(FAppStyle::Get(), "Button")
								.Visibility(this, &SWorldBLDAssetDetailPanel::GetPreviewNavigationVisibility)
								.Text(FText::FromString(TEXT("<")))
								.OnClicked(this, &SWorldBLDAssetDetailPanel::HandlePrevImage)
								.IsEnabled_Lambda([this]() { return PreviewImageBrushes.Num() > 1; })
							]
							+ SOverlay::Slot()
							.HAlign(HAlign_Right)
							.VAlign(VAlign_Center)
							.Padding(FMargin(8.0f, 0.0f))
							[
								SNew(SButton)
								.ButtonStyle(FAppStyle::Get(), "Button")
								.Visibility(this, &SWorldBLDAssetDetailPanel::GetPreviewNavigationVisibility)
								.Text(FText::FromString(TEXT(">")))
								.OnClicked(this, &SWorldBLDAssetDetailPanel::HandleNextImage)
								.IsEnabled_Lambda([this]() { return PreviewImageBrushes.Num() > 1; })
							]
						]
						+ SHorizontalBox::Slot()
						.FillWidth(0.4f)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.FillHeight(1.0f)
							[
								SNew(SScrollBox)
								// Title moved to sidebar (next to preview image).
								+ SScrollBox::Slot()
								[
									SNew(STextBlock)
									.Text_Lambda([this]()
									{
										return bShowingFeatured ? FText::FromString(CurrentFeaturedCollection.Title) : GetTitleText();
									})
									.Font_Lambda([]()
									{
										FSlateFontInfo TitleFont = FAppStyle::Get().GetFontStyle("BoldFont");
										TitleFont.Size = 32;
										return TitleFont;
									})
									.ColorAndOpacity(FLinearColor(0.95f, 0.95f, 0.95f, 1.0f))
									.AutoWrapText(true)
								]
								+ SScrollBox::Slot()
								.Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
								[
									SNew(SHorizontalBox)
									.Visibility_Lambda([this]()
									{
										return (!bIsKit && !bShowingFeatured) ? EVisibility::Visible : EVisibility::Collapsed;
									})
									+ SHorizontalBox::Slot()
									.AutoWidth()
									.VAlign(VAlign_Center)
									[
										SNew(STextBlock)
										.Text(FText::FromString(TEXT("Created By: ")))
										.Font(GetCreditsLabelFont())
										.AutoWrapText(true)
									]
									+ SHorizontalBox::Slot()
									.FillWidth(1.0f)
									.VAlign(VAlign_Center)
									[
										SNew(STextBlock)
										.Text_Lambda([this]()
										{
											const FString Trimmed = CurrentAsset.Author.TrimStartAndEnd();
											return FText::FromString(Trimmed.IsEmpty() ? TEXT("WorldBLD") : Trimmed);
										})
										.Font(GetCreditsValueFont())
										.AutoWrapText(true)
									]
								]
								+ SScrollBox::Slot()
								.Padding(FMargin(0.0f, 12.0f, 0.0f, 0.0f))
								[
									SNew(STextBlock)
									.Text(this, &SWorldBLDAssetDetailPanel::GetDescriptionText)
									.AutoWrapText(true)
								]
								// Dependency information section
								+ SScrollBox::Slot()
								.Padding(FMargin(0.0f, 12.0f, 0.0f, 0.0f))
								[
									SNew(STextBlock)
									.Text(this, &SWorldBLDAssetDetailPanel::GetDependencyInfoText)
									.Visibility(this, &SWorldBLDAssetDetailPanel::GetDependencyInfoVisibility)
									.ColorAndOpacity_Lambda([this]()
									{
										// Show yellow/warning color if there are missing dependencies (use cached values)
										if (!bIsKit && CurrentAsset.Manifest.Dependencies.Num() > 0 && bDependencyCacheValid)
										{
											if (CachedMissingDepsCount > 0)
											{
												return FSlateColor(FLinearColor(0.95f, 0.75f, 0.25f, 1.0f)); // Orange/yellow warning
											}
										}
										return FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f)); // Gray
									})
									.AutoWrapText(true)
								]
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(FMargin(0.0f, 10.0f, 0.0f, 0.0f))
							[
								SNew(SVerticalBox)
								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(FMargin(0.0f, 0.0f, 0.0f, 6.0f))
								[
									SNew(SOverlay)
									+ SOverlay::Slot()
									[
										SNew(STextBlock)
										.Text(this, &SWorldBLDAssetDetailPanel::GetStatusText)
										.Visibility(this, &SWorldBLDAssetDetailPanel::GetStatusTextVisibility)
										.ColorAndOpacity(this, &SWorldBLDAssetDetailPanel::GetStatusColor)
										.Font_Lambda([]()
										{
											return WorldBLDContextMenuFont::GetMedium(16.0f, -10);
										})
									]
									+ SOverlay::Slot()
									[
										SNew(SButton)
										.ButtonStyle(&WorldBLDContextMenuStyle::GetHighlightedButtonStyleNoOutline())
										.Visibility(this, &SWorldBLDAssetDetailPanel::GetViewAssetButtonVisibility)
										.OnClicked(this, &SWorldBLDAssetDetailPanel::HandleViewAssetClicked)
										.HAlign(HAlign_Center)
										.VAlign(VAlign_Center)
										.ContentPadding(FMargin(3.0f))
										[
											SNew(STextBlock)
											.Text(FText::FromString(TEXT("View Asset")).ToUpper())
											.Font_Lambda([]()
											{
												// Match the Asset Status text font.
												return WorldBLDContextMenuFont::GetMedium(16.0f, -10);
											})
											.Justification(ETextJustify::Center)
										]
									]
								]
								+ SVerticalBox::Slot()
								.AutoHeight()
								[
									SNew(SVerticalBox)
									+ SVerticalBox::Slot()
									.AutoHeight()
									.HAlign(HAlign_Fill)
									[
										SNew(SVerticalBox)
										// Purchase button (full width)
										+ SVerticalBox::Slot()
										.AutoHeight()
										.HAlign(HAlign_Fill)
										.Padding(FMargin(0.0f, 8.0f, 0.0f, 0.0f))
										[
											SNew(SButton)
											.ButtonStyle(&WorldBLDContextMenuStyle::GetHighlightedButtonStyleNoOutline())
											.Visibility(this, &SWorldBLDAssetDetailPanel::GetPurchaseButtonVisibility)
											.OnClicked(this, &SWorldBLDAssetDetailPanel::HandlePurchaseClicked)
											.HAlign(HAlign_Center)
											.VAlign(VAlign_Center)
											.ContentPadding(FMargin(3.0f))
											.IsEnabled_Lambda([this]()
											{
												return AreToolRequirementsSatisfied() && AuthSubsystem.IsValid() && UWorldBLDAuthSubsystem::IsLoggedIn();
											})
											[
												SNew(STextBlock)
												.Text(this, &SWorldBLDAssetDetailPanel::GetPurchaseButtonText)
												.Font_Lambda([]()
												{
													// Match the Asset Status text font.
													return WorldBLDContextMenuFont::GetMedium(16.0f, -10);
												})
												.Justification(ETextJustify::Center)
											]
										]
										// Download button (full width)
										+ SVerticalBox::Slot()
										.AutoHeight()
										.HAlign(HAlign_Fill)
										.Padding(FMargin(0.0f, 8.0f, 0.0f, 0.0f))
										[
											SNew(SButton)
											.ButtonStyle(&WorldBLDContextMenuStyle::GetHighlightedButtonStyleNoOutline())
											.Visibility(this, &SWorldBLDAssetDetailPanel::GetDownloadButtonVisibility)
											.OnClicked(this, &SWorldBLDAssetDetailPanel::HandleDownloadClicked)
											.HAlign(HAlign_Center)
											.VAlign(VAlign_Center)
											.ContentPadding(FMargin(3.0f))
											.IsEnabled_Lambda([this]()
											{
												const FString AppId = GetCurrentAssetId();
												return AreToolRequirementsSatisfied()
													&& DownloadManager.IsValid()
													&& !AppId.IsEmpty()
													&& !DownloadManager->IsDownloadInProgress(AppId);
											})
											[
												SNew(STextBlock)
												.Text(FText::FromString(TEXT("Download")).ToUpper())
												.Font_Lambda([]()
												{
													// Match the Asset Status text font.
													return WorldBLDContextMenuFont::GetMedium(16.0f, -10);
												})
												.Justification(ETextJustify::Center)
											]
										]
										// Download progress (below button, full width)
										+ SVerticalBox::Slot()
										.AutoHeight()
										.Padding(FMargin(0.0f, 6.0f, 0.0f, 0.0f))
										[
											SNew(SProgressBar)
											.Visibility(this, &SWorldBLDAssetDetailPanel::GetDownloadProgressVisibility)
											.Percent(this, &SWorldBLDAssetDetailPanel::GetDownloadProgressValue)
										]
										// Import button (full width)
										+ SVerticalBox::Slot()
										.AutoHeight()
										.HAlign(HAlign_Fill)
										.Padding(FMargin(0.0f, 8.0f, 0.0f, 0.0f))
										[
											SNew(SButton)
											.ButtonStyle(&WorldBLDContextMenuStyle::GetHighlightedButtonStyleNoOutline())
											.Visibility(this, &SWorldBLDAssetDetailPanel::GetImportButtonVisibility)
											.OnClicked(this, &SWorldBLDAssetDetailPanel::HandleImportClicked)
											.HAlign(HAlign_Center)
											.VAlign(VAlign_Center)
											.ContentPadding(FMargin(3.0f))
											.IsEnabled_Lambda([this]()
											{
												return AreToolRequirementsSatisfied();
											})
											[
												SNew(STextBlock)
												.Text(FText::FromString(TEXT("Import")).ToUpper())
												.Font_Lambda([]()
												{
													// Match the Asset Status text font.
													return WorldBLDContextMenuFont::GetMedium(16.0f, -10);
												})
												.Justification(ETextJustify::Center)
											]
										]
									]
									// Credits row now sits beneath the action button.
									+ SVerticalBox::Slot()
									.AutoHeight()
									.Padding(FMargin(0.0f, 10.0f, 0.0f, 0.0f))
									[
										SNew(SHorizontalBox)
										+ SHorizontalBox::Slot()
										.FillWidth(1.0f)
										[
											SNew(SSpacer)
										]
										+ SHorizontalBox::Slot()
										.AutoWidth()
										.VAlign(VAlign_Center)
										[
											SNew(STextBlock)
											.Text(FText::FromString(TEXT("Your Credits: ")))
											.Font(GetCreditsLabelFont())
										]
										+ SHorizontalBox::Slot()
										.AutoWidth()
										.VAlign(VAlign_Center)
										[
											SNew(STextBlock)
											.Text_Lambda([this]()
											{
												return FText::AsNumber(AuthSubsystem.IsValid() ? AuthSubsystem->GetUserCredits() : 0);
											})
											.Font(GetCreditsValueFont())
										]
										+ SHorizontalBox::Slot()
										.AutoWidth()
										.VAlign(VAlign_Center)
										.Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
										[
											SNew(SHyperlink)
											.Text(FText::FromString(TEXT("Uninstall Asset")))
											.ToolTipText(FText::FromString(TEXT("Delete the local cache and imported content under /Game/WorldBLD/Assets for this asset.")))
											.Visibility(this, &SWorldBLDAssetDetailPanel::GetUninstallAssetVisibility)
											.IsEnabled(this, &SWorldBLDAssetDetailPanel::CanUninstallAsset)
											.OnNavigate(this, &SWorldBLDAssetDetailPanel::HandleUninstallAssetClicked)
										]
									]
								]
						]
					]
				]
			]
		]
	]
	];

	// Initialize the preview image with a placeholder so the widget tree is stable.
	PreviewImageBrushes.Reset();
	PreviewImageBrushes.Add(FWorldBLDAssetLibraryImageLoader::Get().GetPlaceholderBrush());
	CurrentPreviewIndex = 0;
	BeginLoadPreviewImages(bIsKit ? CurrentKit.PreviewImages : CurrentAsset.PreviewImages);

	// By default, start collapsed.
	bIsExpanded = false;
}

EActiveTimerReturnType SWorldBLDAssetDetailPanel::HandleCreditsRefreshTick(double CurrentTime, float DeltaTime)
{
	(void)DeltaTime;

	// Only poll while the panel is visible/open to keep network usage minimal.
	if (bIsExpanded && AuthSubsystem.IsValid() && !AuthSubsystem->GetSessionToken().IsEmpty())
	{
		const double Last = LastCreditsRefreshRequestTimeSeconds;
		const double Elapsed = (Last < 0.0) ? BIG_NUMBER : (CurrentTime - Last);
		if (Elapsed >= CreditsRefreshIntervalSeconds)
		{
			RequestCreditsRefreshFromServer(/*bForce=*/false);
		}
	}

	return EActiveTimerReturnType::Continue;
}

void SWorldBLDAssetDetailPanel::RequestCreditsRefreshFromServer(bool bForce)
{
	if (!AuthSubsystem.IsValid() || AuthSubsystem->GetSessionToken().IsEmpty())
	{
		return;
	}

	const double Now = FPlatformTime::Seconds();
	if (!bForce && LastCreditsRefreshRequestTimeSeconds >= 0.0 && (Now - LastCreditsRefreshRequestTimeSeconds) < CreditsRefreshMinGapSeconds)
	{
		return;
	}

	LastCreditsRefreshRequestTimeSeconds = Now;
	AuthSubsystem->ValidateSession(); // Uses GET /api/auth/desktop/session (same endpoint as on widget construction).
}

void SWorldBLDAssetDetailPanel::HandleApplicationHasReactivated()
{
	if (!bPendingCreditsRefreshOnAppFocus)
	{
		return;
	}

	bPendingCreditsRefreshOnAppFocus = false;
	RequestCreditsRefreshFromServer(/*bForce=*/true);
}

void SWorldBLDAssetDetailPanel::SetHostHeight(float InHostHeight)
{
	CachedHostHeight = FMath::Max(0.0f, InHostHeight);
}

void SWorldBLDAssetDetailPanel::ShowAsset(const FWorldBLDAssetInfo& Asset)
{
	bShowingFeatured = false;
	bIsKit = false;
	CurrentAsset = Asset;
	CurrentKit = FWorldBLDKitInfo();

	// Invalidate dependency cache when switching assets
	bDependencyCacheValid = false;
	BeginLoadPreviewImages(CurrentAsset.PreviewImages);
	EvaluateToolRequirements(/*bShowDialogIfUnmet=*/true);

	// If the list item is missing richer fields, trigger a detail fetch.
	if (AssetLibrarySubsystem.IsValid() && !CurrentAsset.ID.IsEmpty())
	{
		AssetLibrarySubsystem->FetchAssetDetails(CurrentAsset.ID);
	}
}

void SWorldBLDAssetDetailPanel::ShowKit(const FWorldBLDKitInfo& Kit)
{
	bShowingFeatured = false;
	bIsKit = true;
	CurrentKit = Kit;
	CurrentAsset = FWorldBLDAssetInfo();
	BeginLoadPreviewImages(CurrentKit.PreviewImages);

	if (AssetLibrarySubsystem.IsValid() && !CurrentKit.ID.IsEmpty())
	{
		AssetLibrarySubsystem->FetchKitDetails(CurrentKit.ID);
	}
}

void SWorldBLDAssetDetailPanel::ShowFeaturedContent(const FWorldBLDFeaturedCollection& Collection)
{
	// The initial implementation just shows the collection header. Actual featured tiles will be wired later.
	bShowingFeatured = true;
	CurrentFeaturedCollection = Collection;

	PreviewImageBrushes.Reset();
	PreviewImageBrushes.Add(FWorldBLDAssetLibraryImageLoader::Get().GetPlaceholderBrush());
	CurrentPreviewIndex = 0;
}

void SWorldBLDAssetDetailPanel::Expand()
{
	// Idempotent: switching the selected asset while the panel is already open should not replay the expansion.
	// Ensure we end up fully expanded, even if the animation was interrupted.
	if (bIsExpanded)
	{
		ExpandCollapseAnimation.JumpToEnd();
		return;
	}

	bIsExpanded = true;
	ExpandCollapseAnimation.Play(this->AsShared());
}

void SWorldBLDAssetDetailPanel::Collapse()
{
	// Idempotent collapse for safety (e.g., multiple collapse calls from different UX triggers).
	if (!bIsExpanded)
	{
		ExpandCollapseAnimation.JumpToStart();
		return;
	}

	bIsExpanded = false;
	ExpandCollapseAnimation.PlayReverse(this->AsShared());

	// We intentionally do not clear data immediately. This avoids flicker while the animation is still visible.
	// Data can be cleared lazily if needed, but keeping it enables re-open without re-fetching.
}

void SWorldBLDAssetDetailPanel::CheckScrollPosition(float ScrollOffset)
{
	// User experience rule: if the user starts exploring the grid, we get the detail panel out of the way.
	static constexpr float CollapseThreshold = 100.0f;
	if (bIsExpanded && ScrollOffset > CollapseThreshold)
	{
		Collapse();
	}
}

void SWorldBLDAssetDetailPanel::HandleAssetDetailsFetched(const FWorldBLDAssetInfo& Asset)
{
	if (bIsKit)
	{
		return;
	}
	if (!CurrentAsset.ID.IsEmpty() && Asset.ID == CurrentAsset.ID)
	{
		CurrentAsset = Asset;
		bDependencyCacheValid = false; // Invalidate cache when asset details are fetched
		BeginLoadPreviewImages(CurrentAsset.PreviewImages);
		EvaluateToolRequirements(/*bShowDialogIfUnmet=*/true);
	}
}

void SWorldBLDAssetDetailPanel::HandleKitDetailsFetched(const FWorldBLDKitInfo& Kit)
{
	if (!bIsKit)
	{
		return;
	}
	if (!CurrentKit.ID.IsEmpty() && Kit.ID == CurrentKit.ID)
	{
		CurrentKit = Kit;
		BeginLoadPreviewImages(CurrentKit.PreviewImages);
	}
}

void SWorldBLDAssetDetailPanel::HandlePurchaseComplete(bool bSuccess, int32 RemainingCredits)
{
	(void)RemainingCredits;

	// Always refresh credits from the server after a successful purchase, using the same session endpoint
	// used when constructing the widget (do not rely on "remaining credits" responses).
	if (bSuccess)
	{
		RequestCreditsRefreshFromServer(/*bForce=*/true);
	}
}

void SWorldBLDAssetDetailPanel::HandleImportComplete(const FString& AssetId, bool bSuccess)
{
	if (!bSuccess)
	{
		return;
	}

	// Only react if the import corresponds to the item currently shown in this panel.
	if (AssetId.IsEmpty() || AssetId != GetCurrentAssetId())
	{
		return;
	}

	// Invalidate dependency cache after import completes
	bDependencyCacheValid = false;

	// Sync the Content Browser to the imported content so the user can immediately find it.
	// Note: the download manager imports into a sanitized folder like "<SanitizedAssetName>_<AssetId>",
	// so we must query the actual imported package root from the manager (not assume it's just <AssetId>).
	FString ImportedPackageRoot;
	if (!DownloadManager.IsValid() || !DownloadManager->GetImportedPackageRoot(AssetId, ImportedPackageRoot))
	{
		return;
	}

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	IContentBrowserSingleton& ContentBrowser = ContentBrowserModule.Get();

	// Prefer selecting an actual asset (high-signal UX). Fallback to opening the folder if no assets are found yet.
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> ImportedAssets;
	AssetRegistry.GetAssetsByPath(FName(*ImportedPackageRoot), ImportedAssets, /*bRecursive=*/false);

	if (ImportedAssets.Num() > 0)
	{
		TArray<FAssetData> ToSync;
		ToSync.Add(ImportedAssets[0]);
		ContentBrowser.SyncBrowserToAssets(ToSync);
	}
	else
	{
		ContentBrowser.SyncBrowserToFolders({ ImportedPackageRoot }, /*bAllowLockedBrowsers=*/true, /*bFocusContentBrowser=*/true);
	}
}

FReply SWorldBLDAssetDetailPanel::HandlePurchaseClicked()
{

	if (!AssetLibrarySubsystem.IsValid())
	{
		UE_LOG(LogWorldBLDAssetLibrary, Error, TEXT("[HandlePurchaseClicked] AssetLibrarySubsystem is invalid"));
		return FReply::Handled();
	}

	// Tool gating: keep actions disabled when required tools/versions are missing.
	EvaluateToolRequirements(/*bShowDialogIfUnmet=*/true);
	if (!AreToolRequirementsSatisfied())
	{
		return FReply::Handled();
	}

	// If the user doesn't have enough credits:
	// - If they have an active subscription, route them to buy credits.
	// - Otherwise (including trial / no subscription), route them to choose a plan to avoid confusion.
	if (!HasSufficientCredits())
	{
		static const TCHAR* PurchaseCreditsUrl = TEXT("https://worldbld.com/purchase-credits");
		static const TCHAR* PlansUrl = TEXT("https://www.worldbld.com/plans");

		bPendingCreditsRefreshOnAppFocus = true;
		FPlatformProcess::LaunchURL(HasActiveSubscription() ? PurchaseCreditsUrl : PlansUrl, nullptr, nullptr);
		return FReply::Handled();
	}

	// Get the asset/kit ID for purchase
	FString AssetIdForPurchase = bIsKit ? CurrentKit.ID : CurrentAsset.ID;
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("[HandlePurchaseClicked] bIsKit: %s"), bIsKit ? TEXT("true") : TEXT("false"));
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("[HandlePurchaseClicked] Asset/Kit ID: '%s'"), *AssetIdForPurchase);

	if (AssetIdForPurchase.IsEmpty())
	{
		UE_LOG(LogWorldBLDAssetLibrary, Error, TEXT("[HandlePurchaseClicked] Asset ID is empty, cannot purchase"));
		return FReply::Handled();
	}

	// Minimal confirmation dialog (modal). Keeps UX consistent without introducing new UMG dependencies.
	// We don't include thumbnails yet to keep this implementation self-contained.
	TSharedRef<SWindow> Dialog = SNew(SWindow)
		.Title(FText::FromString(TEXT("Confirm Purchase")))
		.SizingRule(ESizingRule::Autosized)
		.SupportsMaximize(false)
		.SupportsMinimize(false);

	const FText ItemName = GetTitleText();
	const FText PriceText = GetPriceText();

	Dialog->SetContent(
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		.Padding(FMargin(14.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("Purchase %s for %s?"), *ItemName.ToString(), *PriceText.ToString())))
				.AutoWrapText(true)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.0f, 10.0f, 0.0f, 0.0f))
			[
				SNew(STextBlock)
				.Text(GetUserCreditsText())
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.0f, 14.0f, 0.0f, 0.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("Cancel")))
					.OnClicked_Lambda([Dialog]()
					{
						Dialog->RequestDestroyWindow();
						return FReply::Handled();
					})
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("Confirm")))
					.IsEnabled_Lambda([this]() { return HasSufficientCredits(); })
					.OnClicked_Lambda([this, Dialog, AssetIdForPurchase]()
					{
						Dialog->RequestDestroyWindow();
						if (AssetLibrarySubsystem.IsValid())
						{
							// Use asset/kit ID for purchase - Backend expects the MongoDB _id in the asset_id field
							UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("[HandlePurchaseClicked] Calling PurchaseAsset with ID: '%s'"), *AssetIdForPurchase);
							AssetLibrarySubsystem->PurchaseAsset(AssetIdForPurchase);
						}
						return FReply::Handled();
					})
				]
			]
		]
	);

	FSlateApplication::Get().AddModalWindow(Dialog, FSlateApplication::Get().GetActiveTopLevelWindow());
	return FReply::Handled();
}

FReply SWorldBLDAssetDetailPanel::HandleDownloadClicked()
{
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("[HandleDownloadClicked] Download button clicked"));

	if (!DownloadManager.IsValid())
	{
		UE_LOG(LogWorldBLDAssetLibrary, Error, TEXT("[HandleDownloadClicked] DownloadManager is invalid"));
		return FReply::Handled();
	}

	if (!AssetLibrarySubsystem.IsValid())
	{
		UE_LOG(LogWorldBLDAssetLibrary, Error, TEXT("[HandleDownloadClicked] AssetLibrarySubsystem is invalid"));
		return FReply::Handled();
	}

	// Tool gating
	EvaluateToolRequirements(/*bShowDialogIfUnmet=*/true);
	if (!AreToolRequirementsSatisfied())
	{
		return FReply::Handled();
	}

	const FString AssetId = GetCurrentAssetId();
	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("[HandleDownloadClicked] Asset ID: '%s'"), *AssetId);

	if (AssetId.IsEmpty())
	{
		UE_LOG(LogWorldBLDAssetLibrary, Error, TEXT("[HandleDownloadClicked] Asset ID is empty"));
		return FReply::Handled();
	}

	// Dependency checking can be expensive on large assets. Keep it lazy so browsing stays smooth,
	// and only compute when the user explicitly initiates an action.
	RefreshDependencyCache();

	// Request signed download URL from backend
	// The backend will verify ownership and return a temporary download URL
	AssetLibrarySubsystem->RequestDownloadURL(AssetId, bIsKit);

	UE_LOG(LogWorldBLDAssetLibrary, Log, TEXT("[HandleDownloadClicked] Requested download URL from backend"));
	return FReply::Handled();
}

FReply SWorldBLDAssetDetailPanel::HandleImportClicked()
{
	if (!DownloadManager.IsValid())
	{
		return FReply::Handled();
	}

	// Tool gating
	EvaluateToolRequirements(/*bShowDialogIfUnmet=*/true);
	if (!AreToolRequirementsSatisfied())
	{
		return FReply::Handled();
	}

	const FString AppId = GetCurrentAssetId();
	if (AppId.IsEmpty())
	{
		return FReply::Handled();
	}

	// Dependency checking can be expensive on large assets. Keep it lazy so browsing stays smooth,
	// and only compute when the user explicitly initiates an action.
	RefreshDependencyCache();

	DownloadManager->ImportAsset(AppId);
	return FReply::Handled();
}

FReply SWorldBLDAssetDetailPanel::HandleViewAssetClicked()
{
	if (bShowingFeatured || bIsKit)
	{
		return FReply::Handled();
	}

	const FString AppId = GetCurrentAssetId();
	if (AppId.IsEmpty())
	{
		return FReply::Handled();
	}

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	IContentBrowserSingleton& ContentBrowser = ContentBrowserModule.Get();
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	auto TrySyncToPrimaryAssetPath = [&ContentBrowser, &AssetRegistry](const FString& InPath) -> bool
	{
		const FString Trimmed = InPath.TrimStartAndEnd();
		if (Trimmed.IsEmpty())
		{
			return false;
		}

		// Prefer syncing to a concrete asset (selects it and opens the containing folder).
		const FSoftObjectPath SoftPath(Trimmed);
		if (SoftPath.IsValid())
		{
			FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(SoftPath);
			if (!AssetData.IsValid())
			{
				UObject* Loaded = SoftPath.TryLoad();
				if (IsValid(Loaded))
				{
					AssetData = FAssetData(Loaded);
				}
			}

			if (AssetData.IsValid())
			{
				ContentBrowser.SyncBrowserToAssets({ AssetData });
				return true;
			}
		}

		// Fallback: treat as a long package name (or object path with ".") and open its folder.
		FString LongPackageName = Trimmed;
		int32 DotIndex = INDEX_NONE;
		if (LongPackageName.FindChar(TEXT('.'), DotIndex) && DotIndex > 0)
		{
			LongPackageName = LongPackageName.Left(DotIndex);
		}

		if (FPackageName::IsValidLongPackageName(LongPackageName))
		{
			const FString Folder = FPackageName::GetLongPackagePath(LongPackageName);
			if (!Folder.IsEmpty())
			{
				ContentBrowser.SyncBrowserToFolders({ Folder }, /*bAllowLockedBrowsers=*/true, /*bFocusContentBrowser=*/true);
				return true;
			}
		}

		return false;
	};

	// Primary behavior: use the first PrimaryAssets entry from the API.
	if (CurrentAsset.PrimaryAssets.Num() > 0 && TrySyncToPrimaryAssetPath(CurrentAsset.PrimaryAssets[0]))
	{
		return FReply::Handled();
	}

	// Fallback: if we can resolve the imported root, open that folder.
	FString ImportedPackageRoot;
	if (DownloadManager.IsValid() && DownloadManager->GetImportedPackageRoot(AppId, ImportedPackageRoot) && !ImportedPackageRoot.IsEmpty())
	{
		ContentBrowser.SyncBrowserToFolders({ ImportedPackageRoot }, /*bAllowLockedBrowsers=*/true, /*bFocusContentBrowser=*/true);
		return FReply::Handled();
	}

	// Final fallback: try the first manifest dependency package path if present.
	if (CurrentAsset.Manifest.Dependencies.Num() > 0 && TrySyncToPrimaryAssetPath(CurrentAsset.Manifest.Dependencies[0].PackagePath))
	{
		return FReply::Handled();
	}

	return FReply::Handled();
}

FReply SWorldBLDAssetDetailPanel::HandlePrevImage()
{
	if (PreviewImageBrushes.Num() <= 1)
	{
		return FReply::Handled();
	}
	SetPreviewIndex(CurrentPreviewIndex - 1);
	return FReply::Handled();
}

FReply SWorldBLDAssetDetailPanel::HandleNextImage()
{
	if (PreviewImageBrushes.Num() <= 1)
	{
		return FReply::Handled();
	}
	SetPreviewIndex(CurrentPreviewIndex + 1);
	return FReply::Handled();
}

EVisibility SWorldBLDAssetDetailPanel::GetPreviewNavigationVisibility() const
{
	// Hide arrows if the selected item only has one (or zero) preview URL(s).
	// Note: PreviewImageBrushes includes a placeholder brush, so we must not base this on brush count.
	return GetSelectedPreviewImageUrlCount() > 1 ? EVisibility::Visible : EVisibility::Collapsed;
}

int32 SWorldBLDAssetDetailPanel::GetSelectedPreviewImageUrlCount() const
{
	if (bShowingFeatured)
	{
		return 0;
	}
	return bIsKit ? CurrentKit.PreviewImages.Num() : CurrentAsset.PreviewImages.Num();
}

EVisibility SWorldBLDAssetDetailPanel::GetPurchaseButtonVisibility() const
{
	if (bShowingFeatured)
	{
		return EVisibility::Collapsed;
	}

	const FString AppId = GetCurrentAssetId();
	const EWorldBLDAssetStatus Status = GetStatusForSelection(DownloadManager, AppId);
	return Status == EWorldBLDAssetStatus::NotOwned ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SWorldBLDAssetDetailPanel::GetDownloadButtonVisibility() const
{
	if (bShowingFeatured)
	{
		return EVisibility::Collapsed;
	}
	const FString AppId = GetCurrentAssetId();
	const EWorldBLDAssetStatus Status = GetStatusForSelection(DownloadManager, AppId);
	return Status == EWorldBLDAssetStatus::Owned || Status == EWorldBLDAssetStatus::Downloading ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SWorldBLDAssetDetailPanel::GetImportButtonVisibility() const
{
	if (bShowingFeatured)
	{
		return EVisibility::Collapsed;
	}
	const FString AppId = GetCurrentAssetId();
	const EWorldBLDAssetStatus Status = GetStatusForSelection(DownloadManager, AppId);
	return Status == EWorldBLDAssetStatus::Downloaded ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SWorldBLDAssetDetailPanel::GetUninstallAssetVisibility() const
{
	if (bShowingFeatured || bIsKit)
	{
		return EVisibility::Collapsed;
	}

	const FString AppId = GetCurrentAssetId();
	const EWorldBLDAssetStatus Status = GetStatusForSelection(DownloadManager, AppId);

	// Allow uninstall when the asset is imported or downloaded (cache-only).
	if (Status == EWorldBLDAssetStatus::Imported || Status == EWorldBLDAssetStatus::Downloaded)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

bool SWorldBLDAssetDetailPanel::CanUninstallAsset() const
{
	if (bShowingFeatured || bIsKit)
	{
		return false;
	}

	const FString AppId = GetCurrentAssetId();
	if (AppId.IsEmpty() || !DownloadManager.IsValid())
	{
		return false;
	}

	const EWorldBLDAssetStatus Status = GetStatusForSelection(DownloadManager, AppId);
	return Status == EWorldBLDAssetStatus::Imported || Status == EWorldBLDAssetStatus::Downloaded;
}

void SWorldBLDAssetDetailPanel::HandleUninstallAssetClicked()
{
	if (!CanUninstallAsset() || !DownloadManager.IsValid())
	{
		return;
	}

	const FString AppId = GetCurrentAssetId();
	const FText Title = GetTitleText();

	const FText Prompt = FText::Format(
		FText::FromString(TEXT("Uninstall \"{0}\"?\n\nThis will delete:\n- Cached download files for this asset\n- Imported content under /Game/WorldBLD/Assets (if present)\n\nThis does not revoke ownership; you can re-download later.")),
		Title);

	const EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNo, Prompt);
	if (Result != EAppReturnType::Yes)
	{
		return;
	}

	DownloadManager->UninstallAsset(AppId, /*bDeleteCache=*/true, /*bDeleteImportedContent=*/true);
}

FText SWorldBLDAssetDetailPanel::GetTitleText() const
{
	if (bIsKit)
	{
		return FText::FromString(CurrentKit.KitsName);
	}
	return FText::FromString(CurrentAsset.Name);
}

FText SWorldBLDAssetDetailPanel::GetDescriptionText() const
{
	if (bShowingFeatured)
	{
		return FText::FromString(CurrentFeaturedCollection.Description);
	}
	if (bIsKit)
	{
		return FText::FromString(CurrentKit.Description);
	}

	
	if (CurrentAsset.Features.Num() > 0)
	{
		FString BulletList;
		for (const FString& Feature : CurrentAsset.Features)
		{
			const FString Trimmed = Feature.TrimStartAndEnd();
			if (Trimmed.IsEmpty())
			{
				continue;
			}

			if (!BulletList.IsEmpty())
			{
				BulletList += TEXT("\n");
			}
			BulletList += TEXT("- ");
			BulletList += Trimmed;
		}

		if (!BulletList.IsEmpty())
		{
			const FString TypeLabel = ExtractAssetTypeLabelFromFeatures(CurrentAsset.Features);
			const FString Blurb = GetTypeBlurbForAssetTypeLabel(TypeLabel);
			if (!Blurb.IsEmpty())
			{
				return FText::FromString(Blurb + TEXT("\n\n") + BulletList);
			}

			return FText::FromString(BulletList);
		}
	}

	return FText::FromString(CurrentAsset.Description);
}

FText SWorldBLDAssetDetailPanel::GetPriceText() const
{
	if (bShowingFeatured)
	{
		return FText::GetEmpty();
	}

	const int32 PriceCredits = GetCurrentPriceCredits();
	const bool bFree = bIsKit ? CurrentKit.bIsFree : CurrentAsset.bIsFree;
	if (bFree || PriceCredits <= 0)
	{
		return FText::FromString(TEXT("Free"));
	}
	return CreditsToText(PriceCredits);
}

FText SWorldBLDAssetDetailPanel::GetPurchaseButtonText() const
{
	if (!HasSufficientCredits())
	{
		return HasActiveSubscription()
			? FText::FromString(TEXT("Get More Credits")).ToUpper()
			: FText::FromString(TEXT("Get Plan")).ToUpper();
	}

	const FText PriceText = GetPriceText();
	if (PriceText.IsEmpty())
	{
		return FText::FromString(TEXT("Purchase")).ToUpper();
	}

	return FText::Format(FText::FromString(TEXT("Purchase - {0}")), PriceText).ToUpper();
}

bool SWorldBLDAssetDetailPanel::HasActiveSubscription() const
{
	// Subscription endpoint is currently inactive; treat "has an active plan" as "has a tool license".
	// If the user has no paid tool license (or only a trial), we show "Get Plan".
	return AuthSubsystem.IsValid()
		&& (AuthSubsystem->HasLicenseForTool(TEXT("RoadBLD")) || AuthSubsystem->HasLicenseForTool(TEXT("CityBLD")));
}

FText SWorldBLDAssetDetailPanel::GetUserCreditsText() const
{
	if (!AuthSubsystem.IsValid())
	{
		return FText::FromString(TEXT("Your Credits: 0"));
	}
	return FText::FromString(FString::Printf(TEXT("Your Credits: %d"), AuthSubsystem->GetUserCredits()));
}

FSlateColor SWorldBLDAssetDetailPanel::GetStatusColor() const
{
	// Match the description text color (instead of status-specific colors).
	return FSlateColor::UseForeground();
}

FText SWorldBLDAssetDetailPanel::GetStatusText() const
{
	const FText RawStatus = bShowingFeatured
		? FText::FromString(TEXT("Featured"))
		: StatusToText(GetStatusForSelection(DownloadManager, GetCurrentAssetId()));

	return FText::FromString(RawStatus.ToString().ToUpper());
}

EVisibility SWorldBLDAssetDetailPanel::GetStatusTextVisibility() const
{
	if (bShowingFeatured)
	{
		return EVisibility::Visible;
	}

	// Replace "IMPORTED" label with a "View Asset" action button.
	if (!bIsKit)
	{
		const FString AppId = GetCurrentAssetId();
		const EWorldBLDAssetStatus Status = GetStatusForSelection(DownloadManager, AppId);
		if (Status == EWorldBLDAssetStatus::Imported)
		{
			return EVisibility::Collapsed;
		}
	}

	return EVisibility::Visible;
}

EVisibility SWorldBLDAssetDetailPanel::GetViewAssetButtonVisibility() const
{
	if (bShowingFeatured || bIsKit)
	{
		return EVisibility::Collapsed;
	}

	const FString AppId = GetCurrentAssetId();
	const EWorldBLDAssetStatus Status = GetStatusForSelection(DownloadManager, AppId);
	return Status == EWorldBLDAssetStatus::Imported ? EVisibility::Visible : EVisibility::Collapsed;
}

const FSlateBrush* SWorldBLDAssetDetailPanel::GetCurrentPreviewBrush() const
{
	if (PreviewImageBrushes.IsValidIndex(CurrentPreviewIndex) && PreviewImageBrushes[CurrentPreviewIndex].IsValid())
	{
		return PreviewImageBrushes[CurrentPreviewIndex].Get();
	}
	return FWorldBLDAssetLibraryImageLoader::Get().GetPlaceholderBrush().Get();
}

FOptionalSize SWorldBLDAssetDetailPanel::GetAnimatedHeight() const
{
	// We animate a fraction of the *host* height, not the whole window, so the panel behaves properly inside splitters.
	const float Lerp = ExpandCurve.GetLerp();
	const float Target = CachedHostHeight * TargetHeightFraction;
	return FOptionalSize(Lerp * Target);
}

bool SWorldBLDAssetDetailPanel::HasSufficientCredits() const
{
	const int32 PriceCredits = GetCurrentPriceCredits();
	const bool bFree = bIsKit ? CurrentKit.bIsFree : CurrentAsset.bIsFree;
	if (bFree || PriceCredits <= 0)
	{
		return true;
	}

	if (!AuthSubsystem.IsValid())
	{
		return false;
	}

	return AuthSubsystem->GetUserCredits() >= PriceCredits;
}

EVisibility SWorldBLDAssetDetailPanel::GetDownloadProgressVisibility() const
{
	const FString AppId = GetCurrentAssetId();
	if (!DownloadManager.IsValid() || AppId.IsEmpty())
	{
		return EVisibility::Collapsed;
	}
	return DownloadManager->IsDownloadInProgress(AppId) ? EVisibility::Visible : EVisibility::Collapsed;
}

TOptional<float> SWorldBLDAssetDetailPanel::GetDownloadProgressValue() const
{
	const FString AppId = GetCurrentAssetId();
	if (!DownloadManager.IsValid() || AppId.IsEmpty())
	{
		return TOptional<float>();
	}

	const float Progress = DownloadManager->GetDownloadProgress(AppId);
	if (Progress < 0.0f)
	{
		return TOptional<float>();
	}
	return FMath::Clamp(Progress, 0.0f, 1.0f);
}

int32 SWorldBLDAssetDetailPanel::GetCurrentPriceCredits() const
{
	return bIsKit ? CurrentKit.Price : CurrentAsset.CreditsPrice;
}

FString SWorldBLDAssetDetailPanel::GetCurrentAssetId() const
{
	return bIsKit ? CurrentKit.ID : CurrentAsset.ID;
}

void SWorldBLDAssetDetailPanel::BeginLoadPreviewImages(const TArray<FString>& Urls)
{
	// Increment to invalidate any in-flight requests from previous selections.
	++PreviewImagesLoadGeneration;
	const int32 ThisGeneration = PreviewImagesLoadGeneration;

	// Always start with a placeholder so the UI has a stable brush.
	PreviewImageBrushes.Reset();
	PreviewImageBrushes.Add(FWorldBLDAssetLibraryImageLoader::Get().GetPlaceholderBrush());
	CurrentPreviewIndex = 0;

	if (Urls.Num() <= 0)
	{
		return;
	}

	// We load images asynchronously and append them as they arrive. We keep the placeholder at index 0
	// so that even if the first request fails, we still show something meaningful.
	const TWeakPtr<SWorldBLDAssetDetailPanel> WeakDetailPanel = StaticCastSharedRef<SWorldBLDAssetDetailPanel>(AsShared());
	for (const FString& Url : Urls)
	{
		FWorldBLDAssetLibraryImageLoader::Get().LoadImageFromURL(Url, [WeakDetailPanel, ThisGeneration](TSharedPtr<FSlateDynamicImageBrush> Brush)
		{
			const TSharedPtr<SWorldBLDAssetDetailPanel> Pinned = WeakDetailPanel.Pin();
			if (!Pinned.IsValid() || Pinned->PreviewImagesLoadGeneration != ThisGeneration)
			{
				return;
			}

			if (!Brush.IsValid())
			{
				return;
			}

			Pinned->PreviewImageBrushes.Add(Brush);

			// If we're still showing the placeholder (index 0), switch to the first loaded real image.
			if (Pinned->CurrentPreviewIndex == 0 && Pinned->PreviewImageBrushes.Num() > 1)
			{
				Pinned->SetPreviewIndex(1);
			}
		});
	}
}

void SWorldBLDAssetDetailPanel::SetPreviewIndex(int32 NewIndex)
{
	if (PreviewImageBrushes.Num() <= 0)
	{
		CurrentPreviewIndex = 0;
		return;
	}

	// Wrap around for carousel behavior.
	const int32 Count = PreviewImageBrushes.Num();
	int32 Wrapped = NewIndex % Count;
	if (Wrapped < 0)
	{
		Wrapped += Count;
	}
	CurrentPreviewIndex = Wrapped;
}

FText SWorldBLDAssetDetailPanel::GetDependencyInfoText() const
{
	if (bIsKit || CurrentAsset.Manifest.Dependencies.Num() == 0)
	{
		return FText::GetEmpty();
	}

	const int32 TotalDeps = CurrentAsset.Manifest.Dependencies.Num();

	if (!DownloadManager.IsValid() || !bDependencyCacheValid)
	{
		return FText::FromString(FString::Printf(TEXT("Dependencies: %d (not checked)"), TotalDeps));
	}

	// Use cached values instead of calling CheckLocalDependencies every frame
	if (CachedMissingDepsCount == 0)
	{
		return FText::FromString(FString::Printf(TEXT("Dependencies: %d (all available locally)"), TotalDeps));
	}
	else if (CachedExistingDepsCount == 0)
	{
		return FText::FromString(FString::Printf(TEXT("Dependencies: %d (none available locally)"), TotalDeps));
	}
	else
	{
		return FText::FromString(FString::Printf(TEXT("Dependencies: %d (%d available, %d missing)"),
			TotalDeps, CachedExistingDepsCount, CachedMissingDepsCount));
	}
}

EVisibility SWorldBLDAssetDetailPanel::GetDependencyInfoVisibility() const
{
	// TODO(WorldBLD): Make the dependency info text optional (e.g., a user setting / toggle in the Asset Library UI).
	// For now, we hide this line entirely to reduce visual noise in the details panel.
	return EVisibility::Collapsed;
}

void SWorldBLDAssetDetailPanel::RefreshDependencyCache()
{
	// Avoid repeated work if we've already computed the cache for the current selection.
	if (bDependencyCacheValid)
	{
		return;
	}

	// Skip if this is a kit or no dependencies
	if (bIsKit || CurrentAsset.Manifest.Dependencies.Num() == 0 || !DownloadManager.IsValid())
	{
		bDependencyCacheValid = false;
		CachedMissingDepsCount = 0;
		CachedExistingDepsCount = 0;
		return;
	}

	// Perform the actual check and cache the results
	TArray<FWorldBLDAssetDependency> MissingDeps;
	TArray<FWorldBLDAssetDependency> ExistingDeps;
	DownloadManager->CheckLocalDependencies(CurrentAsset, MissingDeps, ExistingDeps);

	CachedMissingDepsCount = MissingDeps.Num();
	CachedExistingDepsCount = ExistingDeps.Num();
	bDependencyCacheValid = true;
}

bool SWorldBLDAssetDetailPanel::AreToolRequirementsSatisfied()
{
	// Kits currently have no requiredTools data in the asset schema. Only gate assets.
	if (bIsKit || bShowingFeatured)
	{
		return true;
	}

	// Keep this lazy so if the tool-name mapping becomes available after the panel is shown
	// (e.g., licenses/tools list arrives), the buttons will re-enable without requiring a re-select.
	EvaluateToolRequirements(/*bShowDialogIfUnmet=*/false);
	return bToolRequirementsSatisfied;
}

void SWorldBLDAssetDetailPanel::EvaluateToolRequirements(bool bShowDialogIfUnmet)
{
	// Default: allow actions.
	bToolRequirementsSatisfied = true;

	if (bIsKit || bShowingFeatured)
	{
		return;
	}

	if (CurrentAsset.RequiredTools.Num() <= 0)
	{
		return;
	}

	const FString AssetId = GetCurrentAssetId();

	// Find the first failing requirement (we keep UX simple and focused).
	const TArray<FString> AuthorizedTools = AuthSubsystem.IsValid() ? AuthSubsystem->GetAuthorizedTools() : TArray<FString>();
	for (const FWorldBLDRequiredTool& Req : CurrentAsset.RequiredTools)
	{
		const FString ToolId = Req.ToolId.TrimStartAndEnd();
		if (ToolId.IsEmpty())
		{
			continue;
		}

		const FString ToolName = ResolveToolNameFromToolId(ToolId, AuthorizedTools.Num() > 0 ? &AuthorizedTools : nullptr);
		const FString MinVersion = Req.MinVersion.TrimStartAndEnd();

		// 1) Plugin must be mounted/enabled
		// WorldBLD is the host tool for this UI; treat it as always present and only enforce version.
		if (!ToolName.Equals(TEXT("WorldBLD"), ESearchCase::IgnoreCase) && !IsPluginMountedAndEnabled(ToolName))
		{
			bToolRequirementsSatisfied = false;

			if (bShowDialogIfUnmet && (AssetId != LastToolRequirementDialogAssetId || LastToolRequirementDialogReason != TEXT("missing")))
			{
				LastToolRequirementDialogAssetId = AssetId;
				LastToolRequirementDialogReason = TEXT("missing");

				const FString Message = FString::Printf(
					TEXT("This asset requires the %s tool. Please download %s first."),
					*ToolName, *ToolName);
				FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Message));
			}
			return;
		}

		// 2) Version must be >= required minVersion
		if (!MinVersion.IsEmpty())
		{
			const FString InstalledVersion = GetPluginVersionString(ToolName);
			const int32 InstalledInt = ParseSemVerToSortableInt(InstalledVersion);
			const int32 RequiredInt = ParseSemVerToSortableInt(MinVersion);

			// If we can parse required version, enforce it. If parsing fails (0), skip enforcing.
			if (RequiredInt > 0 && InstalledInt < RequiredInt)
			{
				bToolRequirementsSatisfied = false;

				if (bShowDialogIfUnmet && (AssetId != LastToolRequirementDialogAssetId || LastToolRequirementDialogReason != TEXT("outdated")))
				{
					LastToolRequirementDialogAssetId = AssetId;
					LastToolRequirementDialogReason = TEXT("outdated");

					const FString Message = FString::Printf(
						TEXT("This asset requires a newer version of %s. Please update %s first."),
						*ToolName, *ToolName);
					FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Message));
				}
				return;
			}
		}
	}
}
