#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RecommendationTypes.h"
#include "Traffic_AICarManagerComponent.generated.h"

class AEnv_RoadLaneTransfer;
class ATraffic_AICar;
class AEnv_RoadLane;
class URecommendationManager;

USTRUCT(BlueprintType)
struct LVL_AGENT_DEMO_API FLaneIndexSpawnConfig
{
    GENERATED_BODY()

    UPROPERTY()
    AEnv_RoadLane* Lane = nullptr;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 LaneIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ELaneCarDensity Density = ELaneCarDensity::Mid;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ELaneCarVelocity Velocity = ELaneCarVelocity::Mid;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class LVL_AGENT_DEMO_API UTraffic_AICarManagerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UTraffic_AICarManagerComponent();

public:
    static UTraffic_AICarManagerComponent* GetTrafficManager(const UObject* WorldContext);

    UFUNCTION(BlueprintCallable, Category="Traffic")
    void RegisterCar(ATraffic_AICar* Car);

    UFUNCTION(BlueprintCallable, Category="Traffic")
    void UnregisterCar(ATraffic_AICar* Car);

    UFUNCTION(BlueprintCallable, Category="Traffic")
    void GetAliveCars(TArray<ATraffic_AICar*>& OutCars);

    UFUNCTION(BlueprintCallable, Category="Traffic|Spawn")
    void SpawnCar();

    void Compact();

	UFUNCTION(BlueprintCallable, Category = "Traffic|Spawn")
	void SetCurrentLane(AEnv_RoadLane* Lane) { CurrentLane = Lane; }

    UFUNCTION(BlueprintCallable, Category = "Traffic|Spawn")
    AEnv_RoadLane* GetCurrentLane() { return CurrentLane; }

    UFUNCTION(BlueprintCallable, Category = "Traffic|Spawn")
    bool GetUsePawnApproachingEffect() { return bUsePawnApproachingEffect; }

protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(EditInstanceOnly, Category="Spawn|Targets")
    TArray<TObjectPtr<AEnv_RoadLane>> TargetLanes;

    UPROPERTY(EditAnywhere, Category="Spawn|Seeds")
    TArray<TSubclassOf<ATraffic_AICar>> CarSeedClasses;
    
    UFUNCTION()
    void OnAnyLoopTransferTriggered(AEnv_RoadLaneTransfer* Transfer, AActor* OtherActor, AEnv_RoadLane* DestLane);

private:
    AEnv_RoadLane* CurrentLane = nullptr;

    TSet<TWeakObjectPtr<ATraffic_AICar>> AliveCars;

    UFUNCTION()
    void HandleCarDestroyed(AActor* DestroyedActor);

    static TWeakObjectPtr<UTraffic_AICarManagerComponent> CachedMgr;

    bool TryGetLaneIndexConfig(const AEnv_RoadLane* Lane, int32 LaneIdx, FLaneIndexSpawnConfig& Out) const;
    FLaneIndexSpawnConfig MakeFallbackConfig(const AEnv_RoadLane* Lane, int32 LaneIdx, bool bUseFixedSeeds = false) const;

    TSet<uint64> SpawnedLaneIdxKeys;

    int32 DensityToCarsPerLane(ELaneCarDensity D) const;
    float VelocityToTargetSpeed(ELaneCarVelocity V) const;
    TSubclassOf<ATraffic_AICar> PickCarClass(int32 Seed) const;
    
    FTimerHandle SpawnCooldownHandle;

    UPROPERTY(EditAnywhere, Category="Spawn")
    float SpawnCooldown = 1.0f;

    bool bCanSpawn = true;
    
    void ResetSpawnCooldown();
    
    UPROPERTY(EditAnywhere, Category="Traffic mapping")
    UDataTable* TrafficMappingTable;
    
    URecommendationManager* RecMgr;
    
    int32 SceneID = -1;

    UPROPERTY(EditAnywhere, Category = "Traffic|Approach")
    bool bUsePawnApproachingEffect = true;

    UPROPERTY(EditAnywhere, Category="Traffic|Approach")
    float MaxAffectDist = 3000.f;

    UPROPERTY(EditAnywhere, Category = "Traffic|Approach")
    float MinApproachEffect = 0.3f;

    UPROPERTY(EditAnywhere, Category = "Traffic|Approach")
    float FalloffPower = 1.5f;

    UPROPERTY()
    TMap<uint64, FLaneIndexSpawnConfig> LaneIndexConfigMap;
};
