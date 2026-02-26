// Copyright WorldBLD LLC. All rights reserved.

#include "ToolsMenu/SWorldBLDToolSwitchMenu.h"

#include "WorldBLDEditorToolkit.h"
#include "WorldBLDToolPaletteWidget.h"

#include "ContextMenu/WorldBLDContextMenuStyle.h"
#include "Downloader/CurlDownloaderLibrary.h"
#include "Utilities/UtilitiesLibrary.h"

#include "Interfaces/IPluginManager.h"
#include "Math/Vector4.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SThrobber.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "WorldBLDToolSwitchMenu"

namespace WorldBLDToolSwitchMenuInternal
{
	static bool IsPluginMounted(const FString& PluginName)
	{
		if (PluginName.IsEmpty())
		{
			return false;
		}

		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(PluginName);
		return Plugin.IsValid() && Plugin->IsMounted();
	}

	static FSlateFontInfo GetToolNameFont()
	{
		FSlateFontInfo Font = FAppStyle::GetFontStyle(TEXT("NormalFont"));
		Font.Size = 12;
		return Font;
	}

	static FText GetStatusText(
		const UWorldBLDEdMode* InEdMode,
		const UCurlDownloaderLibrary* InDownloader,
		const FString& ToolId,
		const FString& ServerVersion,
		const FString& ServerSHA1)
	{
		if ((InDownloader != nullptr) && InDownloader->IsDownloadInProgress(ToolId))
		{
			const float Progress = InDownloader->GetDownloadProgress(ToolId);
			// Some installs reach 100% before the editor has had a chance to fully load the plugin.
			// At that point, a restart is required for the tool to become available, so reflect that in the UI.
			if (Progress >= 0.999f)
			{
				// Safety: only show the restart prompt once the plugin has actually been mounted/discovered.
				// This avoids encouraging a restart while the installer is still finalizing (eg unzipping/mounting).
				if (IsPluginMounted(ToolId))
				{
					return LOCTEXT("Status_InstalledRestartUnreal", "Installed: Restart Unreal");
				}

				return LOCTEXT("Status_InstallingFinalizing", "Installing… 100%");
			}
			if (Progress >= 0.0f)
			{
				return FText::Format(LOCTEXT("Status_InstallingPct", "Installing… {0}%"), FText::AsNumber(FMath::RoundToInt(Progress * 100.0f)));
			}
			return LOCTEXT("Status_Installing", "Installing…");
		}

		// Check for a staged update that is waiting to be applied on the next restart.
		if (!ToolId.IsEmpty() && UCurlDownloaderLibrary::HasPendingStagedUpdate(ToolId))
		{
			return LOCTEXT("Status_StagedUpdateReady", "Update ready: Restart to apply");
		}

		if ((InEdMode != nullptr) && !ToolId.IsEmpty())
		{
			switch (InEdMode->GetToolUpdateState(ToolId, ServerVersion, ServerSHA1))
			{
			case EWorldBLDToolUpdateState::NotInstalled:
				return LOCTEXT("Status_NotInstalled", "Not installed");
			case EWorldBLDToolUpdateState::InstalledUnknownSha1:
				return LOCTEXT("Status_Installed", "Installed");
			case EWorldBLDToolUpdateState::UpToDate:
				return LOCTEXT("Status_UpToDate", "Installed");
			case EWorldBLDToolUpdateState::UpdateAvailable:
				return LOCTEXT("Status_UpdateAvailable", "Update available");
			default:
				break;
			}
		}

		return FText::GetEmpty();
	}

