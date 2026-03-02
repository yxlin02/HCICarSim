// Fill out your copyright notice in the Description page of Project Settings.


#include "Traffic_AICarManagerComponent.h"
#include "Traffic_AICar.h"
#include "AgentGameInstance.h"
#include "Env_RoadLaneTransfer.h"
#include "Env_RoadLane.h"
#include "Components/BoxComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "RecommendationManager.h"
#include "Math/RandomStream.h"
#include "Subsystems/GameInstanceSubsystem.h"

namespace
{
    static uint64 MakeLaneIdxKey(const AEnv_RoadLane* Lane, int32 LaneIdx)
    {
        return (uint64(GetTypeHash(Lane)) << 32) ^ uint64(uint32(LaneIdx));
    }
}

UTraffic_AICarManagerComponent::UTraffic_AICarManagerComponent() 
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UTraffic_AICarManagerComponent::BeginPlay()
{
    Super::BeginPlay();
    
    TArray<AActor*> Found;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AEnv_RoadLaneTransfer::StaticClass(), Found);

    for (AActor* A : Found)
    {
        if (AEnv_RoadLaneTransfer* T = Cast<AEnv_RoadLaneTransfer>(A))
        {
            T->OnLoopTransferTriggered.AddDynamic(this, &UTraffic_AICarManagerComponent::OnAnyLoopTransferTriggered);
            // UE_LOG(LogTemp, Display, TEXT("[Traffic Manager] Found Road Transfer"));
        }
    }
    
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
    

    if (UAgentGameInstance* AgentGI = Cast<UAgentGameInstance>(GI))
    {
        if (!TrafficMappingTable)
        {
            TrafficMappingTable = AgentGI->DefaultTrafficMappingTable;
            
            if (!TrafficMappingTable)
            {
                UE_LOG(LogTemp, Error, TEXT("Traffic mapping table not assigned!"));
                return;
            }
            else UE_LOG(LogTemp, Display, TEXT("[Game Instance] Traffic mapping table read success."));
        }
    }
    
    SceneID = RecMgr->GetCurrentSceneID();
    
    SpawnCar();
}

void UTraffic_AICarManagerComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bUsePawnApproachingEffect) return;

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    APlayerController* PC = World->GetFirstPlayerController();
    if (!PC)
    {
        return;
    }

    APawn* Pawn = PC->GetPawn();
    if (!Pawn)
    {
        return;
    }

    const FVector PawnLoc = Pawn->GetActorLocation();
    const FVector PawnForward = Pawn->GetActorForwardVector().GetSafeNormal();

    const float SafeMaxDist = (MaxAffectDist > KINDA_SMALL_NUMBER)
        ? MaxAffectDist
        : 1.0f;

    const float InvMaxDist = 1.0f / SafeMaxDist;
    const float ClampedMinEffect = FMath::Clamp(MinApproachEffect, 0.0f, 1.0f);
    const float SafeFalloffPower = (FalloffPower > 0.0f) ? FalloffPower : 1.0f;

    for (auto It = AliveCars.CreateIterator(); It; ++It)
    {
        const TWeakObjectPtr<ATraffic_AICar>& WeakCar = *It;

        if (!WeakCar.IsValid())
        {
            It.RemoveCurrent();
            continue;
        }

        ATraffic_AICar* Car = WeakCar.Get();
        if (!Car)
        {
            It.RemoveCurrent();
            continue;
        }

        const FVector Offset = Car->GetActorLocation() - PawnLoc;
        const float   ForwardDist = FVector::DotProduct(Offset, PawnForward);

        float Effect = 1.0f;

        if (ForwardDist > 0.0f && ForwardDist < SafeMaxDist)
        {
            const float t = ForwardDist * InvMaxDist;

            const float tPow = FMath::Pow(t, SafeFalloffPower);

            Effect = ClampedMinEffect + (1.0f - ClampedMinEffect) * tPow;
        }
        else
        {
            Effect = 1.0f;
        }

        Effect = FMath::Clamp(1-Effect, MinApproachEffect, 1.f);

        //Car->PawnApproachingEffect = Effect;
        Car->SetPawnApproachingEffect(Effect);
    }
}

