// Perf_MarkerRegistrar.cpp

#include "Perf_MarkerRegistrar.h"

// 你自己的头文件
#include "Perf_MetricsManager.h"
#include "RecommendationManager.h"
#include "Env_RoadLaneTransfer.h"
#include "Env_RoadLane.h"
#include "DecisionManagerComponent.h"

// 引擎相关
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"

UPerf_MarkerRegistrar::UPerf_MarkerRegistrar()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UPerf_MarkerRegistrar::BeginPlay()
{
    Super::BeginPlay();

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    if (UGameInstance* GI = World->GetGameInstance())
    {
        MetricsManager = GI->GetSubsystem<UPerf_MetricsManager>();
        RecommendationManager = GI->GetSubsystem<URecommendationManager>();
    }

    OpenCsv();
    BindAllTransfers();
    BindAllRecMgrs();
}

void UPerf_MarkerRegistrar::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    CloseCsv();
    Super::EndPlay(EndPlayReason);
}

void UPerf_MarkerRegistrar::OpenCsv()
{
    if (!MetricsManager || !MetricsManager->IsWritingMetrics())
    {
        UE_LOG(LogTemp, Display, TEXT("[Perf Marker] MetricsManager not ready or not writing."));
        return;
    }

    const FString SessionFolder = MetricsManager->GetCurrentSessionFolder();
    if (SessionFolder.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Perf Marker] SessionFolder is empty."));
        return;
    }

    const FString CsvPath = FPaths::Combine(SessionFolder, TEXT("Marker.csv"));

    IFileManager& FileManager = IFileManager::Get();
    CsvFile.Reset(FileManager.CreateFileWriter(*CsvPath, FILEWRITE_AllowRead | FILEWRITE_EvenIfReadOnly));
    if (!CsvFile.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("[Perf Marker] Failed to open Marker.csv at %s"), *CsvPath);
        return;
    }

    UE_LOG(LogTemp, Display, TEXT("[Perf Marker] Marker.csv opened at %s"), *CsvPath);
    WriteHeader();
}

void UPerf_MarkerRegistrar::CloseCsv()
{
    if (CsvFile.IsValid())
    {
        CsvFile->Flush();
        CsvFile.Reset();
    }
}

void UPerf_MarkerRegistrar::WriteHeader()
{
    if (!CsvFile.IsValid())
    {
        return;
    }

    const FString Header =
        TEXT("unixtimestamp_ms,gametime_ms,scene_id, block_id, trial_id,current_encode_id,marker\n");

    auto Ansi = StringCast<ANSICHAR>(*Header);
    CsvFile->Serialize((void*)Ansi.Get(), Ansi.Length());
    CsvFile->Flush();
}

void UPerf_MarkerRegistrar::BindAllTransfers()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    int32 BindCount = 0;

    for (TActorIterator<AEnv_RoadLaneTransfer> It(World); It; ++It)
    {
        AEnv_RoadLaneTransfer* Transfer = *It;
        if (!IsValid(Transfer))
        {
            continue;
        }

        Transfer->OnLoopTransferTriggered.AddDynamic(
            this,
            &UPerf_MarkerRegistrar::HandleLoopTransferTriggered
        );

        ++BindCount;
    }

    UE_LOG(LogTemp, Display, TEXT("[Perf Marker] Bound to %d AEnv_RoadLaneTransfer actors."), BindCount);
}

void UPerf_MarkerRegistrar::BindAllRecMgrs()
{
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        if (UDecisionManagerComponent* DecisionComp =
            It->FindComponentByClass<UDecisionManagerComponent>())
        {
            DecisionComp->OnDecisionTriggered.AddDynamic(
                this,
                &UPerf_MarkerRegistrar::HandleDecisionEvent
            );

            DecisionComp->OnReactionMake.AddDynamic(
                this,
                &UPerf_MarkerRegistrar::HandleReactionMake
            );
        }
    }
}

void UPerf_MarkerRegistrar::HandleDecisionEvent()
{
    // UE_LOG(LogTemp, Display, TEXT("Decision Event triggered"));
    WriteMarkerToCSV(TEXT("trigger"));
}

void UPerf_MarkerRegistrar::HandleReactionMake(int32 ReactionType)
{
    // UE_LOG(LogTemp, Display, TEXT("Reaction Make triggered: %d"), ReactionType);
    switch (ReactionType) {
    case 0:
        WriteMarkerToCSV(TEXT("reaction_ignore"));
        break;
    case 1:
        WriteMarkerToCSV(TEXT("reaction_accept"));
        break;
    case -1:
        WriteMarkerToCSV(TEXT("reaction_reject"));
        break;

    default:
        WriteMarkerToCSV(TEXT("reaction_unknown"));
        break;
    }

}

/** 事件回调：当车经过 LoopTransfer 时被调用 */
void UPerf_MarkerRegistrar::HandleLoopTransferTriggered(
    AEnv_RoadLaneTransfer* Transfer,
    AActor* OtherActor,
    AEnv_RoadLane* DestLane)
{
    if (!CsvFile.IsValid() || !GetWorld())
    {
        return;
    }

    WriteMarkerToCSV(TEXT("teleport"));
}

void UPerf_MarkerRegistrar::WriteMarkerToCSV(const FString& Marker)
{
    if (!CsvFile.IsValid() || !GetWorld())
    {
        return;
    }

    const FDateTime Now = FDateTime::UtcNow();
    const int64 UnixMs = Now.ToUnixTimestamp() * 1000 + Now.GetMillisecond();

    const float GameTimeSec = GetWorld()->GetTimeSeconds();
    const int64 GameTimeMs = (int64)(GameTimeSec * 1000.0);

    int32 TrialId = -1;
    int32 CurrentEncodeId = -1;
    int32 CurrentBlock = -1;
    int32 CurrentSceneID = -1;

    if (RecommendationManager)
    {
        TrialId = RecommendationManager->GetCurrentRecommendationIndex();
        CurrentEncodeId = RecommendationManager->GetCurrentRowId();
        CurrentBlock = RecommendationManager->GetCurrentBlockID();
        CurrentSceneID = RecommendationManager->GetCurrentSceneID();
    }

    FString CleanMarker = Marker;
    CleanMarker.ReplaceInline(TEXT(","), TEXT("_"));
    CleanMarker.ReplaceInline(TEXT("\n"), TEXT(" "));
    CleanMarker.ReplaceInline(TEXT("\r"), TEXT(" "));

    const TCHAR* MarkerStr = *CleanMarker;

    const FString Line = FString::Printf(
        TEXT("%lld,%lld,%d,%d,%d,%d,%s\n"),
        UnixMs,
        GameTimeMs,
        CurrentSceneID,
        CurrentBlock,
        TrialId,
        CurrentEncodeId,
        MarkerStr
    );

    auto Ansi = StringCast<ANSICHAR>(*Line);
    CsvFile->Serialize((void*)Ansi.Get(), Ansi.Length());
    CsvFile->Flush();

    UE_LOG(LogTemp, Verbose,
        TEXT("[Perf Marker] %s marker logged: unix=%lld, game=%lld, trial=%d, encode=%d"),
        *CleanMarker, UnixMs, GameTimeMs, TrialId, CurrentEncodeId);
}
