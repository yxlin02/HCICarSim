// Fill out your copyright notice in the Description page of Project Settings.


#include "DecisionManagerComponent.h"
#include "TimerManager.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Components/AudioComponent.h"  // ===== 新增：添加 AudioComponent 头文件 =====

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

        // ===== 新增：创建独立窗口 =====
        CreateSeparateWindowForWidget(RecommendationWidgetInstance);
        // ===== 结束新增 =====

        UE_LOG(LogTemp, Display, TEXT("[GM] Recommendation widget added to separate window."));
    }
}

// ===== 新增：组件销毁时关闭窗口 =====
void UDecisionManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // ===== 新增：停止正在播放的音频 =====
    if (CurrentContentAudioComponent && CurrentContentAudioComponent->IsPlaying())
    {
        CurrentContentAudioComponent->Stop();
        CurrentContentAudioComponent = nullptr;
    }
    // ===== 结束新增 =====

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
        UE_LOG(LogTemp, Display, TEXT("[DecisionManager] Recommendation window closed on component destruction"));
    }

    Super::EndPlay(EndPlayReason);
}
// ===== 结束新增 =====

// ===== 新增函数：创建独立窗口（兼容单/双显示器）=====
void UDecisionManagerComponent::CreateSeparateWindowForWidget(UUserWidget* Widget)
{
    if (!Widget || !FSlateApplication::IsInitialized())
    {
        UE_LOG(LogTemp, Error, TEXT("[DecisionManager] Cannot create window: Invalid widget or Slate not initialized"));
        return;
    }

    // 获取显示器信息
    FDisplayMetrics DisplayMetrics;
    FSlateApplication::Get().GetDisplayMetrics(DisplayMetrics);

    // 窗口尺寸
    const float WindowWidth = 1920.0f;
    const float WindowHeight = 720.0f;

    // 计算窗口位置
    FVector2D WindowPosition;
    bool bHasSecondMonitor = false;

    // 检查是否有第二显示器
    // 方法：虚拟屏幕宽度 > 主显示器宽度，说明有多个显示器
    if (DisplayMetrics.VirtualDisplayRect.Right > DisplayMetrics.PrimaryDisplayWidth)
    {
        // 有第二显示器，将窗口放在右侧显示器的左上角
        WindowPosition = FVector2D(DisplayMetrics.PrimaryDisplayWidth, 0);
        bHasSecondMonitor = true;
        UE_LOG(LogTemp, Display, TEXT("[DecisionManager] Second monitor detected!"));
        UE_LOG(LogTemp, Display, TEXT("[DecisionManager] Primary Display: %d x %d"),
            DisplayMetrics.PrimaryDisplayWidth,
            DisplayMetrics.PrimaryDisplayHeight);
        UE_LOG(LogTemp, Display, TEXT("[DecisionManager] Virtual Display: %d x %d"),
            DisplayMetrics.VirtualDisplayRect.Right,
            DisplayMetrics.VirtualDisplayRect.Bottom);
    }
    else
    {
        // 只有一个显示器，居中显示窗口
        WindowPosition = FVector2D(
            (DisplayMetrics.PrimaryDisplayWidth - WindowWidth) / 2.0f,
            (DisplayMetrics.PrimaryDisplayHeight - WindowHeight) / 2.0f
        );
        bHasSecondMonitor = false;
        UE_LOG(LogTemp, Warning, TEXT("[DecisionManager] Only one monitor detected, centering window"));
        UE_LOG(LogTemp, Display, TEXT("[DecisionManager] Primary Display: %d x %d"),
            DisplayMetrics.PrimaryDisplayWidth,
            DisplayMetrics.PrimaryDisplayHeight);
    }

    UE_LOG(LogTemp, Display, TEXT("[DecisionManager] Window Position: (%.0f, %.0f)"),
        WindowPosition.X,
        WindowPosition.Y);

    // 创建新窗口 - 固定1920x720像素，禁止调整大小
    TSharedRef<SWindow> NewWindow = SNew(SWindow)
        .Title(FText::FromString(bHasSecondMonitor ? 
            TEXT("Recommendation System - Second Monitor") : 
            TEXT("Recommendation System - Centered")))
        .ClientSize(FVector2D(WindowWidth, WindowHeight))  // 固定为1920x720
        .ScreenPosition(WindowPosition)                     // 设置窗口位置
        .SupportsMaximize(false)                            // 禁用最大化
        .SupportsMinimize(true)                             // 允许最小化
        .IsTopmostWindow(false)
        .SizingRule(ESizingRule::FixedSize)                 // 固定大小，不允许调整
        .AutoCenter(EAutoCenter::None);                     // 不自动居中，使用自定义位置

    // 将 UMG Widget 转换为 Slate Widget
    TSharedRef<SWidget> SlateWidget = Widget->TakeWidget();

    // 设置窗口内容
    NewWindow->SetContent(SlateWidget);

    // 将窗口添加到应用程序
    FSlateApplication::Get().AddWindow(NewWindow);

    // 保存窗口引用以便后续管理
    RecommendationWindow = NewWindow;

    UE_LOG(LogTemp, Display, TEXT("[DecisionManager] Separate window created successfully (1920x720, fixed size, %s)"),
        bHasSecondMonitor ? TEXT("on second monitor") : TEXT("centered on primary monitor"));
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

    // ===== 新增：停止正在播放的内容音频并恢复原始图片 =====
    if (CurrentContentAudioComponent && CurrentContentAudioComponent->IsPlaying())
    {
        CurrentContentAudioComponent->Stop();
        CurrentContentAudioComponent = nullptr;
        
        // 清除内容显示定时器
        GetWorld()->GetTimerManager().ClearTimer(ContentDisplayTimerHandle);
        
        // 恢复原始图片
        if (RecMgr && RecMgr->RecommendationWidget)
        {
            RecMgr->RecommendationWidget->ClearContent();
            UE_LOG(LogTemp, Display, TEXT("[Decision Manager] Previous content audio stopped and image restored due to new recommendation"));
        }
    }
    // ===== 结束新增 =====

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
    
    // ===== 修改：保存当前播放的音频组件 =====
    CurrentContentAudioComponent = RecMgr->GetLastContentAudioComponent();
    
    // 获取 Content_Sound 的时长并设置定时器
    if (RecMgr->Row && !RecMgr->Row->Content_Sound.IsNull())
    {
        if (USoundBase* ContentSound = RecMgr->Row->Content_Sound.LoadSynchronous())
        {
            float SoundDuration = ContentSound->GetDuration();
            
            // 在音频播放结束时隐藏内容图片
            GetWorld()->GetTimerManager().SetTimer(
                ContentDisplayTimerHandle,
                this,
                &UDecisionManagerComponent::OnContentSoundEnd,
                SoundDuration,
                false
            );
            
            UE_LOG(LogTemp, Display, TEXT("[Decision Manager] Content will be hidden after %.2f seconds"), SoundDuration);
        }
    }
    // ===== 结束修改 =====
    
    GetWorld()->GetTimerManager().SetTimer(
        AgentReactionAnimationHandle,
        this,
        &UDecisionManagerComponent::OnAgentReactionEnd,
        AgentReactionAnimationTime,
        false
    );

    // TODO: 处理"接受"的游戏逻辑
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

    // TODO: 处理"拒绝"的游戏逻辑
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

    // TODO: 处理"拒绝"的游戏逻辑
    UE_LOG(LogTemp, Display, TEXT("[Decision Manager] User ignored the recommendation"));

    // Play ignore effect
    bWaitingForResponse = false;
    StartCooldown();
}

void UDecisionManagerComponent::OnAgentReactionEnd()
{
    // 修改：只清除反应图片和推荐文本内容，保留主背景
    if (RecMgr && RecMgr->RecommendationWidget)
    {
        RecMgr->RecommendationWidget->ClearReactionAndRecommendation();
        UE_LOG(LogTemp, Display, TEXT("[DecisionManager] Agent reaction and recommendation cleared, background preserved"));
    }
}

void UDecisionManagerComponent::OnContentSoundEnd()
{
    // 只清除内容图片
    if (RecMgr && RecMgr->RecommendationWidget)
    {
        RecMgr->RecommendationWidget->ClearContent();
        UE_LOG(LogTemp, Display, TEXT("[DecisionManager] Content image cleared after sound finished"));
    }
    
    // ===== 新增：清除音频组件引用 =====
    CurrentContentAudioComponent = nullptr;
    // ===== 结束新增 =====
}