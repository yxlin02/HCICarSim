// Copyright Epic Games, Inc. All Rights Reserved.

#include "Lvl_agent_demoUI.h"
#include "RecommendationManager.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void ULvl_agent_demoUI::UpdateSpeed(float NewSpeed)
{
	float FormattedSpeed = FMath::Abs(NewSpeed) * (bIsMPH ? 0.022f : 0.036f);
	OnSpeedUpdate(FormattedSpeed);
}

void ULvl_agent_demoUI::UpdateGear(int32 NewGear)
{
	OnGearUpdate(NewGear);
}

// ── 内部工具 ──────────────────────────────────────────────────

int32 ULvl_agent_demoUI::GetCurrentSceneID() const
{
	const UWorld* World = GetWorld();
	if (!World) return -1;

	const UGameInstance* GI = World->GetGameInstance();
	if (!GI) return -1;

	const URecommendationManager* RecMgr = GI->GetSubsystem<URecommendationManager>();
	if (!RecMgr) return -1;

	return RecMgr->GetCurrentSceneID();
}

void ULvl_agent_demoUI::SetCountdownVisibility(ESlateVisibility InVisibility)
{
	if (Text_Countdown)
	{
		Text_Countdown->SetVisibility(InVisibility);
	}
	if (Text)
	{
		Text->SetVisibility(InVisibility);
	}
}

// ── 倒计时实现 ────────────────────────────────────────────────

void ULvl_agent_demoUI::NativeConstruct()
{
	Super::NativeConstruct();

	CountdownRemaining = CountdownDuration;
	bWasCountdownVisible = false;

	// 初始隐藏所有倒计时控件
	SetCountdownVisibility(ESlateVisibility::Collapsed);

	if (bAutoStartCountdown && CountdownDuration > 0.f)
	{
		StartCountdown();
	}
}

void ULvl_agent_demoUI::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 判断当前是否应该显示倒计时
	const int32 CurrentSceneID = GetCurrentSceneID();
	const bool bShouldShow = (CountdownRequiredSceneID == -1) ||
	                         (CurrentSceneID == CountdownRequiredSceneID);

	// 可见性变化时统一操作两个控件
	if (bShouldShow != bWasCountdownVisible)
	{
		bWasCountdownVisible = bShouldShow;
		SetCountdownVisibility(
			bShouldShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// 不满足条件或未运行时不更新
	if (!bShouldShow || !bCountdownRunning)
	{
		return;
	}

	CountdownRemaining = FMath::Max(0.f, CountdownRemaining - InDeltaTime);

	// 更新倒计时数字
	if (Text_Countdown)
	{
		const int32 SecondsInt = FMath::CeilToInt(CountdownRemaining);
		Text_Countdown->SetText(FText::AsNumber(SecondsInt));
	}

	if (CountdownRemaining <= 0.f)
	{
		bCountdownRunning = false;
		if (Text_Countdown)
		{
			Text_Countdown->SetText(FText::FromString(TEXT("0")));
		}
		OnCountdownFinished();
	}
}

void ULvl_agent_demoUI::StartCountdown()
{
	CountdownRemaining = CountdownDuration;
	bCountdownRunning = (CountdownDuration > 0.f);
}

void ULvl_agent_demoUI::PauseCountdown()
{
	bCountdownRunning = false;
}

void ULvl_agent_demoUI::ResumeCountdown()
{
	if (CountdownRemaining > 0.f)
	{
		bCountdownRunning = true;
	}
}

void ULvl_agent_demoUI::ResetCountdown()
{
	bCountdownRunning = false;
	CountdownRemaining = CountdownDuration;
}