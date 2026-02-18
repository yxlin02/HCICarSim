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

    // 触发某一条推荐，外部只需要传 RowName（你可以用条件 -> RowName 映射）
    UFUNCTION(BlueprintCallable)
    void TriggerRecommendationByName(FName RowName);

    // 如果你想随机推荐，也可以加一个函数：随机选一行
    UFUNCTION(BlueprintCallable)
    void TriggerRandomRecommendation();

    // 数据表，从编辑器里指定
    UPROPERTY(EditAnywhere, Category="Recommendation")
    UDataTable* RecommendationTable;

    // UI Widget 类（UMG Widget）
    UPROPERTY(EditAnywhere, Category="Recommendation")
    TSubclassOf<UUserWidget> RecommendationWidgetClass;
    
    FRecommendationTypes* Row;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Recommendation")
    EDecisionTypes CurrentDecision = EDecisionTypes::Ignore;

    // 运行时创建的 Widget 实例
    UPROPERTY()
    URecommendationWidget* RecommendationWidget;
    
    UFUNCTION(BlueprintCallable)
    void DisplayReaction();

    UFUNCTION(BlueprintCallable)
    void DisplayRecommendation();

    UFUNCTION(BlueprintCallable)
    void DisplayContent();

    // ===== 新增：返回播放内容音频时的音频组件 =====
    UAudioComponent* GetLastContentAudioComponent() const { return LastContentAudioComponent; }
    // ===== 结束新增 =====

private:
    // ===== 新增：保存最后播放的音频组件 =====
    UPROPERTY()
    UAudioComponent* LastContentAudioComponent;
    // ===== 结束新增 =====
};
