// Copyright WorldBLD LLC. All rights reserved.


#include "Utilities/UtilitiesLibrary.h"

#include "WorldBLDEditorModule.h"
#include "Dialogs/Dialogs.h"
#include "Misc/MessageDialog.h"

UUtilsEditorContext_Notification* UUtilitiesLibrary::GetNotificationContext(
	const FS_UtilsNotificationInfo& Info, const bool ShowNotification, UObject* InGraphObject)
{	
		const FWorldBLDEditorModule& WorldBldEditorModule = FModuleManager::Get().LoadModuleChecked<FWorldBLDEditorModule>(TEXT("FWorldBLDEditorModule"));
		return WorldBldEditorModule.GetContextManager().GetNotificationContext(Info, InGraphObject, ShowNotification);
}

#define LOCTEXT_NAMESPACE "WorldBLDEditorToolkit"

void UUtilitiesLibrary::ShowQuickNotification(const FText& Text, const FS_UtilsNotificationIcon& Icon,
	EUtilsBPNotificationCompletionState State, float Duration, const FS_UtilsNotificationButtons& Buttons,
	UObject* InGraphObject)
{
	FS_UtilsNotificationInfo Info;
	Info.Text = Text;
	Info.ExpireDuration = Duration;
	Info.Buttons = Buttons;

	if (Icon.Texture != nullptr)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(Icon.Texture);
		Brush.ImageSize = Icon.Size;
		Info.Image = Brush;
	}

	UUtilsEditorContext_Notification* Notification = GetNotificationContext(Info, true, InGraphObject);
	Notification->SetCompletionState(State);
}

void UUtilitiesLibrary::ShowOkModalDialog(const FText& Message, const FText& Title)
{
#if WITH_EDITOR
	const FText EffectiveTitle = Title.IsEmpty() ? LOCTEXT("WorldBLD_DefaultOkDialogTitle", "WorldBLD") : Title;
	FMessageDialog::Open(EAppMsgType::Ok, Message, EffectiveTitle);
#endif
}

void UUtilitiesLibrary::OpenExternalUrlLinkInWebBrowser(const FString& Url)
{
	if (Url.StartsWith(TEXT("http")) || Url.StartsWith(TEXT("https")))
	{
		const FText Message = LOCTEXT("OpeningURLMessage", "You are about to open an external URL. This will open your web browser. Do you want to proceed?");
		const FText URLDialog = LOCTEXT("OpeningURLTitle", "Open external link");

		FSuppressableWarningDialog::FSetupInfo Info(Message, URLDialog, "SuppressOpenURLWarning");
		Info.ConfirmText = LOCTEXT("OpenURL_yes", "Yes");
		Info.CancelText = LOCTEXT("OpenURL_no", "No");
		const FSuppressableWarningDialog OpenURLWarning(Info);
		if (OpenURLWarning.ShowModal() != FSuppressableWarningDialog::Cancel)
		{
			FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
		}
	}
}

#undef LOCTEXT_NAMESPACE
