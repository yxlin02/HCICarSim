// Fill out your copyright notice in the Description page of Project Settings.

#include "DecisionManagerComponent.h"
#include "TimerManager.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Components/AudioComponent.h"  // ===== 新增 =====

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
        // ===== 可选：使用独立窗口或添加到Viewport =====
        // 选项1：独立窗口（推荐用于双显示器）
        CreateSeparateWindowForWidget(RecommendationWidgetInstance);

        // 选项2：添加到Viewport（原始方式）
        // RecommendationWidgetInstance->AddToViewport(100);
        // ===== 结束可选 =====

        UE_LOG(LogTemp, Display, TEXT("[GM] Recommendation widget added."));
    }
}

// ===== 新增：组件销毁时的清理 =====
void UDecisionManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 停止正在播放的音频
    if (CurrentContentAudioComponent && CurrentContentAudioComponent->IsPlaying())
    {
        CurrentContentAudioComponent->Stop();
        CurrentContentAudioComponent = nullptr;
    }

    // 清理所有定时器
    if (UWorld* World = GetWorld())
    {
        FTimerManager& TimerManager = World->GetTimerManager();
        TimerManager.ClearTimer(CooldownTimerHandle);
        TimerManager.ClearTimer(RespondTimerHandle);
        TimerManager.ClearTimer(AgentReactionAnimationHandle);
        TimerManager.ClearTimer(ContentDisplayTimerHandle);
    }

    // 关闭独立窗口
    if (RecommendationWindow.IsValid() && FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().RequestDestroyWindow(RecommendationWindow.ToSharedRef());
        RecommendationWindow.Reset();
        UE_LOG(LogTemp, Display, TEXT("[DecisionManager] Recommendation window closed"));
    }

    Super::EndPlay(EndPlayReason);
}
// ===== 结束新增 =====

// ===== 新增：创建独立窗口（可选功能）=====
void UDecisionManagerComponent::CreateSeparateWindowForWidget(UUserWidget* Widget)
{
    if (!Widget || !FSlateApplication::IsInitialized())
    {
        UE_LOG(LogTemp, Error, TEXT("[DecisionManager] Cannot create window"));
        return;
    }

    FDisplayMetrics DisplayMetrics;
    FSlateApplication::Get().GetDisplayMetrics(DisplayMetrics);

    const float WindowWidth = 1920.0f;
    const float WindowHeight = 720.0f;

    FVector2D WindowPosition;
    bool bHasSecondMonitor = false;

    if (DisplayMetrics.VirtualDisplayRect.Right > DisplayMetrics.PrimaryDisplayWidth)
    {
        WindowPosition = FVector2D(DisplayMetrics.PrimaryDisplayWidth, 0);
        bHasSecondMonitor = true;
    }
    else
    {
        WindowPosition = FVector2D(
            (DisplayMetrics.PrimaryDisplayWidth - WindowWidth) / 2.0f,
            (DisplayMetrics.PrimaryDisplayHeight - WindowHeight) / 2.0f
        );
    }

    // ===== 方案2：无边框但可拖动 =====
    TSharedRef<SWindow> NewWindow = SNew(SWindow)
        .Type(EWindowType::Normal)
        .Title(FText::FromString(TEXT("")))
        .ClientSize(FVector2D(WindowWidth, WindowHeight))
        .ScreenPosition(WindowPosition)
        .UseOSWindowBorder(false)                          // 不使用系统边框
        .CreateTitleBar(false)                             // 不创建标题栏
        .bDragAnywhere(true)                               // ===== 允许从任意位置拖动窗口 =====
        .SupportsMaximize(false)
        .SupportsMinimize(false)
        .HasCloseButton(false)
        .SizingRule(ESizingRule::FixedSize)
        .AutoCenter(EAutoCenter::None)
        .FocusWhenFirstShown(false)
        .ActivationPolicy(EWindowActivationPolicy::Never)
        .IsTopmostWindow(false);
    // ===== 修改结束 =====

    NewWindow->SetContent(Widget->TakeWidget());
    FSlateApplication::Get().AddWindow(NewWindow);
    RecommendationWindow = NewWindow;

    UE_LOG(LogTemp, Display, TEXT("[DecisionManager] Borderless draggable window created (%s)"),
        bHasSecondMonitor ? TEXT("second monitor") : TEXT("centered"));
}
// ===== 结束新增 =====

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

    // ===== 新增：停止之前的音频播放 =====
    if (CurrentContentAudioComponent && CurrentContentAudioComponent->IsPlaying())
    {
        CurrentContentAudioComponent->Stop();
        CurrentContentAudioComponent = nullptr;

        GetWorld()->GetTimerManager().ClearTimer(ContentDisplayTimerHandle);

        if (RecMgr && RecMgr->RecommendationWidget)
        {
            RecMgr->RecommendationWidget->ClearContent();
            UE_LOG(LogTemp, Display, TEXT("[Decision Manager] Previous audio stopped"));
        }
    }
    // ===== 结束新增 =====

    if (RecMgr->bHasPendingDelayedRecommendation) return;

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

    RecMgr->DisplayRecommendation();
}

