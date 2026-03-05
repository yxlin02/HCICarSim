#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h" 
#include "TimerManager.h" 
#include "MozaInputModifier.generated.h" 

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteeringChanged, float, SteeringValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnThrottleChanged, float, ThrottleValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBrakeChanged, float, BrakeValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBrakeStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBrakeTriggered, float, BrakeValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBrakeCompleted);

UCLASS(Blueprintable)
class MOZASDKPLUGIN_API UMozaInputBroadcaster : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	void StartPolling();

	UPROPERTY(BlueprintAssignable) FOnSteeringChanged OnSteeringChanged;
	UPROPERTY(BlueprintAssignable) FOnThrottleChanged OnThrottleChanged;
	UPROPERTY(BlueprintAssignable) FOnBrakeChanged    OnBrakeChanged;
	UPROPERTY(BlueprintAssignable) FOnBrakeStarted    OnBrakeStarted;
	UPROPERTY(BlueprintAssignable) FOnBrakeTriggered  OnBrakeTriggered;
	UPROPERTY(BlueprintAssignable) FOnBrakeCompleted  OnBrakeCompleted;

private:
	void PollMozaInput();

	FTimerHandle PollTimerHandle;
	float PollingInterval = 0.01f;

	UPROPERTY()
	bool bIsPolling = false;

	bool  bBrakeWasPressed       = false;
	bool  bSteeringInitialized   = false;
	bool  bThrottleInitialized   = false;
	bool  bBrakeInitialized      = false;

	// ★ 新增：用于按钮边沿检测
	bool  bButtonsInitialized    = false;
	int32 LastButtonPressNum[128] = {};

	float LastValidSteering = 0.0f;
	float LastValidThrottle = 0.0f;
	float LastValidBrake    = 0.0f;
};