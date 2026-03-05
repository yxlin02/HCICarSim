// RecommendationManager.cpp

#include "RecommendationManager.h"
#include "RecommendationTypes.h"
#include "RecommendationWidget.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "AgentGameInstance.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/DataTable.h"
#include "VibrationSender.h"   // ← 新增

// ──────────────────────── 内部工具函数 ───────────────────────────────────────
/** 在当前 World 中找到第一个 AVibrationSender 实例（如果有的话）*/
static AVibrationSender* FindVibrationSender(UWorld* World)
{
    if (!World) return nullptr;
    TArray<AActor*> Found;
    UGameplayStatics::GetAllActorsOfClass(World, AVibrationSender::StaticClass(), Found);
    return (Found.Num() > 0) ? Cast<AVibrationSender>(Found[0]) : nullptr;
}

// ── 其余函数保持不变，仅展示修改部分 ────────────────────────────────────────

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
                    UE_LOG(LogTemp, Error, TEXT("[RecommendationManager] RecommendationTable not assigned."));
                    return;
                }
                else UE_LOG(LogTemp, Display, TEXT("[RecommendationManager] RecommendationTable read from GI."));
            }

            if (!RecommendationWidgetClass)
            {
                RecommendationWidgetClass = AgentGI->RecommendationWidgetClass;
                UE_LOG(LogTemp, Display, TEXT("[Game Instance] UI read."));
            }
            
            if (!PhaseBlockTable)
            {
                PhaseBlockTable = AgentGI->PhaseBlockTable;
                UE_LOG(LogTemp, Display, TEXT("[RecommendationManager] PhaseBlockTable read from GI."));
                if (!PhaseBlockTable) UE_LOG(LogTemp, Display, TEXT("[RecommendationManager] PhaseBlockTable not assigned."));
            }
            
            if (!RecommendationPatternTable)
            {
                RecommendationPatternTable = AgentGI->RecommendationPatternTable;
                UE_LOG(LogTemp, Display, TEXT("[RecommendationManager] RecommendationPatternTable read from GI."));
                if (!RecommendationPatternTable) UE_LOG(LogTemp, Display, TEXT("[RecommendationManager] RecommendationPatternTable not assigned."));
            }
            
            if (SubNum < 0 || Block < 0)
            {
                SubNum = AgentGI->SubNum;
                Block = AgentGI->Block;
                UE_LOG(LogTemp, Display, TEXT("[RecommendationManager] Subnum and block read from GI."));
                if (!RecommendationPatternTable) UE_LOG(LogTemp, Display, TEXT("[RecommendationManager] Subnum and block not assigned."));
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
    
    if (InitializeFromDesignTable())
    {
        UE_LOG(LogTemp, Display, TEXT("[RecommendationManager] Made initialize designated table."))
        // UpdateDesignatedRowToCurrentRow();
    }
    else {
        // UpdateRandomRowToCurrentRow();
        UE_LOG(LogTemp, Error, TEXT("[RecommendationManager] Failed to initialize designated table."))
    }
}

bool URecommendationManager::InitializeFromDesignTable()
{
    ActivePatternID = NAME_None;
    ActivePatternRows.Empty();
    CurrentDesignatedIndex = -1;

    if (!PhaseBlockTable)
    {
        UE_LOG(LogTemp, Error, TEXT("[RecommendationManager] PhaseBlockTable is not set."));
        return false;
    }

    int32 ModeAsInt = static_cast<int32>(Mode);

    FExperimentBlockDesignRow* MatchedRow = nullptr;

    const TMap<FName, uint8*>& RowMap = PhaseBlockTable->GetRowMap();
    for (const TPair<FName, uint8*>& Pair : RowMap)
    {
        FExperimentBlockDesignRow* Row = reinterpret_cast<FExperimentBlockDesignRow*>(Pair.Value);
        if (!Row) continue;

        if (Row->Sub == SubNum &&
            Row->Block == Block)
        {
            MatchedRow = Row;
            break;
        }
    }

    if (!MatchedRow)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[RecommendationManager] No row found in PhaseBlockTable for Sub=%d Mode=%d SceneID=%d"),
            SubNum, ModeAsInt, SceneID);
        return false;
    }
    
    ModeAsInt = MatchedRow->Mode;
    SceneID = MatchedRow->SceneID;

    ActivePatternID = MatchedRow->PatternID;
    UE_LOG(LogTemp, Display,
        TEXT("[RecommendationManager] Found design row. Sub=%d Mode=%d SceneID=%d. PatternID = %s"),
        SubNum, ModeAsInt, SceneID, *ActivePatternID.ToString());

    return BuildPatternRows();
}

