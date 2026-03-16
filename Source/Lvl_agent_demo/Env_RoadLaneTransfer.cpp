#include "Env_RoadLaneTransfer.h"
#include "Components/BoxComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/MovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/MovementComponent.h"
#include "Traffic_AICar.h"
#include "Traffic_AutoDriving.h"

AEnv_RoadLaneTransfer::AEnv_RoadLaneTransfer()
{
    // 创建 LoopTrigger，挂在 RoadSurface 上
    LoopTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("LoopTrigger"));
    LoopTrigger->SetupAttachment(RoadSurface);

    // 只做 Overlap，用于逻辑触发
    LoopTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    LoopTrigger->SetCollisionObjectType(ECC_WorldDynamic);
    LoopTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
    
    LoopTrigger->SetCollisionResponseToChannel(ECC_Pawn,    ECR_Overlap);
    LoopTrigger->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);
    LoopTrigger->SetGenerateOverlapEvents(true);
    
    // Construct DeadZone
    NearDeadZone = CreateDefaultSubobject<UBoxComponent>(TEXT("NearDeadZone"));
    NearDeadZone->SetupAttachment(RoadSurface);
    
    NearDeadZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    NearDeadZone->SetCollisionObjectType(ECC_WorldDynamic);
    NearDeadZone->SetCollisionResponseToAllChannels(ECR_Ignore);
    
    NearDeadZone->SetCollisionResponseToChannel(ECC_Pawn,    ECR_Overlap);
    NearDeadZone->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);
    NearDeadZone->SetGenerateOverlapEvents(true);
    
    EndDeadZone = CreateDefaultSubobject<UBoxComponent>(TEXT("EndDeadZone"));
    EndDeadZone->SetupAttachment(RoadSurface);
    
    EndDeadZone->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    EndDeadZone->SetCollisionObjectType(ECC_WorldDynamic);
    EndDeadZone->SetCollisionResponseToAllChannels(ECR_Ignore);
    
    EndDeadZone->SetCollisionResponseToChannel(ECC_Pawn,    ECR_Overlap);
    EndDeadZone->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);
    EndDeadZone->SetGenerateOverlapEvents(true);
}

void AEnv_RoadLaneTransfer::BeginPlay()
{
    Super::BeginPlay();
    LoopTrigger->OnComponentBeginOverlap.AddDynamic(
        this,
        &AEnv_RoadLaneTransfer::OnLoopTriggerBeginOverlap
    );
    NearDeadZone->OnComponentBeginOverlap.AddDynamic(
        this,
        &AEnv_RoadLaneTransfer::OnDeadZoneBeginOverlap
    );
    EndDeadZone->OnComponentBeginOverlap.AddDynamic(
        this,
        &AEnv_RoadLaneTransfer::OnDeadZoneBeginOverlap
    );
    // UE_LOG(LogTemp, Display, TEXT("[Decision Trigger] Alive!"));
    AICar_Mgr = UTraffic_AICarManagerComponent::GetTrafficManager(this);
    if (!AICar_Mgr)
    {
        UE_LOG(LogTemp, Error, TEXT("[Lane Transfer] Can't find AI car manager!"));
    }
}

void AEnv_RoadLaneTransfer::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    if (!RoadSurface)
    {
        return;
    }

    const FVector BoxExtentRoad = RoadSurface->GetUnscaledBoxExtent();
    const float HalfLength = BoxExtentRoad.X;
    const float HalfWidth  = BoxExtentRoad.Y;
    RoadLength = HalfLength * 2.f;

    LoopTriggerLengthFromStart = FMath::Clamp(LoopTriggerLengthFromStart, 0.f, RoadLength);
    NearDeadTriggerLengthFromLoop = FMath::Clamp(
        NearDeadTriggerLengthFromLoop,
        0.f,
        FMath::Max(0.f, RoadLength - LoopTriggerLengthFromStart)
    );

    // -------- LoopTrigger --------
    if (LoopTrigger)
    {
        const float TriggerThickness   = 200.f;        // cm
        const float TriggerHalfLength  = TriggerThickness * 0.5f;
        const float TriggerHalfWidth   = HalfWidth * 0.9;
        const float TriggerHalfHeight  = 100.f;

        LoopTrigger->SetBoxExtent(FVector(TriggerHalfLength, TriggerHalfWidth, TriggerHalfHeight), true);

        const float XStart = -HalfLength;
        const float TriggerCenterX = XStart + LoopTriggerLengthFromStart + TriggerHalfLength;
        const float TriggerCenterZ = TriggerHalfHeight;

        LoopTrigger->SetRelativeLocation(FVector(TriggerCenterX, 0.f, TriggerCenterZ));
    }

    // -------- NearDeadZone & EndDeadZone --------
    if (NearDeadZone && EndDeadZone)
    {
        const float TriggerThickness   = 100.f;
        const float TriggerHalfLength  = TriggerThickness * 0.5f;
        const float TriggerHalfWidth   = HalfWidth;
        const float TriggerHalfHeight  = 100.f;

        NearDeadZone->SetBoxExtent(FVector(TriggerHalfLength, TriggerHalfWidth, TriggerHalfHeight), true);
        EndDeadZone ->SetBoxExtent(FVector(TriggerHalfLength, TriggerHalfWidth, TriggerHalfHeight), true);

        const float XStart = -HalfLength;

        const float NearDeadCenterX =
            XStart + (LoopTriggerLengthFromStart + NearDeadTriggerLengthFromLoop) + TriggerHalfLength;

        const float EndMargin = 50.f;
        const float EndDeadCenterX =
            (+HalfLength) - EndMargin - TriggerHalfLength;

        const float CenterZ = TriggerHalfHeight;

        NearDeadZone->SetRelativeLocation(FVector(NearDeadCenterX, 0.f, CenterZ));
        EndDeadZone ->SetRelativeLocation(FVector(EndDeadCenterX,  0.f, CenterZ));
    }
}


