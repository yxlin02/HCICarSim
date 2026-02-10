// Copyright Epic Games, Inc. All Rights Reserved.

#include "VibrationSender.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Sockets.h"
#include "TimerManager.h"
#include "Engine/World.h"

AVibrationSender::AVibrationSender()
{
	PrimaryActorTick.bCanEverTick = false;
	UDPSocket = nullptr;
	IPAddress = TEXT("");
	Port = 5005;
	Token = TEXT("abc123");
}

void AVibrationSender::BeginPlay()
{
	Super::BeginPlay();
	
	// 如果启用自动初始化且有默认 IP，则自动初始化
	if (bAutoInitialize && !DefaultIPAddress.IsEmpty())
	{
		InitSender(DefaultIPAddress, DefaultPort, DefaultToken);
		UE_LOG(LogTemp, Log, TEXT("[VibrationSender] Auto-initialized with IP: %s, Port: %d"), *DefaultIPAddress, DefaultPort);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[VibrationSender] Auto-initialize disabled or IP is empty. Call InitSender manually."));
	}
}

void AVibrationSender::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// Clean up socket
	if (UDPSocket)
	{
		UDPSocket->Close();
		ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		if (SocketSubsystem)
		{
			SocketSubsystem->DestroySocket(UDPSocket);
		}
		UDPSocket = nullptr;
		UE_LOG(LogTemp, Log, TEXT("[VibrationSender] Socket destroyed."));
	}
}

void AVibrationSender::InitSender(const FString& InIPAddress, int32 InPort, const FString& InToken)
{
	IPAddress = InIPAddress;
	Port = InPort;
	Token = InToken;

	// Create remote address
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("[VibrationSender] SocketSubsystem not available."));
		return;
	}

	RemoteAddr = SocketSubsystem->CreateInternetAddr();
	bool bIsValid = false;
	RemoteAddr->SetIp(*IPAddress, bIsValid);
	RemoteAddr->SetPort(Port);

	if (!bIsValid)
	{
		UE_LOG(LogTemp, Error, TEXT("[VibrationSender] Invalid IP address: %s"), *IPAddress);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[VibrationSender] Initialized: IP=%s, Port=%d, Token=%s"), *IPAddress, Port, *Token);
	
	// Create the socket immediately
	CreateSocketIfNeeded();
}

void AVibrationSender::CreateSocketIfNeeded()
{
	if (UDPSocket)
	{
		return; // Socket already exists
	}

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("[VibrationSender] SocketSubsystem not available."));
		return;
	}

	// Create UDP socket using traditional method
	UDPSocket = SocketSubsystem->CreateSocket(NAME_DGram, TEXT("VibrationSender"), false);

	if (!UDPSocket)
	{
		UE_LOG(LogTemp, Error, TEXT("[VibrationSender] Failed to create UDP socket."));
		return;
	}

	// Set socket properties
	UDPSocket->SetNonBlocking(true);
	UDPSocket->SetReuseAddr(true);

	UE_LOG(LogTemp, Log, TEXT("[VibrationSender] UDP socket created successfully."));
}

bool AVibrationSender::SendUDPMessage(const FString& Message)
{
	CreateSocketIfNeeded();

	if (!UDPSocket)
	{
		UE_LOG(LogTemp, Error, TEXT("[VibrationSender] UDP socket is null."));
		return false;
	}

	if (!RemoteAddr.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("[VibrationSender] Remote address not initialized. Call InitSender first."));
		return false;
	}

	// Convert message to UTF-8
	FTCHARToUTF8 Converter(*Message);
	int32 BytesSent = 0;

	bool bSuccess = UDPSocket->SendTo((uint8*)Converter.Get(), Converter.Length(), BytesSent, *RemoteAddr);

	if (bSuccess && BytesSent > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[VibrationSender] Sent: %s (%d bytes)"), *Message, BytesSent);
		return true;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[VibrationSender] Failed to send message: %s"), *Message);
		return false;
	}
}

void AVibrationSender::SendVibration(int32 Intensity, int32 DurationMs)
{
	// Clamp intensity to 0-255
	Intensity = FMath::Clamp(Intensity, 0, 255);

	// Format: VIB:<token>,<intensity>,<duration>
	FString Message = FString::Printf(TEXT("VIB:%s,%d,%d"), *Token, Intensity, DurationMs);
	SendUDPMessage(Message);
}

void AVibrationSender::SendStrongVibration()
{
	// Send first vibration immediately
	SendVibration(255, 2000);

	// Schedule second vibration after 120ms
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			SecondVibrationTimerHandle,
			this,
			&AVibrationSender::SendSecondVibration,
			0.12f,
			false
		);
	}
}

void AVibrationSender::SendSecondVibration()
{
	SendVibration(255, 2000);
}