bool URecommendationManager::BuildPatternRows()
{
    ActivePatternRows.Empty();
    CurrentDesignatedIndex = -1;

    if (!RecommendationPatternTable)
    {
        UE_LOG(LogTemp, Error, TEXT("[RecommendationManager] RecommendationPatternTable is not set."));
        return false;
    }

    if (RecommendationPatternTable->GetRowStruct() != FRecommendationPatternRow::StaticStruct())
    {
        UE_LOG(LogTemp, Error,
            TEXT("[RecommendationManager] RecommendationPatternTable RowStruct mismatch! Expect %s, got %s"),
            *FRecommendationPatternRow::StaticStruct()->GetName(),
            *RecommendationPatternTable->GetRowStruct()->GetName());
        return false;
    }

    if (ActivePatternID.IsNone())
    {
        UE_LOG(LogTemp, Error, TEXT("[RecommendationManager] ActivePatternID is None, cannot build pattern rows."));
        return false;
    }

    const TMap<FName, uint8*>& RowMap = RecommendationPatternTable->GetRowMap();
    for (const TPair<FName, uint8*>& Pair : RowMap)
    {
        FRecommendationPatternRow* Row = reinterpret_cast<FRecommendationPatternRow*>(Pair.Value);
        if (!Row) continue;

        if (Row->PatternID == ActivePatternID)
        {
            ActivePatternRows.Add(*Row);
        }
    }

    if (ActivePatternRows.Num() == 0)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[RecommendationManager] No rows found in RecommendationPatternTable for PatternID=%s"),
            *ActivePatternID.ToString());
        return false;
    }

    ActivePatternRows.Sort([](const FRecommendationPatternRow& A, const FRecommendationPatternRow& B)
    {
        return A.OrderIndex < B.OrderIndex;
    });

    UE_LOG(LogTemp, Display,
        TEXT("[RecommendationManager] Loaded %d pattern rows for PatternID=%s"),
        ActivePatternRows.Num(), *ActivePatternID.ToString());
    
    return true;
}

// ====================================================================================================
// Random recommendation generating component
// ====================================================================================================
bool URecommendationManager::SetCurrentRow(int32 EncodedID)
{
    if (!RecommendationTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("RecommendationTable is null."));
        return false;
    }

    if (EncodedID <= 0)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("SetCurrentRow: invalid EncodedID=%d"),
            EncodedID);
        return false;
    }

    // 由 EncodedID 构造 RowName，比如 10101 -> "10101"
    const FName RowName(*FString::FromInt(EncodedID));

    // 先在 DataTable 里查一遍，确认这个行存在
    FRecommendationTypes* FoundRow =
        RecommendationTable->FindRow<FRecommendationTypes>(
            RowName,
            TEXT("SetCurrentRow")
        );

    if (!FoundRow)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("SetCurrentRow: Row %s not found in RecommendationTable."),
            *RowName.ToString());
        return false;
    }

    LastEncodedID    = CurrentEncodedID;
    CurrentEncodedID = EncodedID;
    CurrentRow       = FoundRow;

    UE_LOG(LogTemp, Display,
        TEXT("SetCurrentRow: Now EncodedID=%d (RowName=%s)"),
        CurrentEncodedID,
        *RowName.ToString());
    
    RecommendationIndex++;

    return true;
}

