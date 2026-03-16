// Perf_MetricsManager.h

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Perf_MetricsManager.generated.h"

UCLASS()
class LVL_AGENT_DEMO_API UPerf_MetricsManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

protected:
    // Perf_MetricsManager.h
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

public:
    UFUNCTION(BlueprintCallable)
    void SetOutputFolderPath(const FString& InOutputFolderPath)
    {
        OutputFolderPath = InOutputFolderPath;
    }

    UFUNCTION(BlueprintCallable)
    void BeginExperimentSession(int32 SubNum, bool bWriteMetrics);

    UFUNCTION(BlueprintCallable, BlueprintPure)
    FString GetCurrentSessionFolder() const { return CurrentSessionFolder; }

    UFUNCTION(BlueprintCallable, BlueprintPure)
    bool IsWritingMetrics() const { return bIsWritingMetrics; }

private:
    UPROPERTY()
    FString OutputFolderPath;

    UPROPERTY()
    FString CurrentSessionFolder;

    UPROPERTY()
    bool bIsWritingMetrics = false;
};
