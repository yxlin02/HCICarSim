// Copyright WorldBLD LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Authorization/WorldBLDAuthSubsystem.h"
#include "WorldBLDCredentialsWidget.generated.h"

/**
 * Base Widget for WorldBLD Login UI.
 */
UCLASS()
class WORLDBLDEDITOR_API UWorldBLDCredentialsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	/**
	 * Submit credentials to the auth subsystem.
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Auth")
	void SubmitCredentials(FString Email, FString Password, FString OTP);

	/**
	 * Attempt login with just username/password first. 
	 * If 2FA is needed, OnRequest2FA will be broadcast.
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Auth")
	void Login(FString Email, FString Password);

	/**
	 * Submit 2FA code after OnRequest2FA has triggered.
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Auth")
	void Submit2FA(FString Email, FString Password, FString OTP);

	/**
	 * Check whether an account exists for the given email before prompting for password.
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Auth")
	void CheckEmail(FString Email);

public:
	UPROPERTY(BlueprintAssignable, Category = "WorldBLD|Auth")
	FOnAuthSuccess OnLoginSuccess;

	UPROPERTY(BlueprintAssignable, Category = "WorldBLD|Auth")
	FOnAuthError OnLoginFail;

	UPROPERTY(BlueprintAssignable, Category = "WorldBLD|Auth")
	FOnAuthTarget2FA OnRequest2FA;

	UPROPERTY(BlueprintAssignable, Category = "WorldBLD|Auth")
	FOnEmailCheckSuccess OnEmailCheckSuccess;

	UPROPERTY(BlueprintAssignable, Category = "WorldBLD|Auth")
	FOnEmailCheckFailure OnEmailCheckFailure;

private:
	UFUNCTION()
	void HandleLoginSuccess(const FString& SessionToken);

	UFUNCTION()
	void HandleLoginFail(const FString& ErrorMessage);

	UFUNCTION()
	void HandleTwoFactorRequired();

	UFUNCTION()
	void HandleEmailCheckSuccess();

	UFUNCTION()
	void HandleEmailCheckFailure(const FString& ErrorMessage);

	UPROPERTY()
	UWorldBLDAuthSubsystem* AuthSubsystem;
};