bool URecommendationManager::UpdateRandomRowToCurrentRow()
{
    int32 RandomCategory = 0;
    int32 RandomSubcategory = 0;
    FRecommendationTypes RandomRecommendation;
    
    // 1. 随机一个 (Category, SubCategory)
    if (!GetRandomCategoryAndSubCategory(RandomCategory, RandomSubcategory))
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to get random Category/SubCategory"));
        return false;
    }

    // 2. 在这个 Category/SubCategory 里随机一个 Item
    if (!GetRandomItem(RandomCategory, RandomSubcategory, RandomRecommendation))
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to get random Item for Category %d SubCategory %d"),
               RandomCategory, RandomSubcategory);
        return false;
    }

    // 3. 根据 Category/SubCategory/Item 计算出编码 ID
    int32 EncodedID = MakeRecommendationID(
        RandomRecommendation.Category,
        RandomRecommendation.SubCategory,
        RandomRecommendation.Item
    );
    
    // EncodedID = 10401;
    
    UE_LOG(LogTemp, Display, TEXT("[RecommendationManager] Random row updated to current row"));
    return SetCurrentRow(EncodedID);
}

bool URecommendationManager::UpdateDesignatedRowToCurrentRow()
{
    int32 DesignatedCategory = 0;
    int32 DesignatedSubcategory = 0;
    FRecommendationTypes DesignatedRecommendation;
    
    if (bHasPendingDelayedRecommendation)
    {
        bHasPendingDelayedRecommendation = false;

        UE_LOG(LogTemp, Display,
               TEXT("[RecommendationManager] Consuming delayed recommendation at index=%d"),
               CurrentDesignatedIndex);

        return true;
    }
    
    if (ActivePatternRows.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[RecommendationManager] No ActivePatternRows to update."));
        return false;
    }

    const int32 NextIndex = CurrentDesignatedIndex + 1;
    if (NextIndex >= ActivePatternRows.Num())
    {
        UE_LOG(LogTemp, Display,
            TEXT("[RecommendationManager] Reached end of pattern rows (CurrentIndex=%d)."), CurrentDesignatedIndex);
        return false;
    }

    CurrentDesignatedIndex = NextIndex;
    const FRecommendationPatternRow& Row = ActivePatternRows[CurrentDesignatedIndex];
    
    DesignatedCategory = Row.Category;
    DesignatedSubcategory = Row.Subcategory;
    
    if (!GetRandomItem(DesignatedCategory, DesignatedSubcategory, DesignatedRecommendation))
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to get random Item for Category %d SubCategory %d"),
               DesignatedCategory, DesignatedSubcategory);
        return false;
    }
    
    int32 EncodedID = MakeRecommendationID(
                                           DesignatedRecommendation.Category,
                                           DesignatedRecommendation.SubCategory,
                                           DesignatedRecommendation.Item
    );

    UE_LOG(LogTemp, Display,
        TEXT("[RecommendationManager] Switched to OrderIndex=%d, EncodeID=%d (Cat=%d Sub=%d Delay=%s)"),
        Row.OrderIndex, EncodedID, Row.Category, Row.Subcategory, Row.bDelay ? TEXT("true") : TEXT("false"));
    
    bDelayCurrentRecommendation = Row.bDelay;
    
    if (!SetCurrentRow(EncodedID))
    {
        UE_LOG(LogTemp, Warning, TEXT("[RecommendationManager] SetCurrentRow failed for EncodedID=%d"), EncodedID);
        return false;
    }

    if (Row.bDelay)
    {
        bHasPendingDelayedRecommendation = true;

        UE_LOG(LogTemp, Display,
               TEXT("[RecommendationManager] Delayed recommendation prepared (index=%d). Will display on next call."),
               CurrentDesignatedIndex);

        return true;
    }

    return true;
}


// random number generator
bool URecommendationManager::GetRandomItem(
    int32 InCategory,
    int32 InSubCategory,
    FRecommendationTypes& OutRecommendation)
{
    if (!RecommendationTable)
        return false;

    TArray<FRecommendationTypes*> MatchingRows;

    static const FString ContextString(TEXT("Recommendation Lookup"));

    TArray<FRecommendationTypes*> AllRows;
    RecommendationTable->GetAllRows(ContextString, AllRows);

    for (FRecommendationTypes* Row : AllRows)
    {
        if (Row &&
            Row->Category == InCategory &&
            Row->SubCategory == InSubCategory)
        {
            MatchingRows.Add(Row);
        }
    }

    if (MatchingRows.Num() == 0)
        return false;

    const int32 RandomIndex = FMath::RandRange(0, MatchingRows.Num() - 1);

    OutRecommendation = *MatchingRows[RandomIndex];

    return true;
}

