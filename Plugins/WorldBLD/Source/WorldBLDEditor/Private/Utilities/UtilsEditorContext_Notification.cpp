// Copyright WorldBLD LLC. All rights reserved.

#include "Utilities/UtilsEditorContext_Notification.h"

#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"

#pragma  region Setup
UUtilsEditorContext_Notification::~UUtilsEditorContext_Notification()
{
	if (NotificationPtr.IsValid())
	{
		NotificationPtr.Reset();
		NotificationPtr = nullptr;
	}
}

void UUtilsEditorContext_Notification::Setup(const FS_UtilsNotificationInfo& Info, UObject* InGraphObject, const bool ShowNotification)
{	
	NotificationInfo = Info;
	bIsShowing = false;
	bIsFree = false;
	GraphObject = InGraphObject;

	if (ShowNotification)
	{
		Show();
	}
}

void UUtilsEditorContext_Notification::Show()
{
	if (NotificationPtr.IsValid())
	{
		NotificationPtr->SetFadeOutDuration(0.0f);
		NotificationPtr->Fadeout();
		NotificationPtr.Reset();
	}

	bIsShowing = true;
	bIsFree = false;

	FNotificationInfo Info(NotificationInfo.Text);
	Info.FadeInDuration = NotificationInfo.FadeInDuration;
	Info.FadeOutDuration = NotificationInfo.FadeOutDuration;
	Info.ExpireDuration = NotificationInfo.ExpireDuration;
	Info.bUseThrobber = NotificationInfo.bUseThrobber;
	Info.bUseSuccessFailIcons = NotificationInfo.bUseSuccessFailIcons;
	Info.bUseLargeFont = NotificationInfo.bUseLargeFont;

	if (NotificationInfo.WidthOverride != 0.0f)
	{
		Info.WidthOverride = FOptionalSize(NotificationInfo.WidthOverride);
	}

	Info.bFireAndForget = NotificationInfo.bFireAndForget;
	Info.bAllowThrottleWhenFrameRateIsLow = NotificationInfo.bAllowThrottleWhenFrameRateIsLow;

	// only add an image if we have set an texture.
	if (NotificationInfo.Image.GetResourceObject() != nullptr)
	{
		Info.Image = &NotificationInfo.Image;
	}

	// only add the hyperlink if it has text and a valid function name.
	if (!NotificationInfo.HyperlinkText.IsEmpty())
	{
		const FName eventName(*NotificationInfo.HyperlinkEventName);
		if (HasFunctionWithName(eventName))
		{
			Info.HyperlinkText = NotificationInfo.HyperlinkText;
			Info.Hyperlink = FSimpleDelegate::CreateUFunction(GraphObject, eventName);
		}
	}

	// Need to have a valid object to work with...
	if (GraphObject != nullptr)
	{
		for (FS_UtilsNotificationButtonInfo buttonInfo : NotificationInfo.Buttons.Buttons)
		{
			if (buttonInfo.EventName == "") continue;
			const FName EventName(*buttonInfo.EventName);
			if (HasFunctionWithName(EventName))
			{
				FNotificationButtonInfo NotificationButton = FNotificationButtonInfo(
					FText::FromString(buttonInfo.Text),
					FText::FromString(buttonInfo.ToolTip),
					FSimpleDelegate::CreateWeakLambda(this, [this, buttonInfo]() { HandleOnButtonClicked(buttonInfo); })
				);

				NotificationButton.VisibilityOnNone = buttonInfo.VisibilityOnNone ? EVisibility::Visible : EVisibility::Collapsed;
				NotificationButton.VisibilityOnPending = buttonInfo.VisibilityOnPending ? EVisibility::Visible : EVisibility::Collapsed;
				NotificationButton.VisibilityOnSuccess = buttonInfo.VisibilityOnSuccess ? EVisibility::Visible : EVisibility::Collapsed;
				NotificationButton.VisibilityOnFail = buttonInfo.VisibilityOnFail ? EVisibility::Visible : EVisibility::Collapsed;

				Info.ButtonDetails.Add(NotificationButton);
			}
		}
	}

	NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
	NotificationPtr->SetCompletionState(SNotificationItem::CS_None);
}

