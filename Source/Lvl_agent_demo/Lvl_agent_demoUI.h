// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Lvl_agent_demoUI.generated.h"

/**
 *  Simple Vehicle HUD class
 *  Displays the current speed, gear, and a configurable countdown timer.
 *  Widget setup is handled in a Blueprint subclass.
 */
UCLASS(abstract)
class ULvl_agent_demoUI : public UUserWidget
{
	GENERATED_BODY()
	
protected:

	/** Controls the display of speed in Km/h or MPH */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle")
	bool bIsMPH = false;

	// ── 倒计时控件绑定（Optional：蓝图里不存在也不报错）──────

	/** 显示倒计时数字的文本控件，蓝图里命名为 Text_Countdown */
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* Text_Countdown = nullptr;

	/** 倒计时旁边的标签控件，蓝图里命名为 Text */
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* Text = nullptr;

	// ── 倒计时参数 ──────────────────────────────────────────

	/** 倒计时总时长（秒），在 Editor Class Defaults 中设置；0 表示不启用 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Countdown",
		meta = (ClampMin = "0.0", UIMin = "0.0", DisplayName = "Countdown Duration (s)"))
	float CountdownDuration = 60.f;

	/** 是否在 BeginPlay 时自动开始倒计时 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Countdown")
	bool bAutoStartCountdown = true;

	/** 当前剩余时间（运行时），蓝图可读 */
	UPROPERTY(BlueprintReadOnly, Category = "Vehicle|Countdown")
	float CountdownRemaining = 0.f;

	/** 倒计时是否正在运行 */
	UPROPERTY(BlueprintReadOnly, Category = "Vehicle|Countdown")
	bool bCountdownRunning = false;

	/** 只有 SceneID == 此值时才显示倒计时，-1 表示始终显示 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Countdown",
		meta = (DisplayName = "Show Only In SceneID (-1 = Always)"))
	int32 CountdownRequiredSceneID = 3;

public:

	/** Called to update the speed display */
	void UpdateSpeed(float NewSpeed);

	/** Called to update the gear display */
	void UpdateGear(int32 NewGear);

	// ── 倒计时公开接口 ──────────────────────────────────────

	UFUNCTION(BlueprintCallable, Category = "Vehicle|Countdown")
	void StartCountdown();

	UFUNCTION(BlueprintCallable, Category = "Vehicle|Countdown")
	void PauseCountdown();

	UFUNCTION(BlueprintCallable, Category = "Vehicle|Countdown")
	void ResumeCountdown();

	UFUNCTION(BlueprintCallable, Category = "Vehicle|Countdown")
	void ResetCountdown();

protected:

	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	int32 GetCurrentSceneID() const;

	// ── 蓝图事件 ─────────────────────────────────────────────

	UFUNCTION(BlueprintImplementableEvent, Category = "Vehicle")
	void OnSpeedUpdate(float NewSpeed);

	UFUNCTION(BlueprintImplementableEvent, Category = "Vehicle")
	void OnGearUpdate(int32 NewGear);

	/** 倒计时归零时调用（蓝图可选处理） */
	UFUNCTION(BlueprintImplementableEvent, Category = "Vehicle|Countdown")
	void OnCountdownFinished();

private:

	bool bWasCountdownVisible = false;

	/** 统一设置倒计时相关控件的可见性 */
	void SetCountdownVisibility(ESlateVisibility InVisibility);
};