void UTraffic_AICarManagerComponent::RegisterCar(ATraffic_AICar* Car)
{
    if (!IsValid(Car)) return;

    AliveCars.Add(Car);
    UE_LOG(LogTemp, Warning, TEXT("[AI Car Manager] Registering!"));

    Car->OnDestroyed.RemoveDynamic(this, &UTraffic_AICarManagerComponent::HandleCarDestroyed);
    Car->OnDestroyed.AddDynamic(this, &UTraffic_AICarManagerComponent::HandleCarDestroyed);
}

void UTraffic_AICarManagerComponent::UnregisterCar(ATraffic_AICar* Car)
{
    if (!Car) return;

    AliveCars.Remove(Car);
    Car->OnDestroyed.RemoveDynamic(this, &UTraffic_AICarManagerComponent::HandleCarDestroyed);
}

void UTraffic_AICarManagerComponent::HandleCarDestroyed(AActor* DestroyedActor)
{
    ATraffic_AICar* Car = Cast<ATraffic_AICar>(DestroyedActor);
    if (!Car) return;

    AliveCars.Remove(Car);
}

void UTraffic_AICarManagerComponent::Compact()
{
    for (auto It = AliveCars.CreateIterator(); It; ++It)
    {
        if (!It->IsValid())
        {
            It.RemoveCurrent();
        }
    }
}

void UTraffic_AICarManagerComponent::GetAliveCars(TArray<ATraffic_AICar*>& OutCars)
{
    OutCars.Reset();
    Compact();

    for (const TWeakObjectPtr<ATraffic_AICar>& W : AliveCars)
    {
        if (W.IsValid())
        {
            OutCars.Add(W.Get());
        }
    }
}

TWeakObjectPtr<UTraffic_AICarManagerComponent> UTraffic_AICarManagerComponent::CachedMgr;

UTraffic_AICarManagerComponent*
UTraffic_AICarManagerComponent::GetTrafficManager(const UObject* WorldContext)
{
    if (CachedMgr.IsValid())
    {
        return CachedMgr.Get();
    }

    if (!WorldContext) return nullptr;
    UWorld* World = WorldContext->GetWorld();
    if (!World) return nullptr;

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!IsValid(Actor)) continue;

        if (UTraffic_AICarManagerComponent* Comp =
            Actor->FindComponentByClass<UTraffic_AICarManagerComponent>())
        {
            CachedMgr = Comp;
            return Comp;
        }
    }

    return nullptr;
}

// AI Car Spawner
int32 UTraffic_AICarManagerComponent::DensityToCarsPerLane(ELaneCarDensity D) const
{
    switch (D)
    {
        case ELaneCarDensity::Low:  return 1;
        case ELaneCarDensity::Mid:  return 4;
        case ELaneCarDensity::High: return 8;
        default: return 2;
    }
}

float UTraffic_AICarManagerComponent::VelocityToTargetSpeed(ELaneCarVelocity V) const
{
    switch (V)
    {
        case ELaneCarVelocity::Slow: return 600.f;
        case ELaneCarVelocity::Mid:  return 800.f;
        case ELaneCarVelocity::Fast: return 1200.f;
        default: return 800.f;
    }
}

TSubclassOf<ATraffic_AICar> UTraffic_AICarManagerComponent::PickCarClass(int32 Seed) const
{
    if (CarSeedClasses.Num() == 0) return nullptr;
    const int32 Idx = FMath::Abs(Seed) % CarSeedClasses.Num();
    return CarSeedClasses[Idx];
}

