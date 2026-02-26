// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "UtilsEditorContext_Manager.generated.h"

class UUtilsEditorContext_Notification;
struct FS_UtilsNotificationInfo;
/**
 * 
 */
UCLASS()
class WORLDBLDEDITOR_API UUtilsEditorContext_Manager : public UObject
{
	GENERATED_BODY()
	
public:
	UUtilsEditorContext_Notification* GetNotificationContext(const FS_UtilsNotificationInfo& Info, UObject* InGraphObject, const bool ShowNotification);

protected:
	UPROPERTY()
	TMap<TWeakObjectPtr<UObject>, TWeakObjectPtr<UUtilsEditorContext_Notification>> ActiveNotifications;
};
