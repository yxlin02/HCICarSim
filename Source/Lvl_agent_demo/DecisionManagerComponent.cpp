// Fill out your copyright notice in the Description page of Project Settings.

#include "DecisionManagerComponent.h"
#include "TimerManager.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Components/AudioComponent.h"

UDecisionManagerComponent::UDecisionManagerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UDecisionManagerComponent::BeginPlay()
{
    Super::BeginPlay();
    StartCooldown();

    UWorld* World = GetWorld();
    if (!World) { UE_LOG(LogTemp, Error, TEXT("[GM] World is null")); return; }

    UGameInstance* GI = World->GetGameInstance();
    if (!GI) { UE_LOG(LogTemp, Error, TEXT("[GM] GameInstance is null")); return; }

    RecMgr = GI->GetSubsystem<URecommendationManager>();
    if (!RecMgr) { UE_LOG(LogTemp, Error, TEXT("[GM] RecommendationManager not found")); return; }

    if (UUserWidget* RecommendationWidgetInstance = Cast<UUserWidget>(RecMgr->RecommendationWidget))
    {
        CreateSeparateWindowForWidget(RecommendationWidgetInstance);
        UE_LOG(LogTemp, Display, TEXT("[GM] Recommendation widget added."));
    }
}

void UDecisionManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (CurrentContentAudioComponent && CurrentContentAudioComponent->IsPlaying())
    {
        CurrentContentAudioComponent->Stop();
        CurrentContentAudioComponent = nullptr;
    }

    if (UWorld* World = GetWorld())
    {
        FTimerManager& TM = World->GetTimerManager();
        TM.ClearTimer(CooldownTimerHandle);
        TM.ClearTimer(RespondTimerHandle);
        TM.ClearTimer(AgentReactionAnimationHandle);
        TM.ClearTimer(ContentDisplayTimerHandle);
        TM.ClearTimer(QuitGameTimerHandle);
    }

    if (RecommendationWindow.IsValid() && FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().RequestDestroyWindow(RecommendationWindow.ToSharedRef());
        RecommendationWindow.Reset();
    }

    Super::EndPlay(EndPlayReason);
}

void UDecisionManagerComponent::CreateSeparateWindowForWidget(UUserWidget* Widget)
{
    if (!Widget || !FSlateApplication::IsInitialized()) return;

    FDisplayMetrics DisplayMetrics;
    FSlateApplication::Get().GetDisplayMetrics(DisplayMetrics);

    const float WindowWidth  = 1920.0f;
    const float WindowHeight = 720.0f;

    FVector2D WindowPosition;
    bool bHasSecondMonitor = false;

    if (DisplayMetrics.VirtualDisplayRect.Right > DisplayMetrics.PrimaryDisplayWidth)
    {
        WindowPosition    = FVector2D(DisplayMetrics.PrimaryDisplayWidth, 0);
        bHasSecondMonitor = true;
    }
    else
    {
        WindowPosition = FVector2D(
            (DisplayMetrics.PrimaryDisplayWidth  - WindowWidth)  / 2.0f,
            (DisplayMetrics.PrimaryDisplayHeight - WindowHeight) / 2.0f
        );
    }

    TSharedRef<SWindow> NewWindow = SNew(SWindow)
        .Type(EWindowType::Normal)
        .Title(FText::FromString(TEXT("")))
        .ClientSize(FVector2D(WindowWidth, WindowHeight))
        .ScreenPosition(WindowPosition)
        .UseOSWindowBorder(false)
        .CreateTitleBar(false)
        .bDragAnywhere(true)
        .SupportsMaximize(false)
        .SupportsMinimize(false)
        .HasCloseButton(false)
        .SizingRule(ESizingRule::FixedSize)
        .AutoCenter(EAutoCenter::None)
        .FocusWhenFirstShown(false)
        .ActivationPolicy(EWindowActivationPolicy::Never)
        .IsTopmostWindow(false);

    NewWindow->SetContent(Widget->TakeWidget());
    FSlateApplication::Get().AddWindow(NewWindow);
    RecommendationWindow = NewWindow;

    UE_LOG(LogTemp, Display, TEXT("[DecisionManager] Window created (%s)"),
        bHasSecondMonitor ? TEXT("second monitor") : TEXT("centered"));
}

void UDecisionManagerComponent::StartCooldown()
{
    bCooldownActive = true;
    bHitInWindow    = false;

    GetWorld()->GetTimerManager().SetTimer(
        CooldownTimerHandle, this,
        &UDecisionManagerComponent::OnCooldownExpired,
        CooldownTime, false);

    UE_LOG(LogTemp, Display, TEXT("[Decision Manager] Cooldown starts"));
}

