#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sockets.h"
#include "VibrationSender.generated.h"

/**
 * AVibrationSender
 * 通过 UDP 向外部 ESP 板发送振动指令。
 * 载荷格式: VIB:<token>,<intensity>,<duration>
 */
UCLASS(Blueprintable, BlueprintType)
class LVL_AGENT_DEMO_API AVibrationSender : public AActor
{
    GENERATED_BODY()

public:
    AVibrationSender();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    /**
     * 初始化 UDP 目标地址、端口和令牌。
     * 建议在关卡蓝图或 GameMode 的 BeginPlay 中调用。
     */
    UFUNCTION(BlueprintCallable, Category = "VibrationSender")
    void InitSender(const FString& InIPAddress, int32 InPort = 5005,
        const FString& InToken = TEXT("abc123"));

    /**
     * 发送一条振动指令。
     * @param Intensity  振动强度 0-255
     * @param DurationMs 持续时间（毫秒）
     */
    UFUNCTION(BlueprintCallable, Category = "VibrationSender")
    void SendVibration(int32 Intensity, int32 DurationMs);

    /**
     * 发送双脉冲强振动（255 强度，200ms，间隔 120ms 再重复一次）。
     */
    UFUNCTION(BlueprintCallable, Category = "VibrationSender")
    void SendStrongVibration();

    // ── 编辑器可配置默认值 ──────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibrationSender|Config")
    FString IPAddress = TEXT("192.168.1.100");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibrationSender|Config")
    int32 Port = 5005;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VibrationSender|Config")
    FString Token = TEXT("abc123");

private:
    /** 确保 UDP Socket 已创建 */
    bool CreateSocketIfNeeded();

    /** 向当前 RemoteAddr 发送原始字符串 */
    bool SendRawString(const FString& Payload);

    FSocket* UDPSocket = nullptr;
    TSharedPtr<FInternetAddr> RemoteAddr;

    FTimerHandle StrongVibRepeatHandle;
};