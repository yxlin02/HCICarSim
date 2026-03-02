// Copyright WorldBLD LLC. All Rights Reserved.

#include "Authorization/SWorldBLDLoginDialog.h"

#include "Authorization/WorldBLDAuthSubsystem.h"
#include "ContextMenu/SWorldBLDContextMenu.h"
#include "ContextMenu/WorldBLDContextMenuStyle.h"

#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformProcess.h"
#include "Input/Reply.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "WorldBLDLoginDialog"

void SWorldBLDLoginDialog::Construct(const FArguments& InArgs)
{
	OnRequestClose = InArgs._OnRequestClose;

	AuthSubsystem = GEditor ? GEditor->GetEditorSubsystem<UWorldBLDAuthSubsystem>() : nullptr;
	bAlreadyLoggedIn = AuthSubsystem.IsValid() && UWorldBLDAuthSubsystem::IsLoggedIn();

	if (AuthSubsystem.IsValid())
	{
		// Bind directly to native delegates so Slate always receives auth results,
		// even if UObject/dynamic delegate routing changes.
		LoginSuccessHandle = AuthSubsystem->OnLoginSuccessNative.AddSP(SharedThis(this), &SWorldBLDLoginDialog::HandleAuthLoginSuccess);
		LoginFailHandle = AuthSubsystem->OnLoginFailNative.AddSP(SharedThis(this), &SWorldBLDLoginDialog::HandleAuthLoginFail);
		TwoFactorRequiredHandle = AuthSubsystem->OnTwoFactorRequiredNative.AddSP(SharedThis(this), &SWorldBLDLoginDialog::HandleAuthTwoFactorRequired);
	}

	const TSharedRef<SWidget> MissingSubsystemPanel =
		SNew(SVerticalBox)
		.Visibility_Lambda([this]()
		{
			return AuthSubsystem.IsValid() ? EVisibility::Collapsed : EVisibility::Visible;
		})
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("MissingSubsystem", "Authentication subsystem is unavailable."))
			.AutoWrapText(true)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0.0f, 12.0f, 0.0f, 0.0f))
		[
			SNew(SButton)
			.ButtonStyle(&WorldBLDContextMenuStyle::GetNeutralButtonStyle())
			.Text(LOCTEXT("CloseButton_MissingSubsystem", "Close"))
			.OnClicked(this, &SWorldBLDLoginDialog::HandleCancelClicked)
		];

	const TSharedRef<SWidget> LoggedInPanel =
		SNew(SVerticalBox)
		.Visibility(this, &SWorldBLDLoginDialog::GetLoggedInVisibility)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("AlreadyLoggedIn", "You are already signed in."))
			.AutoWrapText(true)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0.0f, 12.0f, 0.0f, 0.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SButton)
				.ButtonStyle(&WorldBLDContextMenuStyle::GetNeutralButtonStyle())
				.IsEnabled_Lambda([this]() { return !bIsBusy; })
				.Text(LOCTEXT("CloseButton_LoggedIn", "Close"))
				.OnClicked(this, &SWorldBLDLoginDialog::HandleCancelClicked)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
			[
				SNew(SButton)
				.ButtonStyle(&WorldBLDContextMenuStyle::GetHighlightedButtonStyle())
				.IsEnabled_Lambda([this]() { return !bIsBusy && AuthSubsystem.IsValid(); })
				.Text(LOCTEXT("LogoutButton", "Logout"))
				.OnClicked(this, &SWorldBLDLoginDialog::HandleLogoutClicked)
			]
		];

	const TSharedRef<SWidget> LoginForm =
		SNew(SVerticalBox)
		.Visibility(this, &SWorldBLDLoginDialog::GetLoginFormVisibility)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("EmailLabel", "Email"))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0.0f, 4.0f, 0.0f, 10.0f))
		[
			SAssignNew(EmailTextBox, SEditableTextBox)
			.IsEnabled_Lambda([this]() { return !bIsBusy; })
			.OnTextChanged_Lambda([this](const FText& InText)
			{
				const FString IncomingEmail = InText.ToString();
				const FString LowercaseEmail = IncomingEmail.ToLower();

				Email = LowercaseEmail;

				// Keep widget content normalized so users only ever submit lowercase email.
				if (IncomingEmail != LowercaseEmail && EmailTextBox.IsValid())
				{
					EmailTextBox->SetText(FText::FromString(LowercaseEmail));
				}
			})
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("PasswordLabel", "Password"))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0.0f, 4.0f, 0.0f, 10.0f))
		[
			SAssignNew(PasswordTextBox, SEditableTextBox)
			.IsPassword(true)
			.IsEnabled_Lambda([this]() { return !bIsBusy; })
			.OnTextChanged_Lambda([this](const FText& InText) { Password = InText.ToString(); })
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("OTPLabel", "One-Time Password (2FA)"))
			.Visibility(this, &SWorldBLDLoginDialog::GetOTPVisibility)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0.0f, 4.0f, 0.0f, 10.0f))
		[
			SAssignNew(OTPTextBox, SEditableTextBox)
			.IsEnabled_Lambda([this]() { return !bIsBusy; })
			.OnTextChanged_Lambda([this](const FText& InText) { OTP = InText.ToString(); })
			.Visibility(this, &SWorldBLDLoginDialog::GetOTPVisibility)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(0.0f, 0.0f, 0.0f, 0.0f))
			[
				SNew(SButton)
				.ButtonStyle(&WorldBLDContextMenuStyle::GetNeutralButtonStyle())
				.IsEnabled_Lambda([this]() { return !bIsBusy; })
				.Text(LOCTEXT("CancelButton", "Cancel"))
				.OnClicked(this, &SWorldBLDLoginDialog::HandleCancelClicked)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
			[
				SNew(SButton)
				.ButtonStyle(&WorldBLDContextMenuStyle::GetHighlightedButtonStyle())
				.IsEnabled_Lambda([this]() { return !bIsBusy && CanSubmitLogin(); })
				.Text_Lambda([this]()
				{
					return bTwoFactorRequired ? LOCTEXT("SubmitOTP", "Submit") : LOCTEXT("LoginButton", "Login");
				})
				.OnClicked(this, &SWorldBLDLoginDialog::HandleLoginClicked)
			]
			
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
			[
				SNew(SBox)
				.WidthOverride(16.0f)
				.HeightOverride(16.0f)
				[
					SNew(SThrobber)
					.Visibility(this, &SWorldBLDLoginDialog::GetBusyVisibility)
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNullWidget::NullWidget
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
			[
				SNew(SHyperlink)
				.Text(LOCTEXT("CreateAccountLink", "Create Account"))
				.IsEnabled_Lambda([this]() { return !bIsBusy; })
				.OnNavigate_Lambda([]()
				{
					FPlatformProcess::LaunchURL(TEXT("https://worldbld.com/register"), nullptr, nullptr);
				})
			]
		];

	ChildSlot
	[
		SAssignNew(RootMenu, SWorldBLDContextMenu)
		.Title(LOCTEXT("DialogTitle", "WorldBLD Login"))
		.ShowTitleBar(false)
		.OnRequestClose(OnRequestClose)
		.Content(
			SNew(SBox)
			.MinDesiredWidth(420.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MissingSubsystemPanel
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					LoggedInPanel
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					LoginForm
				]
			])
	];

	if (!bAlreadyLoggedIn && AuthSubsystem.IsValid() && EmailTextBox.IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(EmailTextBox.ToSharedRef(), EFocusCause::SetDirectly);
	}
}

