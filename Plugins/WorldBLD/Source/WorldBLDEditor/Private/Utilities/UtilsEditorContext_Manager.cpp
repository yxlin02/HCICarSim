// Copyright WorldBLD LLC. All rights reserved.


#include "Utilities/UtilsEditorContext_Manager.h"

#include "Utilities/UtilsEditorContext_Notification.h"

UUtilsEditorContext_Notification* UUtilsEditorContext_Manager::GetNotificationContext(
	const FS_UtilsNotificationInfo& Info, UObject* InGraphObject, const bool ShowNotification)
{
	// Clean old references
	ActiveNotifications = ActiveNotifications.FilterByPredicate([](const TPair<TWeakObjectPtr<UObject>, TWeakObjectPtr<UUtilsEditorContext_Notification>>& Entry)
	{
		return Entry.Key.IsValid() && Entry.Value.IsValid();
	});

	const float CurrentTime = static_cast<float>(FPlatformTime::Seconds() - GStartTime);
	if (IsValid(InGraphObject))
	{
		if (const auto* ToastEntryPtr = ActiveNotifications.Find(InGraphObject))
		{
			auto& Toast = *ToastEntryPtr->Get();
			// Throttle notifications so they don't swarm the UI (might want to make this configurable)
			if (CurrentTime - Toast.CreationTimestamp < 0.700f)
			{
				return &Toast;
			}

			// Force the existing notification to fade out.
			Toast.Fadeout();
		}
	}

	UUtilsEditorContext_Notification* Notification = NewObject<UUtilsEditorContext_Notification>();
	if (IsValid(InGraphObject))
	{
		ActiveNotifications.Emplace(InGraphObject) = Notification;
	}

	Notification->CreationTimestamp = CurrentTime;
	Notification->Setup(Info, InGraphObject, ShowNotification);
	return Notification;
}
