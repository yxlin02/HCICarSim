// RecommendationManager.h
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/DataTable.h"
#include "RecommendationTypes.h"
#include "RecommendationWidget.h"
#include "RecommendationManager.generated.h"

// 前向声明，避免引发未定义类型错误
class UAudioComponent;

UENUM(BlueprintType)
enum class EDecisionTypes : uint8
{
    Accept      UMETA(DisplayName = "Accept"),
    Decline  UMETA(DisplayName = "Decline"),
    Ignore  UMETA(DisplayName = "Ignore")
};

// ★ 新增：给 Metrics 系统的事件广播
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRecommendationAudioStarted, float, AudioDuration);

UCLASS()
class LVL_AGENT_DEMO_API URecommendationManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // 初始化
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // 数据表，从编辑器里指定
    UPROPERTY(EditAnywhere, Category = "Recommendation")
    UDataTable* RecommendationTable;

    /** PhaseI_Block.csv 对应的 DataTable */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
    UDataTable* PhaseBlockTable;

    /** RecommendationPatterns.csv 对应的 DataTable */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
    UDataTable* RecommendationPatternTable;

    /** 当前被试编号 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Design")
    int32 SubNum = -1;

    /** 当前模式（1/2） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Design")
    ERecommendMode Mode = ERecommendMode::Mode1;

    /** 当前 SceneID（1/2/3） */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Design")
    int32 Block = -1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Design")
    int32 SceneID = -1;

    UFUNCTION(BlueprintCallable)
    int32 GetCurrentSceneID() const { return SceneID; }

    UFUNCTION()
    int32 GetCurrentModeID() const { return ModeAsInt; }

    UFUNCTION()
    int32 GetCurrentBlockID() const { return Block; }

    UFUNCTION()
    int32 GetCurrentRecommendationIndex() const { return RecommendationIndex; }

    /** 当前展示的是否是 pattern 中最后一个 recommendation */
    UFUNCTION(BlueprintCallable)
    bool IsPatternComplete() const
    {
        return ActivePatternRows.Num() > 0 &&
            CurrentDesignatedIndex >= ActivePatternRows.Num() - 1;
    }

    // UI Widget 类（UMG Widget）
    UPROPERTY(EditAnywhere, Category = "Recommendation")
    TSubclassOf<UUserWidget> RecommendationWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recommendation")
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

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Recommendation")
    FName ActivePatternID;

    UPROPERTY()
    bool bHasPendingDelayedRecommendation = false;

    // ★ 把以前的获取分离组件的方法统一返回共享组件，以兼顾外部其他代码接口
    UFUNCTION(BlueprintCallable, Category = "Recommendation")
    UAudioComponent* GetLastContentAudioComponent() const { return SharedAudioComponent; }

    UFUNCTION(BlueprintCallable, Category = "Recommendation")
    UAudioComponent* GetLastRecommendationAudioComponent() const { return SharedAudioComponent; }

    // ==========================================
    // ★ 供 AEnv_RoadLane 的 Metrics 系统访问的变量和事件
    // ==========================================
    UPROPERTY(BlueprintReadOnly, Category="Metrics")
    float CurrentRecommendationAudioDuration = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category="Metrics")
    float CurrentRecommendationStartTime = 0.0f;

    UPROPERTY(BlueprintAssignable, Category="Events")
    FOnRecommendationAudioStarted OnRecommendationAudioStarted;

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
    int32 ModeAsInt = 0;

    UPROPERTY()
    int32 LastEncodedID = 0;

    FRecommendationTypes* CurrentRow = nullptr;

    int32 RecommendationIndex = 0;

    bool bDelayCurrentRecommendation = false;

    // ★ 统一的音频播放通道（单例模式）
    UPROPERTY()
    UAudioComponent* SharedAudioComponent = nullptr;
};