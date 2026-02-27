// RecommendationManager.h
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/DataTable.h"
#include "RecommendationTypes.h"
#include "RecommendationWidget.h"
#include "RecommendationManager.generated.h"

UENUM(BlueprintType)
enum class EDecisionTypes : uint8
{
    Accept      UMETA(DisplayName = "Accept"),
    Decline  UMETA(DisplayName = "Decline"),
    Ignore  UMETA(DisplayName = "Ignore")
};

UCLASS()
class LVL_AGENT_DEMO_API URecommendationManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // 初始化
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // 数据表，从编辑器里指定
    UPROPERTY(EditAnywhere, Category="Recommendation")
    UDataTable* RecommendationTable;
    
    /** PhaseI_Block.csv 对应的 DataTable */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Data")
    UDataTable* PhaseBlockTable;

    /** RecommendationPatterns.csv 对应的 DataTable */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Data")
    UDataTable* RecommendationPatternTable;
    
    /** 当前被试编号 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Design")
    int32 SubNum = -1;

    /** 当前模式（1/2） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Design")
    ERecommendMode Mode = ERecommendMode::Mode1;

    /** 当前 SceneID（1/2/3） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Design")
    int32 Block = -1;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Design")
    int32 SceneID = -1;
    
    UFUNCTION(BlueprintCallable)
    int32 GetCurrentSceneID() const { return SceneID; }
    

    // UI Widget 类（UMG Widget）
    UPROPERTY(EditAnywhere, Category="Recommendation")
    TSubclassOf<UUserWidget> RecommendationWidgetClass;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recommendation")
    EDecisionTypes CurrentDecision = EDecisionTypes::Ignore;

    // 运行时创建的 Widget 实例
    UPROPERTY()
    URecommendationWidget* RecommendationWidget;
    
    UFUNCTION(BlueprintCallable)
    int32 GetCurrentRowId() const { return CurrentEncodedID; }
    
    UFUNCTION(BlueprintCallable)
    int32 GetLastRowId() const { return LastEncodedID; }
    
    UFUNCTION(BlueprintCallable)
    void DisplayReaction();

    UFUNCTION(BlueprintCallable)
    void DisplayRecommendation();

    UFUNCTION(BlueprintCallable)
    void DisplayContent();
    
    UFUNCTION(BlueprintCallable)
    bool GetRandomCategoryAndSubCategory(int32& OutCategory, int32& OutSubCategory);
    
    UFUNCTION(BlueprintCallable)
    bool GetRandomItem(int32 InCategory, int32 InSubCategory, FRecommendationTypes& OutRecommendation);
    
    UFUNCTION(BlueprintCallable)
    bool UpdateRandomRowToCurrentRow();
    
    UFUNCTION(BlueprintCallable)
    bool UpdateDesignatedRowToCurrentRow();
    
    UFUNCTION(BlueprintCallable)
    void DecomposeRecommendationID(
        int32 EncodedID,
        int32& OutCategory,
        int32& OutSubCategory,
        int32& OutItem) const;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Recommendation")
    FName ActivePatternID;
    
    UPROPERTY()
    bool bHasPendingDelayedRecommendation = false;

    // ===== 从同事代码合并：音频控制功能 =====
    /** 获取最后播放的内容音频组件，可用于暂停/停止音频 */
    UFUNCTION(BlueprintCallable, Category="Recommendation")
    UAudioComponent* GetLastContentAudioComponent() const { return LastContentAudioComponent; }
    // ===== 合并结束 =====
    
private:
    /** 当前 Pattern 下的所有行（已按 OrderIndex 排好） */
    UPROPERTY()
    TArray<FRecommendationPatternRow> ActivePatternRows;
    
    /** 当前走到哪个 index（-1 表示还没开始） */
    int32 CurrentDesignatedIndex = -1;
    
    bool InitializeFromDesignTable();
    bool BuildPatternRows();
    
    int32 MakeRecommendationID(int32 Category, int32 SubCategory, int32 Item) const;
    
    bool SetCurrentRow(int32 EncodedID);

    UPROPERTY()
    int32 CurrentEncodedID = 0;

    UPROPERTY()
    int32 LastEncodedID = 0;
    
    FRecommendationTypes* CurrentRow;
    
    int32 RecommendationIndex = 0;
    
    bool bDelayCurrentRecommendation = false;

    // ===== 从同事代码合并：保存音频组件引用 =====
    /** 保存最后播放的内容音频组件 */
    UPROPERTY()
    UAudioComponent* LastContentAudioComponent;
    // ===== 合并结束 =====
};
