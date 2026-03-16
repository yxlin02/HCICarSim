#include "Env_TrafficLightManagerComponent.h"
#include "Env_TrafficLight.h"
#include "Kismet/GameplayStatics.h"

UEnv_TrafficLightManagerComponent::UEnv_TrafficLightManagerComponent()
{
    PrimaryComponentTick.bCanEverTick = true;

    PhaseTimeAccumulated = 0.f;
    CurrentPhase = EGlobalTrafficPhase::NS_Straight_Ped_Green;
    PendingPhase = EGlobalTrafficPhase::NS_Left_Green;
}

void UEnv_TrafficLightManagerComponent::BeginPlay()
{
    Super::BeginPlay();
    
    // 自动搜当前关卡里的所有 TrafficLightActor
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AEnv_TrafficLight::StaticClass(), FoundActors);
    
    UE_LOG(LogTemp, Display, TEXT("Found %d traffic lights"), FoundActors.Num());

    for (AActor* Actor : FoundActors)
    {
        if (AEnv_TrafficLight* TL = Cast<AEnv_TrafficLight>(Actor))
        {
            RegisteredLights.Add(TL);
        }
    }

    BroadcastPhaseToAll();
}

void UEnv_TrafficLightManagerComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    PhaseTimeAccumulated += DeltaTime;
    const float TargetDuration = GetCurrentPhaseDuration();

    if (PhaseTimeAccumulated >= TargetDuration)
    {
        PhaseTimeAccumulated -= TargetDuration;
        SwitchPhase();
        BroadcastPhaseToAll();
    }
}

void UEnv_TrafficLightManagerComponent::RegisterTrafficLight(AEnv_TrafficLight* TrafficLight)
{
    if (TrafficLight && !RegisteredLights.Contains(TrafficLight))
    {
        RegisteredLights.Add(TrafficLight);
        TrafficLight->ApplyPhase(CurrentPhase);
    }
}

void UEnv_TrafficLightManagerComponent::UnregisterTrafficLight(AEnv_TrafficLight* TrafficLight)
{
    if (TrafficLight)
    {
        RegisteredLights.Remove(TrafficLight);
    }
}

void UEnv_TrafficLightManagerComponent::ForceSetPhase(EGlobalTrafficPhase NewPhase, bool bResetTimer)
{
    CurrentPhase = NewPhase;

    if (bResetTimer)
    {
        PhaseTimeAccumulated = 0.f;
    }

    BroadcastPhaseToAll();
}

void UEnv_TrafficLightManagerComponent::SwitchPhase()
{
    if (CurrentPhase == EGlobalTrafficPhase::All_Red)
    {
        // 全红期结束 → 切换到之前预定的下一个正式相位
        CurrentPhase = PendingPhase;
    }
    else
    {
        // 正式相位结束 → 先计算出下一个正式相位，存入 PendingPhase，然后进入全红
        switch (CurrentPhase)
        {
            case EGlobalTrafficPhase::NS_Straight_Ped_Green:
                PendingPhase = EGlobalTrafficPhase::NS_Left_Green;
                break;
            case EGlobalTrafficPhase::NS_Left_Green:
                PendingPhase = EGlobalTrafficPhase::EW_Straight_Ped_Green;
                break;
            case EGlobalTrafficPhase::EW_Straight_Ped_Green:
                PendingPhase = EGlobalTrafficPhase::EW_Left_Green;
                break;
            case EGlobalTrafficPhase::EW_Left_Green:
            default:
                PendingPhase = EGlobalTrafficPhase::NS_Straight_Ped_Green;
                break;
        }

        CurrentPhase = EGlobalTrafficPhase::All_Red;
    }

    OnTrafficPhaseChanged.Broadcast(CurrentPhase);
}

void UEnv_TrafficLightManagerComponent::BroadcastPhaseToAll()
{
    for (int32 i = RegisteredLights.Num() - 1; i >= 0; --i)
    {
        AEnv_TrafficLight* TL = RegisteredLights[i];
        if (!IsValid(TL))
        {
            RegisteredLights.RemoveAt(i);
            continue;
        }

        TL->ApplyPhase(CurrentPhase);
    }
}

float UEnv_TrafficLightManagerComponent::GetCurrentPhaseDuration() const
{
    switch (CurrentPhase)
    {
        case EGlobalTrafficPhase::NS_Straight_Ped_Green:
        case EGlobalTrafficPhase::EW_Straight_Ped_Green:
            return StraightDuration;

        case EGlobalTrafficPhase::NS_Left_Green:
        case EGlobalTrafficPhase::EW_Left_Green:
            return LeftDuration;

        case EGlobalTrafficPhase::All_Red:
            return AllRedDuration;

        default:
            return StraightDuration;
    }
}

EGlobalTrafficPhase UEnv_TrafficLightManagerComponent::GetCurrentPhase()
{
    return CurrentPhase;
};