void UDecisionManagerComponent::OnCooldownExpired()
{
    bCooldownActive = false;
    if (!bWaitingForResponse && bHitInWindow) TriggerDecision();
    UE_LOG(LogTemp, Display, TEXT("[Decision Manager] Cooldown ends"));
}

void UDecisionManagerComponent::HandleColliderHit()
{
    if (bWaitingForResponse) return;
    if (bCooldownActive) { bHitInWindow = true; return; }
    if (bHitInWindow)    return;
    if (RecMgr && RecMgr->GetCurrentRowId() != 0) TriggerDecision();
}

void UDecisionManagerComponent::TriggerDecision()
{
    if (bWaitingForResponse) return;

    if (CurrentContentAudioComponent && CurrentContentAudioComponent->IsPlaying())
    {
        CurrentContentAudioComponent->Stop();
        CurrentContentAudioComponent = nullptr;
        GetWorld()->GetTimerManager().ClearTimer(ContentDisplayTimerHandle);
        if (RecMgr && RecMgr->RecommendationWidget) RecMgr->RecommendationWidget->ClearContent();
    }

    if (RecMgr->bHasPendingDelayedRecommendation) return;
	if (RecMgr->GetCurrentRowId() == 0) return;

    bWaitingForResponse = true;
    bCooldownActive     = false;
    recommendationTimes++;

    GetWorld()->GetTimerManager().SetTimer(
        RespondTimerHandle, this,
        &UDecisionManagerComponent::OnIgnore,
        RespondTime, false);

    GetWorld()->GetTimerManager().ClearTimer(CooldownTimerHandle);

    OnDecisionTriggered.Broadcast();
    RecMgr->DisplayRecommendation();
}

void UDecisionManagerComponent::OnAccept()
{
    if (!bWaitingForResponse) return;

    GetWorld()->GetTimerManager().ClearTimer(RespondTimerHandle);
    
    // ★ 新增：打断正在播放的 Recommendation 音频
    if (RecMgr)
    {
        UAudioComponent* RecAudio = RecMgr->GetLastRecommendationAudioComponent();
        if (RecAudio && RecAudio->IsPlaying())
        {
            RecAudio->Stop();
            UE_LOG(LogTemp, Display, TEXT("[Decision Manager] Recommendation audio stopped by Accept"));
        }
    }

    RecMgr->CurrentDecision = EDecisionTypes::Accept;
    RecMgr->DisplayReaction();
    RecMgr->DisplayContent ();

    CurrentContentAudioComponent = RecMgr->GetLastContentAudioComponent();
    if (CurrentContentAudioComponent && CurrentContentAudioComponent->Sound)
    {
        float SoundDuration = CurrentContentAudioComponent->Sound->GetDuration();
        GetWorld()->GetTimerManager().SetTimer(
            ContentDisplayTimerHandle, this,
            &UDecisionManagerComponent::OnContentSoundEnd,
            SoundDuration, false);
    }

    GetWorld()->GetTimerManager().SetTimer(
        AgentReactionAnimationHandle, this,
        &UDecisionManagerComponent::OnAgentReactionEnd,
        AgentReactionAnimationTime, false);

    OnReactionMake.Broadcast(1);
    UE_LOG(LogTemp, Display, TEXT("[Decision Manager] User accepted the recommendation"));

    // ===== Pattern 完成检测：启动退出倒计时 =====
    if (RecMgr->IsPatternComplete())
    {
        UE_LOG(LogTemp, Display, TEXT("[Decision Manager] Pattern complete after Accept. Quitting in %.1f s"), QuitGameDelay);
        GetWorld()->GetTimerManager().SetTimer(
            QuitGameTimerHandle, this,
            &UDecisionManagerComponent::QuitGame,
            QuitGameDelay, false);
    }
    else
    {
        GetWorld()->GetTimerManager().SetTimer(
            PauseAfterAcceptHandle, this,
            &UDecisionManagerComponent::PauseGameAfterReaction,
            AcceptReactionPauseDelay, false);
    }
    // ===== 结束 =====

    bWaitingForResponse = false;
}