void AEnv_RoadLaneTransfer::OnLoopTriggerBeginOverlap(
    UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult
)
{
    if (!OtherActor || OtherActor == this)
    {
        return;
    }
    
    if(OtherActor->IsA(ATraffic_AICar::StaticClass())) return;

    APawn* Pawn = Cast<APawn>(OtherActor);
    if (!Pawn)
    {
        return;
    }

    if (RecentlyTeleportedActors.Contains(OtherActor))
    {
        return;
    }
    
    RecentlyTeleportedActors.Add(OtherActor);

    UE_LOG(LogTemp, Display,
           TEXT("[Transfer Trigger] BeginOverlap: Self=%s Actor=%s Comp=%s"),
           *GetName(),
           *OtherActor->GetName(),
           *GetNameSafe(OtherComp));

    if (!TransferDestination)
    {
        UE_LOG(LogTemp, Warning, TEXT("AEnv_RoadLaneTransfer: No TransferDestination set."));
        return;
    }

    const FVector PawnLoc = Pawn->GetActorLocation();
    const FVector PawnFwd = Pawn->GetActorForwardVector();

    TArray<ATraffic_AICar*> CarsToTeleport;
    TArray<ATraffic_AICar*> CarsToDestroy;

    TArray<ATraffic_AICar*> AliveCars;
    AICar_Mgr->GetAliveCars(AliveCars);

    for (ATraffic_AICar* Car : AliveCars)
    {
        if (!IsValid(Car)) continue;

        const float ForwardDist =
            FVector::DotProduct((Car->GetActorLocation() - PawnLoc), PawnFwd);

        if (ForwardDist < -300.f || ForwardDist > 5000.f)
            CarsToDestroy.Add(Car);
        else
            CarsToTeleport.Add(Car);
    }

    for (ATraffic_AICar* Car : CarsToDestroy)
        if (IsValid(Car)) Car->Destroy();

    for (ATraffic_AICar* Car : CarsToTeleport)
        if (IsValid(Car)) TeleportActorToDestinationLane(Car);
    
    TeleportActorToDestinationLane(OtherActor);
    /*if (UTraffic_AutoDriving* InAuto = OtherActor->FindComponentByClass<UTraffic_AutoDriving>())
    {
        InAuto->SetCurrentLane(TransferDestination);
		InAuto->bGo = true;
    }*/
    
    OnLoopTransferTriggered.Broadcast(this, OtherActor, TransferDestination);

    FTimerHandle TimerHandle;
    GetWorldTimerManager().SetTimer(
        TimerHandle,
        [this, WeakPawn = TWeakObjectPtr<AActor>(Pawn)]()
        {
            if (WeakPawn.IsValid())
                RecentlyTeleportedActors.Remove(WeakPawn.Get());
        },
        0.3f,
        false
    );
}

void AEnv_RoadLaneTransfer::OnDeadZoneBeginOverlap(
    UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult
)
{
    ATraffic_AICar* AI_Car = Cast<ATraffic_AICar>(OtherActor);

    if (!IsValid(AI_Car) || OtherActor == this || !OtherComp)
    {
        return;
    }

    if (OtherComp != AI_Car->GetMesh())
    {
        return;
    }

    AI_Car->Destroy();
}