bool UTraffic_AICarManagerComponent::TryGetLaneIndexConfig(
    const AEnv_RoadLane* Lane,
    int32 LaneIdx,
    FLaneIndexSpawnConfig& Out) const
{
    Out = FLaneIndexSpawnConfig{};

    if (!RecMgr)
    {
        UE_LOG(LogTemp, Warning, TEXT("TryGetLaneIndexConfig: RecMgr is null."));
        return false;
    }

    if (!TrafficMappingTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("TryGetLaneIndexConfig: TrafficMappingTable is null."));
        return false;
    }

    const int32 CurrentTrafficId = RecMgr->GetCurrentRowId();

    int32 CurrentCategory    = 0;
    int32 CurrentSubcategory = 0;
    int32 CurrentItem        = 0;

    RecMgr->DecomposeRecommendationID(
        CurrentTrafficId,
        CurrentCategory,
        CurrentSubcategory,
        CurrentItem
    );
    
    UE_LOG(LogTemp, Warning, TEXT("[AI Car Manager] CurrentCategory = %d, CurrentSubcategory = %d"),
           CurrentCategory,
           CurrentSubcategory
           );

    TArray<FRecTrafficRow*> AllRows;
    TrafficMappingTable->GetAllRows<FRecTrafficRow>(
        TEXT("TrafficMappingLookup"),
        AllRows
    );

    FRecTrafficRow* MatchedRow = nullptr;
    
    UE_LOG(LogTemp, Warning, TEXT("[AI Car Manager] Looking for matched row now"));
    for (FRecTrafficRow* Row : AllRows)
    {
        if (!Row) continue;

        if (Row->Category    != CurrentCategory ||
            Row->Subcategory != CurrentSubcategory)
        {
            continue;
        }

        if (Row->LaneIndex >= 0 && Row->LaneIndex != LaneIdx)
        {
            continue;
        }

        MatchedRow = Row;
        break;
    }

    if (!MatchedRow)
    {
        UE_LOG(LogTemp, Warning, TEXT("[AI Car Manager] No matched Row"));
        return false;
    }

    if (MatchedRow->Mode == ERecLaneMode::Random)
    {
        return false;
    }

    Out.LaneIndex   = MatchedRow->LaneIndex;
    Out.Density  = MatchedRow->CarDensity;
    Out.Velocity = MatchedRow->CarVelocity;
    
    if (SceneID == 1)
    {
        int32 DensityInt = static_cast<int32>(Out.Density);
        DensityInt = FMath::Max(0, DensityInt - 1);
        Out.Density = static_cast<ELaneCarDensity>(DensityInt);
    }
    else 
    {
        int32 DensityInt = static_cast<int32>(Out.Density);
        DensityInt = FMath::Min(2, DensityInt + 1);
        Out.Density = static_cast<ELaneCarDensity>(DensityInt);
    }
    
    UE_LOG(LogTemp, Display,
        TEXT("[AI Car Manager] LaneIdx=%d Density=%d Velocity=%d"),
        LaneIdx,
        (int32)Out.Density,
        (int32)Out.Velocity);

    return true;
}

FLaneIndexSpawnConfig UTraffic_AICarManagerComponent::MakeFallbackConfig(
    const AEnv_RoadLane* Lane, int32 LaneIdx, bool bUseFixedSeeds) const
{
    FLaneIndexSpawnConfig Cfg;
    
    FRandomStream Rng;

    if (bUseFixedSeeds)
    {
        const int32 Seed = GetTypeHash(Lane) ^ (LaneIdx * 2654435761u);
        Rng.Initialize(Seed);
    }
    else
    {
        Rng.GenerateNewSeed();
    }

    const int32 D = Rng.RandRange(0, 2);
    const int32 V = Rng.RandRange(0, 2);
    
    Cfg.LaneIndex = LaneIdx;
    Cfg.Density  = static_cast<ELaneCarDensity>(D);
    Cfg.Velocity = static_cast<ELaneCarVelocity>(V);
    
    if (SceneID == 1)
    {
        int32 DensityInt = static_cast<int32>(Cfg.Density);
        DensityInt = FMath::Max(0, DensityInt - 2);
        Cfg.Density = static_cast<ELaneCarDensity>(DensityInt);
    }
    return Cfg;
}