	static bool ShouldShowInstallButton(
		const UWorldBLDEdMode* InEdMode,
		const UCurlDownloaderLibrary* InDownloader,
		const FString& ToolId,
		const FString& ServerVersion,
		const FString& ServerSHA1)
	{
		if (ToolId.IsEmpty())
		{
			return false;
		}

		if ((InDownloader != nullptr) && InDownloader->IsDownloadInProgress(ToolId))
		{
			return false;
		}

		// If a staged update is already pending, don't show the install/update button.
		if (UCurlDownloaderLibrary::HasPendingStagedUpdate(ToolId))
		{
			return false;
		}

		if (InEdMode == nullptr)
		{
			return false;
		}

		const EWorldBLDToolUpdateState State = InEdMode->GetToolUpdateState(ToolId, ServerVersion, ServerSHA1);
		return (State == EWorldBLDToolUpdateState::NotInstalled) || (State == EWorldBLDToolUpdateState::UpdateAvailable);
	}

	static FText GetInstallButtonText(const UWorldBLDEdMode* InEdMode, const FString& ToolId, const FString& ServerVersion, const FString& ServerSHA1)
	{
		if ((InEdMode != nullptr) && (InEdMode->GetToolUpdateState(ToolId, ServerVersion, ServerSHA1) == EWorldBLDToolUpdateState::UpdateAvailable))
		{
			return LOCTEXT("UpdateButton", "Update");
		}
		return LOCTEXT("InstallButton", "Install");
	}
}

void SWorldBLDToolSwitchMenu::Construct(const FArguments& InArgs)
{
	EdMode = InArgs._EdMode;
	Downloader = InArgs._Downloader;

	MenuContainer.Reset();
	CachedAvailableToolsHash = 0;

	ChildSlot
	[
		SNew(SBorder)
		.Padding(FMargin(10.0f))
		.BorderImage([]() -> const FSlateBrush*
		{
			// Corner order: TopLeft, TopRight, BottomRight, BottomLeft.
			static const FVector4 CornerRadii(25.0f, 25.0f, 25.0f, 25.0f);

			// Use a RoundedBox brush so corners are actually rounded (not just a tinted box brush).
			// This menu is intended to be readable over the viewport, so keep it slightly translucent.
			static const FSlateRoundedBoxBrush Brush(FLinearColor(0.011f, 0.011f, 0.011f, 0.95f), CornerRadii);
			return &Brush;
		}())
		.BorderBackgroundColor(FLinearColor::White)
		[
			// Constrain the menu width so long descriptions wrap instead of expanding the whole menu sideways.
			SNew(SBox)
			.MaxDesiredWidth(350.0f)
			[
				BuildMenuContent()
			]
		]
	];
}

TSharedRef<SWidget> SWorldBLDToolSwitchMenu::BuildMenuContent()
{
	SAssignNew(MenuContainer, SVerticalBox);
	RebuildMenuEntries();
	return MenuContainer.ToSharedRef();
}

void SWorldBLDToolSwitchMenu::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// The server tool list is fetched asynchronously (HTTP). This widget used to snapshot AvailableTools only once
	// during Construct(), which meant the first open would often show only the fallback "WorldBLD" entry.
	// While the menu is open, keep it in sync whenever AvailableTools changes.
	const uint32 CurrentHash = ComputeAvailableToolsHash();
	if (CurrentHash != CachedAvailableToolsHash)
	{
		RebuildMenuEntries();
	}
}

uint32 SWorldBLDToolSwitchMenu::ComputeAvailableToolsHash() const
{
	const UWorldBLDEdMode* EdModePtr = EdMode.Get();
	if (EdModePtr == nullptr)
	{
		return 0;
	}

	uint32 Hash = 0;
	for (const FPluginToolData& ToolData : EdModePtr->AvailableTools)
	{
		Hash = HashCombineFast(Hash, GetTypeHash(ToolData.ToolName));
		Hash = HashCombineFast(Hash, GetTypeHash(ToolData.Description));
		Hash = HashCombineFast(Hash, GetTypeHash(ToolData.DownloadLink));
		Hash = HashCombineFast(Hash, GetTypeHash(ToolData.Version));
		Hash = HashCombineFast(Hash, GetTypeHash(ToolData.SHA1));
	}
	return Hash;
}

