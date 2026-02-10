#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "VibrationSender.generated.h"

class FSocket;
class FInternetAddr;

/**
 * Actor that sends UDP vibration commands to an external ESP board.
 * Payload format: "VIB:<token>,<intensity>,<duration>"
 */
UCLASS()
class LVL_AGENT_DEMO_API AVibrationSender : public AActor
{
	GENERATED_BODY()

public:
	AVibrationSender();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ===== 新增：可在编辑器中配置的默认值 =====
	
	/** Default IP address for ESP board (can be configured in editor) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vibration Settings")
	FString DefaultIPAddress = TEXT("192.168.1.100");
	
	/** Default UDP port */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vibration Settings")
	int32 DefaultPort = 5005;
	
	/** Default authentication token */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vibration Settings")
	FString DefaultToken = TEXT("abc123");
	
	/** Auto-initialize on BeginPlay? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vibration Settings")
	bool bAutoInitialize = true;
	
	// ===== 结束新增 =====

	/**
	 * Initialize the UDP sender with IP, port, and authentication token.
	 * @param InIPAddress - Target IP address (e.g., "192.168.1.100")
	 * @param InPort - Target UDP port (default 5005)
	 * @param InToken - Authentication token (default "abc123")
	 */
	UFUNCTION(BlueprintCallable, Category = "Vibration")
	void InitSender(const FString& InIPAddress, int32 InPort = 5005, const FString& InToken = TEXT("abc123"));

	/**
	 * Send a vibration command with custom intensity and duration.
	 * @param Intensity - Vibration intensity (0-255)
	 * @param DurationMs - Duration in milliseconds
	 */
	UFUNCTION(BlueprintCallable, Category = "Vibration")
	void SendVibration(int32 Intensity, int32 DurationMs);

	/**
	 * Send a strong vibration (255 intensity, 2000ms) twice with a 120ms delay.
	 */
	UFUNCTION(BlueprintCallable, Category = "Vibration")
	void SendStrongVibration();

protected:
	/** Create the UDP socket if it doesn't exist */
	void CreateSocketIfNeeded();

	/** Send a raw UDP message */
	bool SendUDPMessage(const FString& Message);

	/** Callback for the second vibration in SendStrongVibration */
	void SendSecondVibration();

private:
	/** UDP socket for sending messages */
	FSocket* UDPSocket;

	/** Remote address (IP + port) */
	TSharedPtr<FInternetAddr> RemoteAddr;

	/** Authentication token */
	FString Token;

	/** Timer handle for delayed second vibration */
	FTimerHandle SecondVibrationTimerHandle;

	/** Current IP address */
	FString IPAddress;

	/** Current port */
	int32 Port;
};