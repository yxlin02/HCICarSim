// Copyright WorldBLD LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;
class SWorldBLDContextMenu;
class UWorldBLDAuthSubsystem;

class SWorldBLDLoginDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorldBLDLoginDialog)
	{
	}
		SLATE_EVENT(FSimpleDelegate, OnRequestClose)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SWorldBLDLoginDialog() override;

	// Called by the auth bridge.
	void HandleAuthLoginSuccess(const FString& SessionToken);
	void HandleAuthLoginFail(const FString& ErrorMessage);
	void HandleAuthTwoFactorRequired();

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	FReply HandleLoginClicked();
	FReply HandleLogoutClicked();
	FReply HandleCancelClicked();

	void SetBusy(bool bInBusy);
	void SetErrorMessage(const FText& InError);
	void ClearErrorMessage();

	bool CanSubmitLogin() const;
	EVisibility GetOTPVisibility() const;
	EVisibility GetLoggedInVisibility() const;
	EVisibility GetLoginFormVisibility() const;
	EVisibility GetBusyVisibility() const;

private:
	FSimpleDelegate OnRequestClose;

	TWeakObjectPtr<UWorldBLDAuthSubsystem> AuthSubsystem;
	FDelegateHandle LoginSuccessHandle;
	FDelegateHandle LoginFailHandle;
	FDelegateHandle TwoFactorRequiredHandle;

	TSharedPtr<SWorldBLDContextMenu> RootMenu;
	TSharedPtr<SEditableTextBox> EmailTextBox;
	TSharedPtr<SEditableTextBox> PasswordTextBox;
	TSharedPtr<SEditableTextBox> OTPTextBox;

	FString Email;
	FString Password;
	FString OTP;

	bool bIsBusy = false;
	bool bTwoFactorRequired = false;
	bool bAlreadyLoggedIn = false;
};