void SWorldBLDToolSwitchMenu::RebuildMenuEntries()
{
	if (!MenuContainer.IsValid())
	{
		return;
	}

	MenuContainer->ClearChildren();

	const UWorldBLDEdMode* EdModePtr = EdMode.Get();
	const TArray<FPluginToolData> Tools = EdModePtr ? EdModePtr->AvailableTools : TArray<FPluginToolData>();
	for (const FPluginToolData& ToolData : Tools)
	{
		const FString ToolId = NormalizeToolId(ToolData.ToolName);
		if (ToolId.IsEmpty())
		{
			continue;
		}

		MenuContainer->AddSlot()
		.AutoHeight()
		.Padding(FMargin(0.0f, 0.0f, 0.0f, 6.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				// Main tool-switch button: disabled when the tool isn't installed (or while installing).
				// Install/Update controls are intentionally NOT inside this button, so they remain usable.
				SNew(SButton)
				.ButtonStyle(&WorldBLDContextMenuStyle::GetToolSwitchMenuRowButtonStyle())
				.IsEnabled_Lambda([this, ToolId]() { return IsToolInstalled(ToolId) && !IsToolInstalling(ToolId); })
				.OnClicked_Lambda([this, ToolId]() { return HandleToolRowClicked(ToolId); })
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
							.Text(FText::FromString(ToolData.ToolName))
							.Font(WorldBLDToolSwitchMenuInternal::GetToolNameFont())
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(FMargin(0.0f, 2.0f, 0.0f, 0.0f))
						[
							SNew(STextBlock)
							.Text(FText::FromString(ToolData.Description))
							.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.75f)))
							.AutoWrapText(true)
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Right)
					.Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
					[
						SNew(STextBlock)
						.Text_Lambda([this, ToolId, ServerVersion = ToolData.Version, ServerSHA1 = ToolData.SHA1]()
						{
							return WorldBLDToolSwitchMenuInternal::GetStatusText(EdMode.Get(), Downloader.Get(), ToolId, ServerVersion, ServerSHA1);
						})
						.Justification(ETextJustify::Right)
						.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.85f)))
					]
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			.Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
			[
				// Wrap in a widget that supports Visibility attributes; slots don't in UE5.6/5.7.
				SNew(SBox)
				.Visibility_Lambda([this, ToolId, ServerVersion = ToolData.Version, ServerSHA1 = ToolData.SHA1]()
				{
					// Only reserve space for the install/update controls when they are actually needed.
					// Otherwise, the row background/button appears slightly narrower than other entries.
					const bool bShowInstall =
						WorldBLDToolSwitchMenuInternal::ShouldShowInstallButton(EdMode.Get(), Downloader.Get(), ToolId, ServerVersion, ServerSHA1);
					const bool bShowDownloadSpinner = IsToolInstalling(ToolId);
					return (bShowInstall || bShowDownloadSpinner) ? EVisibility::Visible : EVisibility::Collapsed;
				})
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SButton)
						.ButtonStyle(&WorldBLDContextMenuStyle::GetToolSwitchMenuInstallButtonStyle())
						.Visibility_Lambda([this, ToolId, ServerVersion = ToolData.Version, ServerSHA1 = ToolData.SHA1]()
						{
							return WorldBLDToolSwitchMenuInternal::ShouldShowInstallButton(EdMode.Get(), Downloader.Get(), ToolId, ServerVersion, ServerSHA1)
								? EVisibility::Visible
								: EVisibility::Collapsed;
						})
						.Text_Lambda([this, ToolId, ServerVersion = ToolData.Version, ServerSHA1 = ToolData.SHA1]()
						{
							return WorldBLDToolSwitchMenuInternal::GetInstallButtonText(EdMode.Get(), ToolId, ServerVersion, ServerSHA1);
						})
						.OnClicked_Lambda([
							this,
							ToolId,
							ToolDisplayName = ToolData.ToolName,
							ServerVersion = ToolData.Version,
							ServerSHA1 = ToolData.SHA1,
							Url = ToolData.DownloadLink,
							Sha1 = ToolData.SHA1]()
						{
							return HandleInstallClicked(ToolId, ToolDisplayName, ServerVersion, Url, ServerSHA1);
						})
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(FMargin(8.0f, 0.0f, 0.0f, 0.0f))
					[
						SNew(SBox)
						.WidthOverride(16.0f)
						.HeightOverride(16.0f)
						[
							SNew(SCircularThrobber)
							.Visibility_Lambda([this, ToolId]()
							{
								const UCurlDownloaderLibrary* DownloaderPtr = Downloader.Get();
								return (DownloaderPtr != nullptr) && DownloaderPtr->IsDownloadInProgress(ToolId)
									? EVisibility::Visible
									: EVisibility::Collapsed;
							})
						]
					]
				]
			]
		];
	}

	// Final "WorldBLD" entry (always present).
	MenuContainer->AddSlot()
	.AutoHeight()
	.Padding(FMargin(0.0f, 0.0f, 0.0f, 6.0f))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SButton)
			.ButtonStyle(&WorldBLDContextMenuStyle::GetToolSwitchMenuRowButtonStyle())
			.OnClicked(this, &SWorldBLDToolSwitchMenu::HandleSwitchToWorldBLDClicked)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("WorldBLDEntry", "WorldBLD"))
						.Font(WorldBLDToolSwitchMenuInternal::GetToolNameFont())
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FMargin(0.0f, 2.0f, 0.0f, 0.0f))
					[
						SNew(STextBlock)
						.Text(LOCTEXT("WorldBLDEntryDescription", "Return to the WorldBLD menu."))
						.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.75f)))
						.AutoWrapText(true)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				.Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
				[
					SNew(STextBlock)
					.Text(LOCTEXT("WorldBLDEntryStatus", "Installed"))
					.Justification(ETextJustify::Right)
					.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.85f)))
				]
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Right)
		.Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
		[
			SNew(SBox)
			.Visibility(EVisibility::Collapsed)
		]
	];

	CachedAvailableToolsHash = ComputeAvailableToolsHash();
}