void UDecisionManagerComponent::OnReject()
{
    if (!bWaitingForResponse) return;

    GetWorld()->GetTimerManager().ClearTimer(RespondTimerHandle);
    
    // ★ 新增：打断正在播放的 Recommendation 音频
    if (RecMgr)
    {
        UAudioComponent* RecAudio = RecMgr->GetLastRecommendationAudioComponent();
        if (RecAudio && RecAudio->IsPlaying())
        {
            RecAudio->Stop();
            UE_LOG(LogTemp, Display, TEXT("[Decision Manager] Recommendation audio stopped by Reject"));
        }
    }

    RecMgr->CurrentDecision = EDecisionTypes::Decline;
    RecMgr->DisplayReaction();

    GetWorld()->GetTimerManager().SetTimer(
        AgentReactionAnimationHandle, this,
        &UDecisionManagerComponent::OnAgentReactionEnd,
        AgentReactionAnimationTime, false);

    OnReactionMake.Broadcast(-1);
    UE_LOG(LogTemp, Display, TEXT("[Decision Manager] User rejected the recommendation"));

    // ===== Pattern 完成检测：启动退出倒计时 =====
    if (RecMgr->IsPatternComplete())
    {
        UE_LOG(LogTemp, Display, TEXT("[Decision Manager] Pattern complete after Reject. Quitting in %.1f s"), QuitGameDelay);
        GetWorld()->GetTimerManager().SetTimer(
            QuitGameTimerHandle, this,
            &UDecisionManagerComponent::QuitGame,
            QuitGameDelay, false);
    }
    else
    {
        GetWorld()->GetTimerManager().SetTimer(
            PauseAfterAcceptHandle, this,
            &UDecisionManagerComponent::PauseGameAfterReaction,
            AcceptReactionPauseDelay, false);
    }
    // ===== 结束 =====

    bWaitingForResponse = false;
}

void UDecisionManagerComponent::OnIgnore()
{
    if (!bWaitingForResponse) return;

    GetWorld()->GetTimerManager().ClearTimer(RespondTimerHandle);

    // ★ 新增：打断正在播放的 Recommendation 音频
    if (RecMgr)
    {
        UAudioComponent* RecAudio = RecMgr->GetLastRecommendationAudioComponent();
        if (RecAudio && RecAudio->IsPlaying())
        {
            RecAudio->Stop();
            UE_LOG(LogTemp, Display, TEXT("[Decision Manager] Recommendation audio stopped by Ignore"));
        }
    }

    RecMgr->CurrentDecision = EDecisionTypes::Ignore;
    RecMgr->DisplayReaction();

    GetWorld()->GetTimerManager().SetTimer(
        AgentReactionAnimationHandle, this,
        &UDecisionManagerComponent::OnAgentReactionEnd,
        AgentReactionAnimationTime, false);

    OnReactionMake.Broadcast(0);
    UE_LOG(LogTemp, Display, TEXT("[Decision Manager] User ignored the recommendation"));

    GetWorld()->GetTimerManager().SetTimer(
        PauseAfterAcceptHandle, this,
        &UDecisionManagerComponent::PauseGameAfterReaction,
        AcceptReactionPauseDelay, false);

    bWaitingForResponse = false;
}

void UDecisionManagerComponent::OnAgentReactionEnd()
{
    if (RecMgr && RecMgr->RecommendationWidget)
    {
        RecMgr->RecommendationWidget->ClearReactionAndRecommendation();
        UE_LOG(LogTemp, Display, TEXT("[DecisionManager] Reaction cleared"));
    }
}

void UDecisionManagerComponent::OnContentSoundEnd()
{
    if (RecMgr && RecMgr->RecommendationWidget)
    {
        RecMgr->RecommendationWidget->ClearContent();
        UE_LOG(LogTemp, Display, TEXT("[DecisionManager] Content cleared after sound finished"));
    }
    CurrentContentAudioComponent = nullptr;
}

void UDecisionManagerComponent::QuitGame()
{
    UE_LOG(LogTemp, Display, TEXT("[Decision Manager] Quitting game now."));
    UKismetSystemLibrary::QuitGame(
        GetWorld(),
        GetWorld()->GetFirstPlayerController(),
        EQuitPreference::Quit,
        false);
}

void UDecisionManagerComponent::PauseGameAfterReaction()
{
    if (!PauseGameWidgetClass) return;
    if (!PauseGameWidgetInstance)
    {
        PauseGameWidgetInstance = CreateWidget<UUI_PauseGame>(GetWorld(), PauseGameWidgetClass);
        if (PauseGameWidgetInstance)
        {
            PauseGameWidgetInstance->OnContinue.AddDynamic(
                this, &UDecisionManagerComponent::OnContinueAfterReaction);
        }
    }
    if (PauseGameWidgetInstance) PauseGameWidgetInstance->ShowPauseUI();
}

void UDecisionManagerComponent::OnContinueAfterReaction()
{
    StartCooldown();
    UE_LOG(LogTemp, Display, TEXT("[Decision Manager] Continue after reaction, start cooling down"));
}