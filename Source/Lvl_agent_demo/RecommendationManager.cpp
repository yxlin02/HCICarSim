// RecommendationManager.cpp

#include "RecommendationManager.h"
#include "RecommendationWidget.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "AgentGameInstance.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "VibrationSender.h"

void URecommendationManager::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // RecommendationTable = LoadObject<UDataTable>(...);
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UAgentGameInstance* AgentGI = Cast<UAgentGameInstance>(GI))
        {
            if (!RecommendationTable)
            {
                switch(AgentGI->CurrentExperimentType)
                {
                    case EExperimentType::Default:
                        RecommendationTable = AgentGI->DefaultRecommendationTable;
                        break;
                    case EExperimentType::Personalize:
                        RecommendationTable = AgentGI->PZRecommendationTable;
                        break;
                    case EExperimentType::Personalify:
                        RecommendationTable = AgentGI->PFRecommendationTable;
                        break;
                    default:
                        RecommendationTable = AgentGI->DefaultRecommendationTable;
                        break;
                }
                
                if (!RecommendationTable)
                {
                    UE_LOG(LogTemp, Error, TEXT("RecommendationTable not assigned!"));
                    return;
                }
                else UE_LOG(LogTemp, Display, TEXT("[Game Instance] Recommendation table read success."));
            }

            if (!RecommendationWidgetClass)
            {
                RecommendationWidgetClass = AgentGI->RecommendationWidgetClass;
                UE_LOG(LogTemp, Display, TEXT("[Game Instance] UI read success."));
            }
        }
    }
    
    
    if (RecommendationWidgetClass)
    {
        // UE_LOG(LogTemp, Display, TEXT("[Game Instance] UI 1."));
        UWorld* World = GetWorld();
        if (World)
        {
            RecommendationWidget = CreateWidget<URecommendationWidget>(World, RecommendationWidgetClass);
            // UE_LOG(LogTemp, Display, TEXT("[Game Instance] UI 2."));
        }
    }
}

void URecommendationManager::TriggerRecommendationByName(FName RowName)
{
    if (!RecommendationTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("RecommendationTable is null."));
        return;
    }

    Row = RecommendationTable->FindRow<FRecommendationTypes>(RowName, TEXT("TriggerRecommendationByName"));
    if (!Row)
    {
        UE_LOG(LogTemp, Warning, TEXT("Row %s not found in RecommendationTable."), *RowName.ToString());
        return;
    }

    DisplayRecommendation();
}

void URecommendationManager::TriggerRandomRecommendation()
{
    if (!RecommendationTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("RecommendationTable is null."));
        return;
    }

    TArray<FName> RowNames = RecommendationTable->GetRowNames();
    if (RowNames.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("RecommendationTable has no rows."));
        return;
    }

    int32 Index = FMath::RandRange(0, RowNames.Num() - 1);
    TriggerRecommendationByName(RowNames[Index]);
}

void URecommendationManager::DisplayRecommendation()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    
    FRecommendationTypes& Entry = *Row;

    // 1. Update UI: Image + Text
    if (RecommendationWidget && !Entry.Recommendation_Image.IsNull())
    {
        if (UTexture2D* Image = Entry.Recommendation_Image.LoadSynchronous())
        {
            RecommendationWidget->ShowReaction(Entry.Recommendation_Text, Image);
        }
    }

    // 2. Play Sound
    if (Entry.Recommendation_Sound.IsValid() || !Entry.Recommendation_Sound.ToSoftObjectPath().IsNull())
    {
        USoundBase* Sound = Entry.Recommendation_Sound.LoadSynchronous();
        if (Sound)
        {
            UGameplayStatics::PlaySound2D(World, Sound);
        }
    }

    // 3. Vibration）
    if (Entry.Recommendation_Haptic)
    {
        UE_LOG(LogTemp, Log, TEXT("Haptics triggered for recommendation."));

        // Send UDP vibration to external ESP board
        TArray<AActor*> FoundActors;
        UGameplayStatics::GetAllActorsOfClass(World, AVibrationSender::StaticClass(), FoundActors);
        if (FoundActors.Num() > 0)
        {
            if (AVibrationSender* VibSender = Cast<AVibrationSender>(FoundActors[0]))
            {
                VibSender->SendStrongVibration();
                UE_LOG(LogTemp, Log, TEXT("[RecommendationManager] UDP vibration sent to ESP board."));
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[RecommendationManager] No VibrationSender actor found in level. Add one to enable UDP haptics."));
        }
    }
}

void URecommendationManager::DisplayReaction()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    
    FRecommendationTypes& Entry = *Row;

    // 1. Update UI: Image + Text
    if (!RecommendationWidget) return;
    
    TSoftObjectPtr<UTexture2D> ReactionImage;
    TSoftObjectPtr<USoundBase> ReactionSound;
    bool bTriggerHaptic = false;

    // 2. switch
    switch (CurrentDecision)
    {
    case EDecisionTypes::Accept:
        ReactionImage  = Entry.Reaction_Accept_Image;
        ReactionSound  = Entry.Reaction_Accept_Sound;
        bTriggerHaptic = Entry.Reaction_Accept_Haptic;
        break;

    case EDecisionTypes::Decline:
    case EDecisionTypes::Ignore:
    default:
        ReactionImage  = Entry.Reaction_Decline_Image;
        ReactionSound  = Entry.Reaction_Decline_Sound;
        bTriggerHaptic = Entry.Reaction_Decline_Haptic;
        break;
    }

    // 3. Display image
    if (!ReactionImage.IsNull())
    {
        if (UTexture2D* Image = ReactionImage.LoadSynchronous())
        {
            RecommendationWidget->ShowReaction(FText(),Image);
        }
    }

    // 4. Play sound
    if (!ReactionSound.IsNull())
    {
        if (USoundBase* Sound = ReactionSound.LoadSynchronous())
        {
            UGameplayStatics::PlaySound2D(World, Sound);
        }
    }

    // 5. Vibration
    if (bTriggerHaptic)
    {
        UE_LOG(LogTemp, Log, TEXT("Haptics triggered for reaction."));
    }

}


void URecommendationManager::DisplayContent()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    
    FRecommendationTypes& Entry = *Row;

    // 1. Update UI: Image + Text
    if (RecommendationWidget && !Entry.Content_Image.IsNull())
    {
        if (UTexture2D* Image = Entry.Content_Image.LoadSynchronous())
        {
            RecommendationWidget->ShowContent(Image);
        }
    }

    // 2. Play Sound
    if (Entry.Content_Sound.IsValid() || !Entry.Content_Sound.ToSoftObjectPath().IsNull())
    {
        USoundBase* Sound = Entry.Content_Sound.LoadSynchronous();
        if (Sound)
        {
            UGameplayStatics::PlaySound2D(World, Sound);
        }
    }
}