/*
 * NOTE:
 * The tool entries are now built in RebuildMenuEntries(). The old BuildMenuContent() implementation was a one-time snapshot of
 * UWorldBLDEdMode::AvailableTools, which frequently ran before the async fetch completed.
 */

FString SWorldBLDToolSwitchMenu::NormalizeToolId(const FString& ToolName)
{
	return ToolName.TrimStartAndEnd();
}

bool SWorldBLDToolSwitchMenu::IsToolInstalled(const FString& ToolId) const
{
	return !ToolId.IsEmpty() && IPluginManager::Get().FindPlugin(ToolId).IsValid();
}

bool SWorldBLDToolSwitchMenu::IsToolInstalling(const FString& ToolId) const
{
	const UCurlDownloaderLibrary* DownloaderPtr = Downloader.Get();
	return (DownloaderPtr != nullptr) && !ToolId.IsEmpty() && DownloaderPtr->IsDownloadInProgress(ToolId);
}

FReply SWorldBLDToolSwitchMenu::HandleToolRowClicked(FString ToolId) const
{
	if (ToolId.IsEmpty())
	{
		return FReply::Handled();
	}

	UWorldBLDEdMode* EdModePtr = EdMode.Get();
	if (!IsValid(EdModePtr))
	{
		return FReply::Handled();
	}

	const TArray<UWorldBLDToolPaletteWidget*> Palettes = EdModePtr->GetAvailableToolPalettes();
	for (UWorldBLDToolPaletteWidget* Palette : Palettes)
	{
		if (IsValid(Palette) && Palette->ToolId == FName(*ToolId))
		{
			EdModePtr->SwitchToToolPalette(Palette);
			return FReply::Handled();
		}
	}

	// Palette likely not available until restart (code plugin) or tool not present.
	return FReply::Handled();
}

