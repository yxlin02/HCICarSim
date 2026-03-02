// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RecommendationManager.h"
#include "UI_PauseGame.h"
#include "DecisionManagerComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDecisionEvent);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnReactionMake,
    int32, Reactiontype
);

class SWindow; 

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LVL_AGENT_DEMO_API UDecisionManagerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDecisionManagerComponent();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decision")
    float CooldownTime = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decision")
    float RespondTime = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decision")
    float AgentReactionAnimationTime = 1.0f;

    UPROPERTY(BlueprintAssignable, Category = "Decision")
    FDecisionEvent OnDecisionTriggered;

    UPROPERTY(BlueprintAssignable, Category="Decision")
    FOnReactionMake OnReactionMake;

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
    void OnContentSoundEnd();  // ===== 新增：音频播放结束回调 =====

    // ===== 新增：创建独立窗口（可选功能）=====
    void CreateSeparateWindowForWidget(UUserWidget* Widget);
    TSharedPtr<SWindow> RecommendationWindow;
    // ===== 结束新增 =====

    FTimerHandle CooldownTimerHandle;
    FTimerHandle RespondTimerHandle;
    FTimerHandle AgentReactionAnimationHandle;
    FTimerHandle ContentDisplayTimerHandle;  // ===== 新增：用于控制内容显示时长 =====

    // ===== 新增：跟踪音频播放 =====
    UPROPERTY()
    UAudioComponent* CurrentContentAudioComponent;
    // ===== 结束新增 =====

    bool bCooldownActive = false;
    bool bWaitingForResponse = false;
    bool bHitInWindow = false;

    UPROPERTY()
    URecommendationManager* RecMgr;

    int32 recommendationTimes = 0;

    UPROPERTY(EditAnywhere, Category="UI")
    TSubclassOf<UUI_PauseGame> PauseGameWidgetClass;

    UPROPERTY()
    UUI_PauseGame* PauseGameWidgetInstance;

    FTimerHandle PauseAfterAcceptHandle;

    UPROPERTY(EditAnywhere, Category="Decision")
    float AcceptReactionPauseDelay = 2.0f;

    UFUNCTION()
    void PauseGameAfterReaction();

    UFUNCTION()
    void OnContinueAfterReaction();
};