SWorldBLDLoginDialog::~SWorldBLDLoginDialog()
{
	if (AuthSubsystem.IsValid())
	{
		if (LoginSuccessHandle.IsValid())
		{
			AuthSubsystem->OnLoginSuccessNative.Remove(LoginSuccessHandle);
		}
		if (LoginFailHandle.IsValid())
		{
			AuthSubsystem->OnLoginFailNative.Remove(LoginFailHandle);
		}
		if (TwoFactorRequiredHandle.IsValid())
		{
			AuthSubsystem->OnTwoFactorRequiredNative.Remove(TwoFactorRequiredHandle);
		}
	}
}

void SWorldBLDLoginDialog::HandleAuthLoginSuccess(const FString& SessionToken)
{
	(void)SessionToken;
	SetBusy(false);
	ClearErrorMessage();
	Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);

	OnRequestClose.ExecuteIfBound();
}

void SWorldBLDLoginDialog::HandleAuthLoginFail(const FString& ErrorMessage)
{
	SetBusy(false);

	// Keep the form values; clear OTP so the user re-enters it if needed.
	if (bTwoFactorRequired)
	{
		OTP.Reset();
		if (OTPTextBox.IsValid())
		{
			OTPTextBox->SetText(FText::GetEmpty());
		}
	}

	SetErrorMessage(FText::FromString(ErrorMessage));
	Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
}

