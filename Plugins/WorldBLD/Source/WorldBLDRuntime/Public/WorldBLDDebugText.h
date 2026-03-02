// Copyright WorldBLD LLC. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TextRenderActor.h"
#include "WorldBLDDebugText.generated.h"

/**
 * A specialized TextRenderActor for WorldBLD debug text rendering
 * Provides simple default parameters with Roboto font for easy debug text display
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup = (WorldBLD))
class WORLDBLDRUNTIME_API AWorldBLDDebugText : public ATextRenderActor
{
	GENERATED_BODY()
	
public:
	AWorldBLDDebugText();

	/**
	 * Spawn and configure a debug text actor in the world
	 * @param World - The world to spawn the text actor in
	 * @param Text - The text to display
	 * @param Color - The color of the text
	 * @param WorldLoc - The world location to place the text
	 * @param Scale - The scale multiplier for the text size
	 * @return The spawned text render actor
	 */
	UFUNCTION(BlueprintCallable, Category = "WorldBLD|Debug", meta = (WorldContext = "World"))
	static AWorldBLDDebugText* DrawDebugText(UWorld* World, FText Text, FColor Color, FVector WorldLoc, double Scale = 1.0);
};

