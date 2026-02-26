// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "WorldBLDRuntimeSettings.generated.h"

///////////////////////////////////////////////////////////////////////////////////////////////////

// These settings are loaded directly from the Plugin's INI file.
UCLASS(Config=WorldBLD)
class WORLDBLDRUNTIME_API UWorldBLDRuntimeSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:

    ///////////////////////////////////////////////////////////////////////////////////////////////

    UFUNCTION(BlueprintPure, Category="WorldBLD", Meta = (DisplayName = "WorldBLD Settings"))
    static UWorldBLDRuntimeSettings* Get();

    ///////////////////////////////////////////////////////////////////////////////////////////////

    // Shared global line trace distance for tool traces
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="WorldBLD")
    float GlobalLineTraceDistance {1000000.0f};

    // Classes of actors that are ignored for line traces. See UCityKitElementSubsystem::GetAllToolIgnorableActors
    UPROPERTY(Config, VisibleAnywhere, BlueprintReadOnly, Category="WorldBLD", Meta=(Untracked))
    TArray<TSoftClassPtr<AActor>> TraceIgnoredActorClasses;

    // Whether or not to print debug messages to screen.
    UPROPERTY(Config, VisibleAnywhere, BlueprintReadWrite, Category="WorldBLD|Dev")
    bool bDebugMessages {false};

    UFUNCTION(BlueprintPure, Category="WorldBLD|Dev", Meta=(DisplayName="Show Debug Messages (WorldBLD)"))
    static bool ShowDebugMessages() { return GetDefault<UWorldBLDRuntimeSettings>()->bDebugMessages; }

    // Texture used for UWorldBLDElementSnapPointComponent
    UPROPERTY(Config, VisibleAnywhere, BlueprintReadWrite, Category="WorldBLD|Dev")
    TSoftObjectPtr<class UTexture2D> SnapPointTexture;
};

///////////////////////////////////////////////////////////////////////////////////////////////////