FReply SWorldBLDToolSwitchMenu::HandleInstallClicked(
	FString ToolId,
	FString ToolDisplayName,
	FString ServerVersion,
	FString DownloadUrl,
	FString ExpectedSha1)
{
	if (ToolId.IsEmpty())
	{
		return FReply::Handled();
	}

	UWorldBLDEdMode* EdModePtr = EdMode.Get();
	UCurlDownloaderLibrary* DownloaderPtr = Downloader.Get();
	if (!IsValid(EdModePtr) || !IsValid(DownloaderPtr))
	{
		return FReply::Handled();
	}

	// Safety: allow installs, but block tool *updates* if the user isn't on the latest WorldBLD plugin.
	const bool bIsUpdate = (EdModePtr->GetToolUpdateState(ToolId, ServerVersion, ExpectedSha1) == EWorldBLDToolUpdateState::UpdateAvailable);
	if (bIsUpdate && !EdModePtr->IsWorldBLDPluginUpToDate())
	{
		const FText Title = LOCTEXT("CannotUpdateTool_Title", "Unable to update tool");
		const FText Message = FText::Format(
			LOCTEXT(
				"CannotUpdateTool_Message",
				"We're not able to update {0} right now because you do not have the latest version of WorldBLD. "
				"Please open the Epic Games Launcher and update the WorldBLD plugin in your library before updating {0}."),
			FText::FromString(ToolDisplayName.IsEmpty() ? ToolId : ToolDisplayName));

		UUtilitiesLibrary::ShowOkModalDialog(Message, Title);
		return FReply::Handled();
	}

	const TWeakObjectPtr<UCurlDownloaderLibrary> DownloaderWeak = DownloaderPtr;
	DownloaderPtr->RequestToolDownloadNative(
		ToolId,
		DownloadUrl,
		ExpectedSha1,
		FOnDownloadProgress::FDelegate::CreateLambda([](float) {}),
		FOnComplete::FDelegate::CreateLambda([ToolId, DownloaderWeak](bool bSuccess)
		{
			if (!bSuccess)
			{
				return;
			}

			// Check whether this was a staged update (file swap deferred to next startup).
			const bool bWasStaged = UCurlDownloaderLibrary::HasPendingStagedUpdate(ToolId);

			if (!bWasStaged)
			{
				// Non-staged: ensure the tool plugin is re-enabled in the project file after a successful update/install.
				// (The downloader attempts this too, but we want the menu flow to guarantee it.)
				if (DownloaderWeak.IsValid())
				{
					FString EnableError;
					if (!DownloaderWeak->SetPluginEnabledInProjectFileNative(ToolId, true, EnableError))
					{
						UUtilitiesLibrary::ShowOkModalDialog(FText::FromString(EnableError), LOCTEXT("EnablePluginFailed_Menu_Title", "Plugin enable failed"));
						return;
					}
				}
			}

			if (bWasStaged)
			{
				const FText Title = LOCTEXT("StagedUpdateReadyTitle", "Update Downloaded");
				const FText Message = FText::Format(
					LOCTEXT("StagedUpdateReadyMessage",
						"The update for '{0}' has been downloaded successfully.\n\n"
						"Restart the editor to apply the update. Your levels and project will not be affected."),
					FText::FromString(ToolId));
				UUtilitiesLibrary::ShowOkModalDialog(Message, Title);
			}
			else
			{
				const FText Title = LOCTEXT("RestartRequiredTitle", "Restart Required");
				const FText Message = FText::Format(
					LOCTEXT("RestartRequiredMessage", "Tool '{0}' was installed successfully.\n\nPlease restart the editor to ensure it is fully loaded."),
					FText::FromString(ToolId));
				UUtilitiesLibrary::ShowOkModalDialog(Message, Title);
			}
		}));

	return FReply::Handled();
}

FReply SWorldBLDToolSwitchMenu::HandleSwitchToWorldBLDClicked() const
{
	if (UWorldBLDEdMode* EdModePtr = EdMode.Get())
	{
		EdModePtr->SwitchToMainWorldBLDWidget();
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE

