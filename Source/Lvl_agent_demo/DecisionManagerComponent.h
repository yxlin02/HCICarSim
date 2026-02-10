// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RecommendationManager.h"
#include "DecisionManagerComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDecisionEvent);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class LVL_AGENT_DEMO_API UDecisionManagerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    // Sets default values for this component's properties
    UDecisionManagerComponent();

protected:
    // Called when the game starts
    virtual void BeginPlay() override;

public:
    // Called every frame
//    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Decision")
    float CooldownTime = 3.f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Decision")
    float RespondTime = 10.f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Decision")
    float AgentReactionAnimationTime = 2.f;

    UPROPERTY(BlueprintAssignable, Category="Decision")
    FDecisionEvent OnDecisionTriggered;

    UFUNCTION(BlueprintCallable, Category="Decision")
    void HandleColliderHit();

    UFUNCTION(BlueprintCallable, Category="Decision")
    void OnAccept();

    UFUNCTION(BlueprintCallable, Category="Decision")
    void OnReject();
    
    UFUNCTION(BlueprintCallable, Category="Decision")
    void OnIgnore();
    
private:
    void StartCooldown();
    void OnCooldownExpired();
    void TriggerDecision();
    
    void DisplayReaction();
    void OnAgentReactionEnd();

    bool bCooldownActive = false;
    bool bHitInWindow = false;
    bool bWaitingForResponse = false;
    
    URecommendationManager* RecMgr;
    
    int32 recommendationTimes = 0;

    FTimerHandle CooldownTimerHandle;
    FTimerHandle RespondTimerHandle;
    FTimerHandle AgentReactionAnimationHandle;
        
};