bool URecommendationManager::GetRandomCategoryAndSubCategory(
    int32& OutCategory,
    int32& OutSubCategory)
{
    if (!RecommendationTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("RecommendationTable is null."));
        return false;
    }

    TArray<FRecommendationTypes*> AllRows;
    static const FString ContextString(TEXT("Recommendation Lookup"));

    RecommendationTable->GetAllRows(ContextString, AllRows);

    if (AllRows.Num() == 0)
        return false;

    TSet<FIntPoint> UniquePairs;

    for (FRecommendationTypes* Row : AllRows)
    {
        if (!Row) continue;

        UniquePairs.Add(FIntPoint(Row->Category, Row->SubCategory));
    }

    if (UniquePairs.Num() == 0)
        return false;

    TArray<FIntPoint> PairArray = UniquePairs.Array();

    int32 RandomIndex = FMath::RandRange(0, PairArray.Num() - 1);

    OutCategory    = PairArray[RandomIndex].X;
    OutSubCategory = PairArray[RandomIndex].Y;

    return true;
}

// utils
int32 URecommendationManager::MakeRecommendationID(int32 Category, int32 SubCategory, int32 Item) const
{
    constexpr int32 CATEGORY_MULTIPLIER    = 10000;
    constexpr int32 SUBCATEGORY_MULTIPLIER = 100;
    
    return Category * CATEGORY_MULTIPLIER
         + SubCategory * SUBCATEGORY_MULTIPLIER
         + Item;
}

void URecommendationManager::DecomposeRecommendationID(
    int32 EncodedID,
    int32& OutCategory,
    int32& OutSubCategory,
    int32& OutItem) const
{
    constexpr int32 CATEGORY_MULTIPLIER    = 10000;
    constexpr int32 SUBCATEGORY_MULTIPLIER = 100;

    OutCategory    = EncodedID / CATEGORY_MULTIPLIER;
    OutSubCategory = (EncodedID % CATEGORY_MULTIPLIER) / SUBCATEGORY_MULTIPLIER;
    OutItem        = EncodedID % SUBCATEGORY_MULTIPLIER;
}

// ====================================================================================================
// Action component
// ====================================================================================================

void URecommendationManager::DisplayRecommendation()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    
    FRecommendationTypes& Entry = *CurrentRow;

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

    // 3. Vibration — 触发 ESP 板振动
    if (Entry.Recommendation_Haptic)
    {
        if (AVibrationSender* Sender = FindVibrationSender(World))
        {
            Sender->SendStrongVibration();
            UE_LOG(LogTemp, Log, TEXT("[RecommendationManager] Haptics triggered via VibrationSender."));
        }
        else
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[RecommendationManager] Recommendation_Haptic=true but no AVibrationSender found in level."));
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
    
    FRecommendationTypes& Entry = *CurrentRow;

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
    
    FRecommendationTypes& Entry = *CurrentRow;

    // 1. Update UI: Image + Text
    if (RecommendationWidget && !Entry.Content_Image.IsNull())
    {
        if (UTexture2D* Image = Entry.Content_Image.LoadSynchronous())
        {
            RecommendationWidget->ShowContent(Image);
        }
    }

    // ===== 修改：使用 SpawnSound2D 并保存音频组件引用 =====
    // 2. Play Sound
    if (Entry.Content_Sound.IsValid() || !Entry.Content_Sound.ToSoftObjectPath().IsNull())
    {
        USoundBase* Sound = Entry.Content_Sound.LoadSynchronous();
        if (Sound)
        {
            // 使用 SpawnSound2D 获取音频组件引用，而不是直接 PlaySound2D
            // 这样可以在外部控制音频（暂停/停止等）
            LastContentAudioComponent = UGameplayStatics::SpawnSound2D(World, Sound);
            
            if (LastContentAudioComponent)
            {
                UE_LOG(LogTemp, Display, TEXT("[RecommendationManager] Content audio component created and playing"));
            }
        }
    }
    else
    {
        // 如果没有声音，清空引用
        LastContentAudioComponent = nullptr;
    }
    // ===== 修改结束 =====
}