void UUtilsEditorContext_Notification::Close(const FText Text)
{
	SetText(Text);
	if (NotificationPtr.IsValid())
	{
		NotificationPtr->ExpireAndFadeout();
	}
}

void UUtilsEditorContext_Notification::CloseAsSuccess(const FText Text)
{
	SetCompleteStateSuccess();
	Close(Text);
}

void UUtilsEditorContext_Notification::CloseAsFail(const FText Text)
{
	SetCompleteStateFail();
	Close(Text);
}

#pragma endregion

#pragma region NotificationItem

void UUtilsEditorContext_Notification::ExpireAndFadeout()
{
	if (!NotificationPtr.IsValid()) return;
	NotificationPtr->ExpireAndFadeout();
	bIsShowing = false;
}

void UUtilsEditorContext_Notification::Fadeout()
{
	if (!NotificationPtr.IsValid()) return;
	NotificationPtr->Fadeout();
	bIsShowing = false;
}

void UUtilsEditorContext_Notification::SetCompletionState(EUtilsBPNotificationCompletionState State)
{
	if (!NotificationPtr.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Notification:: ptr is not valid here..."));
		return;
	}

	if (State == EUtilsBPNotificationCompletionState::Fail)
	{
		NotificationPtr->SetCompletionState(SNotificationItem::CS_Fail);
	}
	else if (State == EUtilsBPNotificationCompletionState::Success)
	{
		NotificationPtr->SetCompletionState(SNotificationItem::CS_Success);
	}
	else if (State == EUtilsBPNotificationCompletionState::Pending)
	{
		NotificationPtr->SetCompletionState(SNotificationItem::CS_Pending);
	}
	else
	{
		NotificationPtr->SetCompletionState(SNotificationItem::CS_None);
	}
}

void UUtilsEditorContext_Notification::SetStateAsPending()
{
	SetCompletionState(EUtilsBPNotificationCompletionState::Pending);
}

void UUtilsEditorContext_Notification::SetCompleteStateFail()
{
	SetCompletionState(EUtilsBPNotificationCompletionState::Fail);
}

void UUtilsEditorContext_Notification::SetCompleteStateSuccess()
{
	SetCompletionState(EUtilsBPNotificationCompletionState::Success);
}

void UUtilsEditorContext_Notification::SetText(const FText NewText)
{
	NotificationInfo.Text = NewText;
	if (NotificationPtr.IsValid())
	{
		NotificationPtr->SetText(NewText);
	}
}

bool UUtilsEditorContext_Notification::IsNotificationValid()
{
	return NotificationPtr.IsValid();
}

void UUtilsEditorContext_Notification::Clear()
{
	if (NotificationPtr.IsValid())
	{
		NotificationPtr->SetFadeOutDuration(0.0f);
		NotificationPtr->Fadeout();
		NotificationPtr.Reset();
	}

	bIsShowing = false;
	NotificationInfo = FS_UtilsNotificationInfo();
	bIsFree = true;
}

void UUtilsEditorContext_Notification::HandleOnButtonClicked(FS_UtilsNotificationButtonInfo ButtonInfo)
{
	const FName FunctionFName(*ButtonInfo.EventName);
	FSimpleDelegate::CreateUFunction(GraphObject, FunctionFName).Execute();

	if (ButtonInfo.CloseOnClick)
	{
		if (NotificationPtr.IsValid())
		{
			NotificationPtr->Fadeout();
		}
	}
}

#pragma endregion

#pragma region Helpers
bool UUtilsEditorContext_Notification::HasFunctionWithName(const FName EventName) const
{
	if (EventName.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("Notification:: EventName is empty...it does need a name"));
		return false;
	}

	if (GraphObject == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("Notification:: Failed to get a valid ObjectWithFunction for  [%s]"),
		       *EventName.ToString());
		return false;
	}

	const UFunction* Func = GraphObject->FindFunction(EventName);
	if (!Func)
	{
		UE_LOG(LogTemp, Warning, TEXT("Notification:: Failed to find a function with name [%s]"),
			   *EventName.ToString());
		return false;
	}

	if (Func->ParmsSize != 0)
	{
		UE_LOG(LogTemp, Warning,
			   TEXT("Notification:: Found a function with name [%s] but it take parameters and I dont do that..."),
			   *EventName.ToString());
		return false;
	}

	return true;
}

#pragma endregion