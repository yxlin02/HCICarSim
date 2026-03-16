// Perf_PawnCarRegistrar.h

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Perf_PawnCarRegistrar.generated.h"

class UPerf_MetricsManager;
class UChaosWheeledVehicleMovementComponent;
class USkeletalMeshComponent;
class APawn;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LVL_AGENT_DEMO_API UPerf_PawnCarRegistrar : public UActorComponent
{
    GENERATED_BODY()

public:
    UPerf_PawnCarRegistrar();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintCallable, Category = "Perf")
    void SetTargetPawnCar(APawn* InPawnCar);

protected:
    UPROPERTY(EditAnywhere, Category = "Perf")
    float TargetHz = 60.f;

private:
    void OpenCsv();
    void CloseCsv();
    void WriteHeader();
    void WriteOneLine();

    UPROPERTY()
    UPerf_MetricsManager* MetricsManager;

    TUniquePtr<FArchive> CsvFile;

    int32 FrameIndex = 0;
    float TimeSinceLastSample = 0.f;
    float SampleInterval = 1.f / 60.f;

    TWeakObjectPtr<APawn> TargetPawnCar;

    UPROPERTY()
    UChaosWheeledVehicleMovementComponent* CachedVehicleMovement = nullptr;

    UPROPERTY()
    USkeletalMeshComponent* CachedMesh = nullptr;
};
