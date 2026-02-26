// Copyright WorldBLD LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "WorldBLDAuthSlateBridge.generated.h"

class SWorldBLDLoginDialog;
class UWorldBLDAuthSubsystem;

UCLASS()
class WORLDBLDEDITOR_API UWorldBLDAuthSlateBridge : public UObject
{
	GENERATED_BODY()

public:
	void Init(TSharedRef<SWorldBLDLoginDialog> InWidget, UWorldBLDAuthSubsystem* InAuthSubsystem);

protected:
	virtual void BeginDestroy() override;

private:
	UFUNCTION()
	void HandleLoginSuccess(const FString& SessionToken);

	UFUNCTION()
	void HandleLoginFail(const FString& ErrorMessage);

	UFUNCTION()
	void HandleTwoFactorRequired();

private:
	TWeakPtr<SWorldBLDLoginDialog> Widget;

	UPROPERTY()
	TObjectPtr<UWorldBLDAuthSubsystem> AuthSubsystem = nullptr;
};