void AEnv_RoadLaneTransfer::TeleportActorToDestinationLane(AActor* Actor)
{
    if (!Actor || !TransferDestination || !RoadSurface || !TransferDestination->RoadSurface)
    {
        return;
    }

    // -------- 0. 关掉相机 Lag --------
    TArray<USpringArmComponent*> SpringArms;
    Actor->GetComponents(SpringArms);

    struct FSpringArmLagState
    {
        USpringArmComponent* Arm = nullptr;
        bool bCameraLag = false;
        bool bRotLag = false;
    };

    TArray<FSpringArmLagState> SavedStates;
    SavedStates.Reserve(SpringArms.Num());

    for (USpringArmComponent* Arm : SpringArms)
    {
        if (!Arm) continue;

        FSpringArmLagState State;
        State.Arm = Arm;
        State.bCameraLag = Arm->bEnableCameraLag;
        State.bRotLag = Arm->bEnableCameraRotationLag;
        SavedStates.Add(State);

        Arm->bEnableCameraLag = false;
        Arm->bEnableCameraRotationLag = false;
    }

    const FVector WorldLocation = Actor->GetActorLocation();

    // -------- 1. 源路 / 目标路 --------
    const FTransform SrcLaneXf = RoadSurface->GetComponentTransform();
    const float SrcHalfLength = RoadLength * 0.5f;
    const float SrcHalfWidth = RoadWidth * 0.5f;

    const FTransform DstLaneXf = TransferDestination->RoadSurface->GetComponentTransform();
    const float DstHalfLength = TransferDestination->RoadLength * 0.5f;
    const float DstHalfWidth = TransferDestination->RoadWidth * 0.5f;

    const FVector LocalOnSrc = SrcLaneXf.InverseTransformPosition(WorldLocation);

    const float x_src = LocalOnSrc.X;
    const float y_src = LocalOnSrc.Y;

    const float HeightOffset = WorldLocation.Z - SrcLaneXf.GetLocation().Z;

    float x_dst = x_src * (DstHalfLength / SrcHalfLength);
    float y_dst = y_src;

    FVector LocalOnDst(x_dst, y_dst, 0.f);
    FVector NewWorldLocation = DstLaneXf.TransformPosition(LocalOnDst);
    NewWorldLocation.Z += HeightOffset;

    UE_LOG(LogTemp, Warning, TEXT("==== Transfer Debug ===="));
    UE_LOG(LogTemp, Warning, TEXT("LocalOnDst=(%.2f, %.2f, %.2f)"), x_dst, y_dst, NewWorldLocation.Z);

    // -------- 4. 朝向 --------
    const FVector WorldForward = Actor->GetActorForwardVector();
    const FVector LocalForwardSrc =
        SrcLaneXf.InverseTransformVectorNoScale(WorldForward);

    FVector NewWorldForward =
        DstLaneXf.TransformVectorNoScale(LocalForwardSrc).GetSafeNormal();

    const FRotator NewWorldRotation = NewWorldForward.Rotation();

    // -------- 5. 速度 --------
    UMovementComponent* MoveComp =
        Actor->FindComponentByClass<UMovementComponent>();
    USkeletalMeshComponent* VehicleMesh =
        Actor->FindComponentByClass<USkeletalMeshComponent>();

    FVector NewVelocity = FVector::ZeroVector;

    FVector OldVelWorld = FVector::ZeroVector;
    if (VehicleMesh && VehicleMesh->IsSimulatingPhysics())
    {
        OldVelWorld = VehicleMesh->GetPhysicsLinearVelocity();
    }
    else if (MoveComp)
    {
        OldVelWorld = MoveComp->Velocity;
    }

    if (!OldVelWorld.IsNearlyZero())
    {
        const float SpeedAlongForward =
            FVector::DotProduct(OldVelWorld, WorldForward);
        NewVelocity = NewWorldForward * SpeedAlongForward;
    }

    // -------- 6. 真正 Teleport --------
    if (VehicleMesh && VehicleMesh->IsSimulatingPhysics())
    {
        FHitResult Hit;
        VehicleMesh->SetWorldLocationAndRotation(
            NewWorldLocation,
            NewWorldRotation,
            false,
            &Hit,
            ETeleportType::TeleportPhysics
        );

        VehicleMesh->SetPhysicsLinearVelocity(NewVelocity, false);
    }
    else
    {
        Actor->TeleportTo(NewWorldLocation, NewWorldRotation, false, true);
    }

    if (MoveComp)
    {
        MoveComp->Velocity = NewVelocity;
    }

    // -------- 7. 通知 AutoDriving：Teleport 目标 lane，加锁防干扰 --------
    if (UTraffic_AutoDriving* AutoDriving = Actor->FindComponentByClass<UTraffic_AutoDriving>())
    {
        AutoDriving->NotifyTeleportedToLane(TransferDestination);
        AutoDriving->SetCurrentLane(TransferDestination);
    }

    GetWorld()->GetTimerManager().SetTimer(
        LoopTriggerTimerHandle,
        [this]()
        {
            LoopTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        },
        1.0f,
        false
    );
}