void UTraffic_AICarManagerComponent::SpawnCar()
{
    SpawnedLaneIdxKeys.Reset();
    if (!GetWorld()) return;

    for (AEnv_RoadLane* Lane : TargetLanes)
    {
        if (!Lane) continue;
        if (!Lane->Spawner) continue;

        const int32 NumLanes = FMath::Max(1, Lane->LaneNumber);

        // Spawner box 的局部空间：X forward, Y lateral
        const FTransform SpawnXf = Lane->Spawner->GetComponentTransform();
        const FVector Ext = Lane->Spawner->GetUnscaledBoxExtent();
        const float HalfX = Ext.X;
        const float HalfY = Ext.Y;

        // 每条车道宽度（按 spawner 的 Y 总宽来分）
        const float TotalWidth = HalfY * 2.f;
        const float LaneWidth  = TotalWidth / NumLanes;

        // forward 排列间距：根据 density 估算（也可做成可调参数）
        const float MinGap = 600.f;
        const float UsableLength = FMath::Max(0.f, HalfX * 2.f - 200.f);

        for (int32 LaneIdx = 0; LaneIdx < NumLanes; ++LaneIdx)
        {
            const uint64 Key = MakeLaneIdxKey(Lane, LaneIdx);
            if (SpawnedLaneIdxKeys.Contains(Key))
            {
                continue;
            }
            SpawnedLaneIdxKeys.Add(Key);

            FLaneIndexSpawnConfig Cfg;
            if (!TryGetLaneIndexConfig(Lane, LaneIdx, Cfg))
            {
                Cfg = MakeFallbackConfig(Lane, LaneIdx);
            }

            const int32 CarsPerLane = DensityToCarsPerLane(Cfg.Density);
            const float TargetSpeed = VelocityToTargetSpeed(Cfg.Velocity);
            
            const float CenterY = -HalfY + (LaneIdx + 0.5f) * LaneWidth;
            const float StepX = FMath::Max(MinGap, UsableLength / FMath::Max(1, CarsPerLane));

            const float JitterAmount = StepX * 0.3f;
            FRandomStream Rng(GetTypeHash(Lane) ^ LaneIdx * 97);

            for (int32 k = 0; k < CarsPerLane; ++k)
            {
                float BaseX = -HalfX + 100.f + k * StepX;
                float Jitter = Rng.FRandRange(-JitterAmount, JitterAmount);

                const float LocalX = BaseX + Jitter;

                const FVector LocalPos(LocalX, CenterY, 0.f);

                const FVector WorldPos = SpawnXf.TransformPosition(LocalPos);
                const FVector LaneForward = Lane->GetActorForwardVector().GetSafeNormal();
                FRotator WorldRot = FRotationMatrix::MakeFromX(LaneForward).Rotator();

                const TSubclassOf<ATraffic_AICar> CarClass = PickCarClass(
                    GetTypeHash(Lane) + LaneIdx * 131 + k * 17
                );
                if (!CarClass) continue;

                FActorSpawnParameters Params;
                Params.SpawnCollisionHandlingOverride =
                    ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

                ATraffic_AICar* Car = GetWorld()->SpawnActor<ATraffic_AICar>(
                    CarClass, WorldPos, WorldRot, Params
                );
                if (!Car) continue;
                
                Car->SetActorRotation(WorldRot);

                Car->SetCurrentLane(Lane);
                Car->SetLaneIndex(LaneIdx);
                Car->SetDesiredSpeed(TargetSpeed);
            }
        }
    }
}

void UTraffic_AICarManagerComponent::OnAnyLoopTransferTriggered(
    AEnv_RoadLaneTransfer* Transfer,
    AActor* OtherActor,
    AEnv_RoadLane* DestLane)
{
    if (!bCanSpawn) return;

    bCanSpawn = false;
    
    // Decide next trigger recommendation
    // Read density and velocity according to designated recommendation
    if (!RecMgr->UpdateDesignatedRowToCurrentRow())
    {
        UE_LOG(LogTemp, Display, TEXT("[AI Car Manager] Failed to update designated row"));
        RecMgr->UpdateRandomRowToCurrentRow();
    }
    
    SpawnCar();
    UE_LOG(LogTemp, Display, TEXT("[AI Car Manager] AI Cars Spawned"));

    GetWorld()->GetTimerManager().SetTimer(
        SpawnCooldownHandle,
        this,
        &UTraffic_AICarManagerComponent::ResetSpawnCooldown,
        SpawnCooldown,
        false
    );
}

void UTraffic_AICarManagerComponent::ResetSpawnCooldown()
{
    bCanSpawn = true;
}

