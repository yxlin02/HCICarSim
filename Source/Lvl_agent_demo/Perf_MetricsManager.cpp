// Perf_MetricsManager.cpp

#include "Perf_MetricsManager.h"
#include "AgentGameInstance.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "HAL/FileManager.h"

void UPerf_MetricsManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    UGameInstance* GI = GetGameInstance();
    if (UAgentGameInstance* AgentGI = Cast<UAgentGameInstance>(GI))
    {
        if (!AgentGI->MetricsBaseOutputFolder.IsEmpty())
        {
            OutputFolderPath = AgentGI->MetricsBaseOutputFolder;
        }
        else
        {
            OutputFolderPath = FPaths::ProjectSavedDir() / TEXT("PerfMetrics");
        }

        BeginExperimentSession(AgentGI->SubNum, AgentGI->bWriteMetrics);
    }
}

void UPerf_MetricsManager::BeginExperimentSession(int32 SubNum, bool bWriteMetrics)
{
    bIsWritingMetrics = bWriteMetrics;

    if (!bWriteMetrics)
    {
        CurrentSessionFolder.Empty();
        return;
    }

    if (OutputFolderPath.IsEmpty())
    {
        FString BaseDir = FPlatformProcess::UserDir();
        OutputFolderPath = FPaths::Combine(BaseDir, TEXT("Documents"), TEXT("PerfMetrics"));
    }

    const FDateTime Now = FDateTime::Now();

    const FString TimeString = Now.ToString();

    const FString SubFolderName = FString::Printf(
        TEXT("sub_%d_%s"),
        SubNum,
        *TimeString
    );

    CurrentSessionFolder = FPaths::Combine(OutputFolderPath, SubFolderName);

    IFileManager::Get().MakeDirectory(*CurrentSessionFolder, /*Tree*/ true);

    UE_LOG(LogTemp, Display,
        TEXT("[Perf Metrics Mgr] OutputFolderPath %s"),
        *OutputFolderPath);
}
