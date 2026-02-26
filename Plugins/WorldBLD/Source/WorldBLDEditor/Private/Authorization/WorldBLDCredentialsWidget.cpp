// Copyright WorldBLD LLC. All Rights Reserved.

#include "Authorization/WorldBLDCredentialsWidget.h"
#include "Editor.h"

void UWorldBLDCredentialsWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (GEditor)
	{
		AuthSubsystem = GEditor->GetEditorSubsystem<UWorldBLDAuthSubsystem>();
		if (AuthSubsystem)
		{
			AuthSubsystem->OnLoginSuccess.AddDynamic(this, &UWorldBLDCredentialsWidget::HandleLoginSuccess);
			AuthSubsystem->OnLoginFail.AddDynamic(this, &UWorldBLDCredentialsWidget::HandleLoginFail);
			AuthSubsystem->OnTwoFactorRequired.AddDynamic(this, &UWorldBLDCredentialsWidget::HandleTwoFactorRequired);
			AuthSubsystem->OnEmailCheckSuccess.AddDynamic(this, &UWorldBLDCredentialsWidget::HandleEmailCheckSuccess);
			AuthSubsystem->OnEmailCheckFailure.AddDynamic(this, &UWorldBLDCredentialsWidget::HandleEmailCheckFailure);
		}
	}
}

void UWorldBLDCredentialsWidget::SubmitCredentials(FString Email, FString Password, FString OTP)
{
	if (AuthSubsystem)
	{
		AuthSubsystem->Login(Email, Password, OTP);
	}
}

void UWorldBLDCredentialsWidget::Login(FString Email, FString Password)
{
	SubmitCredentials(Email, Password, TEXT(""));
}

void UWorldBLDCredentialsWidget::Submit2FA(FString Email, FString Password, FString OTP)
{
	SubmitCredentials(Email, Password, OTP);
}

void UWorldBLDCredentialsWidget::CheckEmail(FString Email)
{
	if (AuthSubsystem)
	{
		AuthSubsystem->CheckEmail(Email);
	}
}

void UWorldBLDCredentialsWidget::HandleLoginSuccess(const FString& SessionToken)
{
	OnLoginSuccess.Broadcast(SessionToken);
}

void UWorldBLDCredentialsWidget::HandleLoginFail(const FString& ErrorMessage)
{
	OnLoginFail.Broadcast(ErrorMessage);
}

void UWorldBLDCredentialsWidget::HandleTwoFactorRequired()
{
	OnRequest2FA.Broadcast();
}

void UWorldBLDCredentialsWidget::HandleEmailCheckSuccess()
{
	OnEmailCheckSuccess.Broadcast();
}

void UWorldBLDCredentialsWidget::HandleEmailCheckFailure(const FString& ErrorMessage)
{
	OnEmailCheckFailure.Broadcast(ErrorMessage);
}
