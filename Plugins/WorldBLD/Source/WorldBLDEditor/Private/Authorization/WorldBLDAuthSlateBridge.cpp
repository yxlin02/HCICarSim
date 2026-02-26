// Copyright WorldBLD LLC. All Rights Reserved.

#include "Authorization/WorldBLDAuthSlateBridge.h"

#include "Authorization/SWorldBLDLoginDialog.h"
#include "Authorization/WorldBLDAuthSubsystem.h"

void UWorldBLDAuthSlateBridge::Init(TSharedRef<SWorldBLDLoginDialog> InWidget, UWorldBLDAuthSubsystem* InAuthSubsystem)
{
	Widget = InWidget;
	AuthSubsystem = InAuthSubsystem;

	if (!AuthSubsystem)
	{
		return;
	}

	AuthSubsystem->OnLoginSuccess.AddDynamic(this, &UWorldBLDAuthSlateBridge::HandleLoginSuccess);
	AuthSubsystem->OnLoginFail.AddDynamic(this, &UWorldBLDAuthSlateBridge::HandleLoginFail);
	AuthSubsystem->OnTwoFactorRequired.AddDynamic(this, &UWorldBLDAuthSlateBridge::HandleTwoFactorRequired);
}

void UWorldBLDAuthSlateBridge::BeginDestroy()
{
	if (AuthSubsystem)
	{
		AuthSubsystem->OnLoginSuccess.RemoveDynamic(this, &UWorldBLDAuthSlateBridge::HandleLoginSuccess);
		AuthSubsystem->OnLoginFail.RemoveDynamic(this, &UWorldBLDAuthSlateBridge::HandleLoginFail);
		AuthSubsystem->OnTwoFactorRequired.RemoveDynamic(this, &UWorldBLDAuthSlateBridge::HandleTwoFactorRequired);
	}

	Super::BeginDestroy();
}

void UWorldBLDAuthSlateBridge::HandleLoginSuccess(const FString& SessionToken)
{
	if (TSharedPtr<SWorldBLDLoginDialog> Pinned = Widget.Pin())
	{
		Pinned->HandleAuthLoginSuccess(SessionToken);
	}
}

void UWorldBLDAuthSlateBridge::HandleLoginFail(const FString& ErrorMessage)
{
	if (TSharedPtr<SWorldBLDLoginDialog> Pinned = Widget.Pin())
	{
		Pinned->HandleAuthLoginFail(ErrorMessage);
	}
}

void UWorldBLDAuthSlateBridge::HandleTwoFactorRequired()
{
	if (TSharedPtr<SWorldBLDLoginDialog> Pinned = Widget.Pin())
	{
		Pinned->HandleAuthTwoFactorRequired();
	}
}

