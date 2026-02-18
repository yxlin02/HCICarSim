// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RecommendationManager.h"
#include "DecisionManagerComponent.generated.h"

class SWindow;  // 前向声明

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDecisionTriggered);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LVL_AGENT_DEMO_API UDecisionManagerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDecisionManagerComponent();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;  // 新增

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decision")
    float CooldownTime = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decision")
    float RespondTime = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decision")
    float AgentReactionAnimationTime = 1.0f;

    UPROPERTY(BlueprintAssignable, Category = "Decision")
    FOnDecisionTriggered OnDecisionTriggered;

    UFUNCTION(BlueprintCallable, Category = "Decision")
    void HandleColliderHit();

    UFUNCTION(BlueprintCallable, Category = "Decision")
    void OnAccept();

    UFUNCTION(BlueprintCallable, Category = "Decision")
    void OnReject();

    UFUNCTION(BlueprintCallable, Category = "Decision")
    void OnIgnore();

private:
    void StartCooldown();
    void OnCooldownExpired();
    void TriggerDecision();
    void OnAgentReactionEnd();
    void OnContentSoundEnd();  // 新增：音频播放结束回调

    // ===== 新增：创建独立窗口 =====
    void CreateSeparateWindowForWidget(UUserWidget* Widget);

    /** 独立窗口引用 */
    TSharedPtr<SWindow> RecommendationWindow;
    // ===== 结束新增 =====

    FTimerHandle CooldownTimerHandle;
    FTimerHandle RespondTimerHandle;
    FTimerHandle AgentReactionAnimationHandle;
    FTimerHandle ContentDisplayTimerHandle;  // 新增：用于控制内容显示时长

    bool bCooldownActive = false;
    bool bWaitingForResponse = false;
    bool bHitInWindow = false;

    UPROPERTY()
    URecommendationManager* RecMgr;

    int recommendationTimes = 0;
};