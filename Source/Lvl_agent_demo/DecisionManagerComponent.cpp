// Fill out your copyright notice in the Description page of Project Settings.


#include "DecisionManagerComponent.h"
#include "TimerManager.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"

UDecisionManagerComponent::UDecisionManagerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UDecisionManagerComponent::BeginPlay()
{
    Super::BeginPlay();
    StartCooldown();
    
    // Spawn Recommendation UI
    UWorld* World = GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Error, TEXT("[GM] World is null"));
        return;
    }

    UGameInstance* GI = World->GetGameInstance();
    if (!GI)
    {
        UE_LOG(LogTemp, Error, TEXT("[GM] GameInstance is null"));
        return;
    }

    RecMgr = GI->GetSubsystem<URecommendationManager>();
    if (!RecMgr)
    {
        UE_LOG(LogTemp, Error, TEXT("[GM] RecommendationManager not found"));
        return;
    }
    
    if (UUserWidget* RecommendationWidgetInstance = Cast<UUserWidget>(RecMgr->RecommendationWidget))
    {
        TriggerDecision();
        RecommendationWidgetInstance->AddToViewport(100);
        UE_LOG(LogTemp, Display, TEXT("[GM] Recommendation widget added to viewport."));
    }
}

void UDecisionManagerComponent::StartCooldown()
{
    bCooldownActive = true;
    bHitInWindow = false;

    GetWorld()->GetTimerManager().SetTimer(
        CooldownTimerHandle,
        this,
        &UDecisionManagerComponent::OnCooldownExpired,
        CooldownTime,
        false
    );
    
    UE_LOG(LogTemp, Display, TEXT("[Decision Manager] Pawn cool down starts"));
}

void UDecisionManagerComponent::OnCooldownExpired()
{
    bCooldownActive = false;
    
    if (!bWaitingForResponse && bHitInWindow)
    {
        TriggerDecision();
    }
    UE_LOG(LogTemp, Display, TEXT("[Decision Manager] Pawn cool down ends"));
}

void UDecisionManagerComponent::HandleColliderHit()
{
    if (bWaitingForResponse)
    {
        return;
    }

    if (bCooldownActive)
    {
        bHitInWindow = true;
        return;
    }
    
    if (bHitInWindow)
    {
        return;
    }
    
    TriggerDecision();
}

void UDecisionManagerComponent::TriggerDecision()
{
    if (bWaitingForResponse)
    {
        return;
    }

    bWaitingForResponse = true;
    bCooldownActive = false;
    
    recommendationTimes++;
    
    GetWorld()->GetTimerManager().SetTimer(
        RespondTimerHandle,
        this,
        &UDecisionManagerComponent::OnIgnore,
        RespondTime,
        false
    );

    GetWorld()->GetTimerManager().ClearTimer(CooldownTimerHandle);

    OnDecisionTriggered.Broadcast();
    RecMgr->TriggerRandomRecommendation();
}

void UDecisionManagerComponent::OnAccept()
{
    if (!bWaitingForResponse) return;
    
    GetWorld()->GetTimerManager().ClearTimer(RespondTimerHandle);
    RecMgr->CurrentDecision = EDecisionTypes::Accept;
    RecMgr->DisplayReaction();
    RecMgr->DisplayContent();
    GetWorld()->GetTimerManager().SetTimer(
        AgentReactionAnimationHandle,
        this,
        &UDecisionManagerComponent::OnAgentReactionEnd,
        AgentReactionAnimationTime,
        false
    );

    // TODO: 处理“接受”的游戏逻辑
    UE_LOG(LogTemp, Display, TEXT("[Decision Manager] User accepted the recommendation"));
    
    // Play accept effect
    bWaitingForResponse = false;
    StartCooldown();
}

void UDecisionManagerComponent::OnReject()
{
    if (!bWaitingForResponse) return;
    
    GetWorld()->GetTimerManager().ClearTimer(RespondTimerHandle);
    RecMgr->CurrentDecision = EDecisionTypes::Decline;
    RecMgr->DisplayReaction();
    GetWorld()->GetTimerManager().SetTimer(
        AgentReactionAnimationHandle,
        this,
        &UDecisionManagerComponent::OnAgentReactionEnd,
        AgentReactionAnimationTime,
        false
    );

    // TODO: 处理“拒绝”的游戏逻辑
    UE_LOG(LogTemp, Display, TEXT("[Decision Manager] User rejected the recommendation"));
    
    // Play Reject effect
    bWaitingForResponse = false;
    StartCooldown();
}

void UDecisionManagerComponent::OnIgnore()
{
    if (!bWaitingForResponse) return;
    
    GetWorld()->GetTimerManager().ClearTimer(RespondTimerHandle);
    RecMgr->CurrentDecision = EDecisionTypes::Ignore;
    RecMgr->DisplayReaction();
    GetWorld()->GetTimerManager().SetTimer(
        AgentReactionAnimationHandle,
        this,
        &UDecisionManagerComponent::OnAgentReactionEnd,
        AgentReactionAnimationTime,
        false
    );

    // TODO: 处理“拒绝”的游戏逻辑
    UE_LOG(LogTemp, Display, TEXT("[Decision Manager] User ignored the recommendation"));
    
    // Play ignore effect
    bWaitingForResponse = false;
    StartCooldown();
}

void UDecisionManagerComponent::OnAgentReactionEnd()
{
    RecMgr->RecommendationWidget->ShowReaction();
}