void UDecisionManagerComponent::OnAccept()
{
    if (!bWaitingForResponse) return;

    GetWorld()->GetTimerManager().ClearTimer(RespondTimerHandle);
    RecMgr->CurrentDecision = EDecisionTypes::Accept;
    RecMgr->DisplayReaction();
    RecMgr->DisplayContent();

    // ===== 新增：获取音频组件并设置定时器 =====
    CurrentContentAudioComponent = RecMgr->GetLastContentAudioComponent();

    if (CurrentContentAudioComponent && CurrentContentAudioComponent->Sound)
    {
        float SoundDuration = CurrentContentAudioComponent->Sound->GetDuration();

        GetWorld()->GetTimerManager().SetTimer(
            ContentDisplayTimerHandle,
            this,
            &UDecisionManagerComponent::OnContentSoundEnd,
            SoundDuration,
            false
        );

        UE_LOG(LogTemp, Display, TEXT("[Decision Manager] Content will be hidden after %.2f seconds"), SoundDuration);
    }
    // ===== 结束新增 =====

    GetWorld()->GetTimerManager().SetTimer(
        AgentReactionAnimationHandle,
        this,
        &UDecisionManagerComponent::OnAgentReactionEnd,
        AgentReactionAnimationTime,
        false
    );

    OnReactionMake.Broadcast(1);

    UE_LOG(LogTemp, Display, TEXT("[Decision Manager] User accepted the recommendation"));

    GetWorld()->GetTimerManager().SetTimer(
        PauseAfterAcceptHandle,
        this,
        &UDecisionManagerComponent::PauseGameAfterReaction,
        AcceptReactionPauseDelay,
        false
    );

    bWaitingForResponse = false;
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

    OnReactionMake.Broadcast(-1);

    UE_LOG(LogTemp, Display, TEXT("[Decision Manager] User rejected the recommendation"));

    GetWorld()->GetTimerManager().SetTimer(
        PauseAfterAcceptHandle,
        this,
        &UDecisionManagerComponent::PauseGameAfterReaction,
        AcceptReactionPauseDelay,
        false
    );

    bWaitingForResponse = false;
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

    OnReactionMake.Broadcast(0);

    UE_LOG(LogTemp, Display, TEXT("[Decision Manager] User ignored the recommendation"));

    GetWorld()->GetTimerManager().SetTimer(
        PauseAfterAcceptHandle,
        this,
        &UDecisionManagerComponent::PauseGameAfterReaction,
        AcceptReactionPauseDelay,
        false
    );

    bWaitingForResponse = false;
}

void UDecisionManagerComponent::OnAgentReactionEnd()
{
    // ===== 修改：使用更清晰的方法名 =====
    if (RecMgr && RecMgr->RecommendationWidget)
    {
        RecMgr->RecommendationWidget->ClearReactionAndRecommendation();
        UE_LOG(LogTemp, Display, TEXT("[DecisionManager] Reaction cleared"));
    }
    // ===== 结束修改 =====
}

// ===== 新增：音频播放结束回调 =====
void UDecisionManagerComponent::OnContentSoundEnd()
{
    if (RecMgr && RecMgr->RecommendationWidget)
    {
        RecMgr->RecommendationWidget->ClearContent();
        UE_LOG(LogTemp, Display, TEXT("[DecisionManager] Content cleared after sound finished"));
    }

    CurrentContentAudioComponent = nullptr;
}
// ===== 结束新增 =====

void UDecisionManagerComponent::PauseGameAfterReaction() 
{
    if (!PauseGameWidgetClass) return;
    if (!PauseGameWidgetInstance) 
    {
        PauseGameWidgetInstance = CreateWidget<UUI_PauseGame>(
            GetWorld(),
            PauseGameWidgetClass
        );

        if (PauseGameWidgetInstance) 
        {
            PauseGameWidgetInstance->OnContinue.AddDynamic(
                this,
                &UDecisionManagerComponent::OnContinueAfterReaction
            );
        }
    }

    if (PauseGameWidgetInstance) 
    {
        PauseGameWidgetInstance->ShowPauseUI();
    }
}

void UDecisionManagerComponent::OnContinueAfterReaction() 
{
    StartCooldown();

    UE_LOG(LogTemp, Display, TEXT("[Decision Manager] Continue after reaction, start cooling down"));
}