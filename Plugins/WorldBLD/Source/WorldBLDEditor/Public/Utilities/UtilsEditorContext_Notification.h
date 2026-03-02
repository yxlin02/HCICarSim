// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "Styling/SlateBrush.h"
#include "Templates/SharedPointer.h"
#include "UtilsEditorContext_Notification.generated.h"

struct FSlateBrush;
class SNotificationItem;
class UTexture2D;

#pragma region Enum

UENUM(BlueprintType)
enum class EUtilsBPNotificationCompletionState : uint8
{
	None,	
	Pending,
	Success,
	Fail,	
};

#pragma endregion

#pragma region Struct

USTRUCT(BlueprintType)
struct FS_UtilsNotificationIcon
{
	GENERATED_BODY()

	/* Texture to use as an icon */
	UPROPERTY(BlueprintReadWrite, Category = "Notification")
		UTexture2D* Texture;

	/* The size of the icon */
	UPROPERTY(BlueprintReadWrite, Category = "Notification")
		FVector2D Size;

	FS_UtilsNotificationIcon() {
		Texture = nullptr;
		Size = FVector2D(32, 32);
	}
};

USTRUCT(BlueprintType)
struct FS_UtilsNotificationButtonInfo
{
	GENERATED_BODY()

	/** Message on the button */
	UPROPERTY(BlueprintReadWrite, Category = "Notification")
		FString Text;
	
	/** Tip displayed when moused over */
	UPROPERTY(BlueprintReadWrite, Category = "Notification")
		FString ToolTip;

	/** CustomEvent/FunctionName to call when the button is pressed */
	UPROPERTY(BlueprintReadWrite, Category = "Notification")
		FString EventName;

	/** Should the notification be closed when the button is clicked? */
	UPROPERTY(BlueprintReadWrite, Category = "Notification")
		bool CloseOnClick;

	/** Visibility of the button when the completion state of the button is SNotificationItem::ECompletionState::None */
	UPROPERTY(BlueprintReadWrite, Category = "Notification")
	bool VisibilityOnNone;

	/** Visibility of the button when the completion state of the button is SNotificationItem::ECompletionState::Pending */	
	UPROPERTY(BlueprintReadWrite, Category = "Notification")
	bool VisibilityOnPending;
	
	/** Visibility of the button when the completion state of the button is SNotificationItem::ECompletionState::Success */	
	UPROPERTY(BlueprintReadWrite, Category = "Notification")
	bool VisibilityOnSuccess;
	
	/** Visibility of the button when the completion state of the button is SNotificationItem::ECompletionState::Fail */
	UPROPERTY(BlueprintReadWrite, Category = "Notification")
	bool VisibilityOnFail;

	FS_UtilsNotificationButtonInfo() {
		Text = "";
		ToolTip = "";
		EventName = "";
		CloseOnClick = true;
		VisibilityOnNone = true;
		VisibilityOnPending = true;
		VisibilityOnSuccess = true;
		VisibilityOnFail = true;
	}
};

USTRUCT(BlueprintType)
struct FS_UtilsNotificationButtons
{
	GENERATED_BODY()

	/** Array with the buttons to add to the notification message */
	UPROPERTY(BlueprintReadWrite, Category = "Notification")
	TArray<FS_UtilsNotificationButtonInfo> Buttons;
};


/**
 * Setup class to initialize a notification.
 */
USTRUCT(BlueprintType)
struct FS_UtilsNotificationInfo
{
	GENERATED_BODY()

	/** The text displayed in this text block */
	UPROPERTY(BlueprintReadWrite, Category = "Notification")
	FText Text;

	/** The icon image to display next to the text */
	UPROPERTY(BlueprintReadWrite, Category = "Notification")
	FSlateBrush Image;

	/** The fade in duration for this element */
	UPROPERTY(BlueprintReadWrite, Category = "Notification")
	float FadeInDuration;

	/** The fade out duration for this element */
	UPROPERTY(BlueprintReadWrite, Category = "Notification")
	float FadeOutDuration;

	/** The duration before a fadeout for this element */
	UPROPERTY(BlueprintReadWrite, Category = "Notification")
	float ExpireDuration;

	/** Controls whether or not to add the animated throbber */
	UPROPERTY(BlueprintReadWrite, Category = "Notification")
	bool bUseThrobber;

	/** Controls whether or not to display the success and fail icons */
	UPROPERTY(BlueprintReadWrite, Category = "Notification")
	bool bUseSuccessFailIcons;

	/** When true the larger bolder font will be used to display the message */
	UPROPERTY(BlueprintReadWrite, Category = "Notification")
	bool bUseLargeFont;

	/** When set this forces the width of the box, used to stop resizing on text change */
	UPROPERTY(BlueprintReadWrite, Category = "Notification")
	float WidthOverride;

