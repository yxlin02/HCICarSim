// Perf_MarkerRegistrar.h

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Perf_MarkerRegistrar.generated.h"

class UPerf_MetricsManager;
class URecommendationManager;
class AEnv_RoadLaneTransfer;
class AEnv_RoadLane;
class AActor;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class LVL_AGENT_DEMO_API UPerf_MarkerRegistrar : public UActorComponent
{
    GENERATED_BODY()

public:
    UPerf_MarkerRegistrar();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void OpenCsv();
    void CloseCsv();
    void WriteHeader();

    UFUNCTION()
    void HandleLoopTransferTriggered(AEnv_RoadLaneTransfer* Transfer,
        AActor* OtherActor,
        AEnv_RoadLane* DestLane);

    void BindAllTransfers();

    UFUNCTION()
    void HandleDecisionEvent();

    UFUNCTION()
    void HandleReactionMake(int32 ReactionType);

    void BindAllRecMgrs();

    void WriteMarkerToCSV(const FString& Marker);

private:
    UPROPERTY()
    UPerf_MetricsManager* MetricsManager = nullptr;

    UPROPERTY()
    URecommendationManager* RecommendationManager = nullptr;

    TUniquePtr<FArchive> CsvFile;
};
