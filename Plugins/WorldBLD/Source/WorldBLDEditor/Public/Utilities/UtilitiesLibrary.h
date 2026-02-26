// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UtilsEditorContext_Notification.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UtilitiesLibrary.generated.h"

/**
 * 
 */
UCLASS()
class WORLDBLDEDITOR_API UUtilitiesLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "WorldBLD|Utilities|Notification", Meta = (DefaultToSelf = "InGraphObject", AdvancedDisplay = "2", ShowNotification = "true"))
	static UUtilsEditorContext_Notification* GetNotificationContext(const FS_UtilsNotificationInfo& Info, const bool ShowNotification, UObject* InGraphObject);

	/* Show a quick notification message */
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Utilities|Notification", Meta = (DefaultToSelf = "InGraphObject", AdvancedDisplay = "1", Duration = "8.0"))
	static void ShowQuickNotification(const FText& Text, const FS_UtilsNotificationIcon& Icon, EUtilsBPNotificationCompletionState State, float Duration, const FS_UtilsNotificationButtons& Buttons, UObject* InGraphObject);
	
	/** Shows a modal dialog with an OK button (editor-only). */
	UFUNCTION(BlueprintCallable, Category="WorldBLD|Utilities|Dialog")
	static void ShowOkModalDialog(const FText& Message, const FText& Title);

	UFUNCTION(BlueprintCallable, Category="WorldBLD|Utilities")
	static void OpenExternalUrlLinkInWebBrowser(const FString& Url);
};