	/** When true the notification will automatically time out after the expire duration. */
	UPROPERTY(BlueprintReadWrite, Category = "Notification")
	bool bFireAndForget;

	/** True if we should throttle the editor while the notification is transitioning and performance is poor, to make sure the user can see the animation */
	UPROPERTY(BlueprintReadWrite, Category = "Notification")
	bool bAllowThrottleWhenFrameRateIsLow;
	
	/** Text to display for the hyperlink message */
	UPROPERTY(BlueprintReadWrite, Category = "Notification")
		FText HyperlinkText;
	
	/** When set this will display as a hyperlink on the right side of the notification.
	And the EventName will be executed when the hyperlink is clicked, can be a CustomEvent or FunctionName*/
	UPROPERTY(BlueprintReadWrite, Category = "Notification")
		FString HyperlinkEventName;

	/* Buttons on the Notification */
	UPROPERTY(BlueprintReadWrite, Category = "Notification")
		FS_UtilsNotificationButtons Buttons;


	FS_UtilsNotificationInfo() {
		Text = FText::GetEmpty();
		Image = FSlateBrush();
		FadeInDuration = 0.5f;
		FadeOutDuration = 0.5f;
		ExpireDuration = 2.0f;
		bUseThrobber = true;
		bUseSuccessFailIcons = true;
		bUseLargeFont = true;
		WidthOverride = 0.0f;
		bFireAndForget = true;
		bAllowThrottleWhenFrameRateIsLow = true;
		Buttons = FS_UtilsNotificationButtons();
	}

};

#pragma endregion



/**
 * 
 */
UCLASS(Blueprintable)
class WORLDBLDEDITOR_API UUtilsEditorContext_Notification : public UObject
{
	GENERATED_BODY()

public:

#pragma region Property

	UPROPERTY()
	FS_UtilsNotificationInfo NotificationInfo;

	UPROPERTY()
	bool bIsShowing = false;

	bool bIsFree = false;

	UPROPERTY()
	UObject* GraphObject;

	UPROPERTY()
	float CreationTimestamp = -1.0f;

#pragma endregion

#pragma region Setup
	~UUtilsEditorContext_Notification() override;

	void Setup(const FS_UtilsNotificationInfo& Info, UObject* InGraphObject, const bool ShowNotification = false);

#pragma endregion

#pragma region NotificationItem

	/* Show the notification */
	UFUNCTION(BlueprintCallable, Category = "CityUtilsEditor|Notification")
	void Show();

	/* Close the notification with the fade out */
	UFUNCTION(BlueprintCallable, Category = "CityUtilsEditor|Notification")
	void Close(const FText Text);

	/* Close the notification after it has been tagged as success */
	UFUNCTION(BlueprintCallable, Category = "CityUtilsEditor|Notification")
	void CloseAsSuccess(const FText Text);

	/* Close the notification after it has been tagged as fail */
	UFUNCTION(BlueprintCallable, Category = "CityUtilsEditor|Notification")
	void CloseAsFail(const FText Text);

	/** Waits for the ExpireDuration then begins to fade out */
	UFUNCTION(BlueprintCallable, Category = "CityUtilsEditor|Notification")
	void ExpireAndFadeout();

	/** Begin the fade out */
	UFUNCTION(BlueprintCallable, Category = "CityUtilsEditor|Notification")
	void Fadeout();

	/** Sets the visibility state of the throbber, success, and fail images */
	UFUNCTION(BlueprintCallable, Category = "CityUtilsEditor|Notification")
	void SetCompletionState(EUtilsBPNotificationCompletionState State);

	/** Sets the visibility state of the throbber, success, and fail images */
	UFUNCTION(BlueprintCallable, Category = "CityUtilsEditor|Notification")
	void SetStateAsPending();

	/** Sets the visibility state of the throbber, success, and fail images */
	UFUNCTION(BlueprintCallable, Category = "CityUtilsEditor|Notification")
	void SetCompleteStateFail();

	/** Sets the visibility state of the throbber, success, and fail images */
	UFUNCTION(BlueprintCallable, Category = "CityUtilsEditor|Notification")
	void SetCompleteStateSuccess();

	UFUNCTION(BlueprintCallable, Category = "CityUtilsEditor|Notification")
	void SetText(const FText NewText);

	UFUNCTION(BlueprintPure, Category = "CityUtilsEditor|Notification")
	bool IsNotificationValid();

	UFUNCTION(BlueprintCallable, Category = "CityUtilsEditor|Notification")
	void Clear();

	UFUNCTION()
	void HandleOnButtonClicked(FS_UtilsNotificationButtonInfo ButtonInfo);
#pragma endregion

#pragma region Helpers
	bool IsFree() const { return bIsFree; }

	bool HasFunctionWithName(const FName EventName) const;
#pragma endregion
	
private:
	TSharedPtr<SNotificationItem> NotificationPtr;
};