void SWorldBLDLoginDialog::HandleAuthTwoFactorRequired()
{
	SetBusy(false);
	ClearErrorMessage();

	bTwoFactorRequired = true;
	Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);

	if (OTPTextBox.IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(OTPTextBox.ToSharedRef(), EFocusCause::SetDirectly);
	}
}

FReply SWorldBLDLoginDialog::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	(void)MyGeometry;

	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape)
	{
		return HandleCancelClicked();
	}

	if (Key == EKeys::Enter)
	{
		if (!bAlreadyLoggedIn)
		{
			return HandleLoginClicked();
		}
	}

	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

void SWorldBLDLoginDialog::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// Safety net: if the auth subsystem has already established a session (token persisted)
	// but the login success delegate didn't reach this widget (modal/tick edge cases),
	// stop showing "busy" and close the window.
	if (bIsBusy && AuthSubsystem.IsValid() && UWorldBLDAuthSubsystem::IsLoggedIn())
	{
		const FString Token = AuthSubsystem->GetSessionToken();
		if (!Token.IsEmpty())
		{
			HandleAuthLoginSuccess(Token);
		}
	}
}

FReply SWorldBLDLoginDialog::HandleLoginClicked()
{
	ClearErrorMessage();

	if (!AuthSubsystem.IsValid())
	{
		SetErrorMessage(LOCTEXT("AuthUnavailable", "Authentication subsystem is unavailable."));
		return FReply::Handled();
	}

	if (!CanSubmitLogin())
	{
		SetErrorMessage(LOCTEXT("MissingFields", "Please enter email and password."));
		return FReply::Handled();
	}

	if (bTwoFactorRequired && OTP.TrimStartAndEnd().IsEmpty())
	{
		SetErrorMessage(LOCTEXT("MissingOTP", "Please enter your one-time password."));
		return FReply::Handled();
	}

	Email = Email.ToLower();
	SetBusy(true);
	AuthSubsystem->Login(Email, Password, bTwoFactorRequired ? OTP : TEXT(""));
	return FReply::Handled();
}

FReply SWorldBLDLoginDialog::HandleLogoutClicked()
{
	if (AuthSubsystem.IsValid())
	{
		AuthSubsystem->Logout();
	}

	OnRequestClose.ExecuteIfBound();
	return FReply::Handled();
}

FReply SWorldBLDLoginDialog::HandleCancelClicked()
{
	OnRequestClose.ExecuteIfBound();
	return FReply::Handled();
}

void SWorldBLDLoginDialog::SetBusy(bool bInBusy)
{
	bIsBusy = bInBusy;
	Invalidate(EInvalidateWidgetReason::LayoutAndVolatility);
}

void SWorldBLDLoginDialog::SetErrorMessage(const FText& InError)
{
	if (RootMenu.IsValid())
	{
		RootMenu->SetErrorMessage(InError);
	}
}

void SWorldBLDLoginDialog::ClearErrorMessage()
{
	if (RootMenu.IsValid())
	{
		RootMenu->ClearErrorMessage();
	}
}

bool SWorldBLDLoginDialog::CanSubmitLogin() const
{
	return !Email.TrimStartAndEnd().IsEmpty() && !Password.IsEmpty();
}

EVisibility SWorldBLDLoginDialog::GetOTPVisibility() const
{
	return bTwoFactorRequired ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SWorldBLDLoginDialog::GetLoggedInVisibility() const
{
	return (AuthSubsystem.IsValid() && bAlreadyLoggedIn) ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SWorldBLDLoginDialog::GetLoginFormVisibility() const
{
	if (!AuthSubsystem.IsValid())
	{
		return EVisibility::Collapsed;
	}

	return bAlreadyLoggedIn ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility SWorldBLDLoginDialog::GetBusyVisibility() const
{
	// Use Hidden (not Collapsed) to keep the layout stable when toggling busy state.
	return bIsBusy ? EVisibility::Visible : EVisibility::Hidden;
}

#undef LOCTEXT_NAMESPACE

