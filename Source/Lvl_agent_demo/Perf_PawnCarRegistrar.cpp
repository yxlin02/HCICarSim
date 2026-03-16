// Perf_PawnCarRegistrar.cpp

#include "Perf_PawnCarRegistrar.h"
#include "Perf_MetricsManager.h"

#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

UPerf_PawnCarRegistrar::UPerf_PawnCarRegistrar()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UPerf_PawnCarRegistrar::SetTargetPawnCar(APawn* InPawnCar)
{
    TargetPawnCar = InPawnCar;

    CachedVehicleMovement = nullptr;
    CachedMesh = nullptr;

    if (APawn* Pawn = TargetPawnCar.Get())
    {
        CachedVehicleMovement = Pawn->FindComponentByClass<UChaosWheeledVehicleMovementComponent>();
        CachedMesh = Pawn->FindComponentByClass<USkeletalMeshComponent>();
    }
}

void UPerf_PawnCarRegistrar::BeginPlay()
{
    Super::BeginPlay();

    SampleInterval = 1.f / TargetHz;

    if (UGameInstance* GI = GetWorld()->GetGameInstance())
    {
        MetricsManager = GI->GetSubsystem<UPerf_MetricsManager>();
    }

    if (!MetricsManager->IsWritingMetrics()) return;

    if (!TargetPawnCar.IsValid())
    {
        APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
        SetTargetPawnCar(PlayerPawn);
    }

    OpenCsv();
}

void UPerf_PawnCarRegistrar::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    CloseCsv();
    Super::EndPlay(EndPlayReason);
}

void UPerf_PawnCarRegistrar::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!CsvFile.IsValid() || !TargetPawnCar.IsValid())
        return;

    TimeSinceLastSample += DeltaTime;
    if (TimeSinceLastSample >= SampleInterval)
    {
        TimeSinceLastSample -= SampleInterval;
        WriteOneLine();
    }
}

void UPerf_PawnCarRegistrar::OpenCsv()
{
    if (!MetricsManager || !MetricsManager->IsWritingMetrics())
        return;

    const FString SessionFolder = MetricsManager->GetCurrentSessionFolder();
    if (SessionFolder.IsEmpty())
        return;

    const FString CsvPath = FPaths::Combine(SessionFolder, TEXT("PawnCar.csv"));

    IFileManager& FileManager = IFileManager::Get();
    CsvFile.Reset(FileManager.CreateFileWriter(*CsvPath, FILEWRITE_AllowRead | FILEWRITE_EvenIfReadOnly));
    if (CsvFile.IsValid())
    {
        WriteHeader();
    }
}

void UPerf_PawnCarRegistrar::CloseCsv()
{
    if (CsvFile.IsValid())
    {
        CsvFile->Flush();
        CsvFile.Reset();
    }
}

void UPerf_PawnCarRegistrar::WriteHeader()
{
    const FString Header =
        TEXT("unixtimestamp_ms,n_frame,gametime_ms,")
        TEXT("position_x,position_y,position_z,")
        TEXT("rotation_roll,rotation_pitch,rotation_yaw,")
        TEXT("mesh_velocity_x,mesh_velocity_y,mesh_velocity_z,")
        TEXT("acc_x,acc_y,acc_z,")
        TEXT("forward_x,forward_y,forward_z,forward_speed,")
        TEXT("throttle_input,brake_input,handbrake_input\n");

    auto Ansi = StringCast<ANSICHAR>(*Header);
    CsvFile->Serialize((void*)Ansi.Get(), Ansi.Length());
    CsvFile->Flush();
}

void UPerf_PawnCarRegistrar::WriteOneLine()
{
    if (!CsvFile.IsValid() || !GetWorld() || !TargetPawnCar.IsValid())
        return;

    APawn* Pawn = TargetPawnCar.Get();

    const FDateTime Now = FDateTime::UtcNow();
    const int64 UnixMs = Now.ToUnixTimestamp() * 1000 + Now.GetMillisecond();

    const float GameTimeSec = GetWorld()->GetTimeSeconds();
    const int64 GameTimeMs = (int64)(GameTimeSec * 1000.0);

    const FVector Pos = Pawn->GetActorLocation();
    const FRotator Rot = Pawn->GetActorRotation();

    FVector Vel = FVector::ZeroVector;
    FVector Acc = FVector::ZeroVector;

    if (CachedMesh)
    {
        Vel = CachedMesh->GetPhysicsLinearVelocity();
        Acc = FVector::ZeroVector;
    }

    const FVector Forward = Pawn->GetActorForwardVector();
    const float ForwardSpeed = FVector::DotProduct(Vel, Forward);

    float ThrottleInput = 0.f;
    float BrakeInput = 0.f;
    float HandbrakeInput = 0.f;
	float SteeringInput = 0.f;

    if (CachedVehicleMovement)
    {
        ThrottleInput = CachedVehicleMovement->GetThrottleInput();
        BrakeInput = CachedVehicleMovement->GetBrakeInput();
        SteeringInput = CachedVehicleMovement->GetSteeringInput();

        const bool bHandbrake = CachedVehicleMovement->GetHandbrakeInput();
		
        HandbrakeInput = bHandbrake ? 1.f : 0.f;
    }

    const FString Line = FString::Printf(
        TEXT("%lld,%d,%lld,")
        TEXT("%.6f,%.6f,%.6f,")
        TEXT("%.6f,%.6f,%.6f,")
        TEXT("%.6f,%.6f,%.6f,")
        TEXT("%.6f,%.6f,%.6f,")
        TEXT("%.6f,%.6f,%.6f,%.6f,")
        TEXT("%.3f,%.3f,%.3f,%.3f\n"),
        UnixMs,
        FrameIndex++,
        GameTimeMs,
        Pos.X, Pos.Y, Pos.Z,
        Rot.Roll, Rot.Pitch, Rot.Yaw,
        Vel.X, Vel.Y, Vel.Z,
        Acc.X, Acc.Y, Acc.Z,
        Forward.X, Forward.Y, Forward.Z, ForwardSpeed,
        ThrottleInput, BrakeInput, SteeringInput, HandbrakeInput
    );

    auto Ansi = StringCast<ANSICHAR>(*Line);
    CsvFile->Serialize((void*)Ansi.Get(), Ansi.Length());
    CsvFile->Flush();
}
