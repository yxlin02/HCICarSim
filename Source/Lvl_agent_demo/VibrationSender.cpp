
#include "VibrationSender.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "SocketTypes.h"
#include "Networking.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogVibrationSender, Log, All);

AVibrationSender::AVibrationSender()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AVibrationSender::BeginPlay()
{
    Super::BeginPlay();
    // 用编辑器里配置的默认值自动初始化
    InitSender(IPAddress, Port, Token);
}

void AVibrationSender::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(StrongVibRepeatHandle);
    }

    if (UDPSocket)
    {
        ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
        if (SocketSubsystem)
        {
            SocketSubsystem->DestroySocket(UDPSocket);
        }
        UDPSocket = nullptr;
        UE_LOG(LogVibrationSender, Log, TEXT("[VibrationSender] Socket destroyed."));
    }
}

// ────────────────────────────────────────────────────────────────────────────
void AVibrationSender::InitSender(const FString& InIPAddress, int32 InPort,
    const FString& InToken)
{
    IPAddress = InIPAddress;
    Port = InPort;
    Token = InToken;

    // 销毁旧 Socket（重新初始化时）
    if (UDPSocket)
    {
        ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
        if (SocketSubsystem)
        {
            SocketSubsystem->DestroySocket(UDPSocket);
        }
        UDPSocket = nullptr;
    }

    if (CreateSocketIfNeeded())
    {
        UE_LOG(LogVibrationSender, Log,
            TEXT("[VibrationSender] Initialized — IP=%s  Port=%d  Token=%s"),
            *IPAddress, Port, *Token);
    }
    else
    {
        UE_LOG(LogVibrationSender, Error,
            TEXT("[VibrationSender] InitSender failed — IP=%s  Port=%d"),
            *IPAddress, Port);
    }
}

void AVibrationSender::SendVibration(int32 Intensity, int32 DurationMs)
{
    // 限制范围
    Intensity = FMath::Clamp(Intensity, 0, 255);
    DurationMs = FMath::Max(DurationMs, 0);

    const FString Payload = FString::Printf(
        TEXT("VIB:%s,%d,%d"), *Token, Intensity, DurationMs);

    if (SendRawString(Payload))
    {
        UE_LOG(LogVibrationSender, Log,
            TEXT("[VibrationSender] Sent: %s"), *Payload);
    }
    else
    {
        UE_LOG(LogVibrationSender, Warning,
            TEXT("[VibrationSender] Failed to send: %s"), *Payload);
    }
}

void AVibrationSender::SendStrongVibration()
{
    // 第一脉冲，立即发送
    SendVibration(255, 1000);

    // 第二脉冲，120ms 后重复，不阻塞游戏线程
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            StrongVibRepeatHandle,
            [this]()
            {
                SendVibration(255, 200);
            },
            0.12f,
            false
        );
    }
}

// ────────────────────────────────────────────────────────────────────────────
bool AVibrationSender::CreateSocketIfNeeded()
{
    if (UDPSocket)
    {
        return true; // 已存在，直接复用
    }

    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        UE_LOG(LogVibrationSender, Error,
            TEXT("[VibrationSender] ISocketSubsystem not available."));
        return false;
    }

    // 构造远端地址
    RemoteAddr = SocketSubsystem->CreateInternetAddr();
    if (!RemoteAddr.IsValid())
    {
        UE_LOG(LogVibrationSender, Error,
            TEXT("[VibrationSender] Failed to create InternetAddr."));
        return false;
    }

    bool bIsValid = false;
    RemoteAddr->SetIp(*IPAddress, bIsValid);
    if (!bIsValid)
    {
        UE_LOG(LogVibrationSender, Error,
            TEXT("[VibrationSender] Invalid IP address: %s"), *IPAddress);
        RemoteAddr.Reset();
        return false;
    }
    RemoteAddr->SetPort(Port);

    // 创建 UDP Socket
    UDPSocket = FUdpSocketBuilder(TEXT("VibrationSenderSocket"))
        .AsNonBlocking()
        .Build();

    if (!UDPSocket)
    {
        UE_LOG(LogVibrationSender, Error,
            TEXT("[VibrationSender] Failed to build UDP socket."));
        RemoteAddr.Reset();
        return false;
    }

    return true;
}

bool AVibrationSender::SendRawString(const FString& Payload)
{
    if (!CreateSocketIfNeeded())
    {
        return false;
    }

    if (!RemoteAddr.IsValid())
    {
        UE_LOG(LogVibrationSender, Error,
            TEXT("[VibrationSender] RemoteAddr is invalid."));
        return false;
    }

    // 转换为 UTF-8
    FTCHARToUTF8 Converter(*Payload);
    int32 BytesSent = 0;
    const bool bSuccess = UDPSocket->SendTo(
        reinterpret_cast<const uint8*>(Converter.Get()),
        Converter.Length(),
        BytesSent,
        *RemoteAddr
    );

    return bSuccess && (BytesSent > 0);
}