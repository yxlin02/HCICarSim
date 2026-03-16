// Fill out your copyright notice in the Description page of Project Settings.


#include "AgentGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h" // 新增引入
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
//#include "RecommendationTypes.h" // 如果编译报错找不到 FExperimentBlockDesignRow，请确保包含了它   

void UAgentGameInstance::OnStart()
{
    // 切记要调用父类的 OnStart
    Super::OnStart();

    // 禁用所有屏幕上的调试和警告信息（执行控制台命令彻底关闭）
    if (GetWorld())
    {
        UKismetSystemLibrary::ExecuteConsoleCommand(GetWorld(), TEXT("DisableAllScreenMessages"));
    }

    // 1. 播放基础背景音乐
    if (BackgroundAudioData)
    {
        BackgroundAudioComponent = UGameplayStatics::CreateSound2D(this, BackgroundAudioData, BackgroundAudioVolume, 1.f, 0.f, nullptr, true, false);
        
        if (BackgroundAudioComponent)
        {
            BackgroundAudioComponent->Play();
            UE_LOG(LogTemp, Display, TEXT("[AgentGameInstance] Background audio started playing."));
        }
        else 
        {
            UE_LOG(LogTemp, Error, TEXT("[AgentGameInstance] Failed to create BackgroundAudioComponent!"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[AgentGameInstance] BackgroundAudioData is NOT set in the blueprint."));
    }

    // 2. 从 PhaseBlockTable 读取当前的 SceneID
    int32 CurrentSceneID = -1;
    if (PhaseBlockTable)
    {
        const TMap<FName, uint8*>& RowMap = PhaseBlockTable->GetRowMap();
        for (const TPair<FName, uint8*>& Pair : RowMap)
        {
            FExperimentBlockDesignRow* Row = reinterpret_cast<FExperimentBlockDesignRow*>(Pair.Value);
            if (!Row) continue;

            // 根据当前的被试编号(SubNum)和环节(Block)寻找匹配行
            if (Row->Sub == SubNum && Row->Block == Block)
            {
                CurrentSceneID = Row->SceneID;
                break;
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("[AgentGameInstance] PhaseBlockTable is NOT set in Editor."));
    }

    // 3. 判断如果当前 SceneID 为 3，则播放第二个专属背景音
    if (CurrentSceneID == 3)
    {
        if (Scene3BackgroundAudioData)
        {
            Scene3BackgroundAudioComponent = UGameplayStatics::CreateSound2D(this, Scene3BackgroundAudioData, Scene3BackgroundAudioVolume, 1.f, 0.f, nullptr, true, false);
            
            if (Scene3BackgroundAudioComponent)
            {
                Scene3BackgroundAudioComponent->Play();
                UE_LOG(LogTemp, Display, TEXT("[AgentGameInstance] Scene 3 extra background audio started playing."));
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[AgentGameInstance] Scene3BackgroundAudioData is NOT set in Editor."));
        }
    }
}