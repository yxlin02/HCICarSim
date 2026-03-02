// Copyright WorldBLD LLC. All rights reserved.

#include "WorldBLDDebugText.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Font.h"
#include "UObject/ConstructorHelpers.h"

AWorldBLDDebugText::AWorldBLDDebugText()
{
	// Get the text render component from the parent class
	if (UTextRenderComponent* TextComponent = GetTextRender())
	{
		// Set simple default parameters
		TextComponent->SetHorizontalAlignment(EHTA_Center);
		TextComponent->SetVerticalAlignment(EVRTA_TextCenter);
		TextComponent->SetWorldSize(50.0f);
		TextComponent->SetTextRenderColor(FColor::White);
		
		// Use the default Roboto font
		static ConstructorHelpers::FObjectFinder<UFont> RobotoFontObj(TEXT("/Engine/EngineFonts/Roboto"));
		if (RobotoFontObj.Succeeded())
		{
			TextComponent->SetFont(RobotoFontObj.Object);
		}
		
		// Make text always face camera
		TextComponent->SetHiddenInGame(false);
	}
}

AWorldBLDDebugText* AWorldBLDDebugText::DrawDebugText(UWorld* World, FText Text, FColor Color, FVector WorldLoc, double Scale)
{
	if (!World)
	{
		return nullptr;
	}
	
	// Spawn the debug text actor
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	AWorldBLDDebugText* DebugTextActor = World->SpawnActor<AWorldBLDDebugText>(
		AWorldBLDDebugText::StaticClass(),
		WorldLoc,
		FRotator::ZeroRotator,
		SpawnParams
	);
	
	if (DebugTextActor)
	{
		if (UTextRenderComponent* TextComponent = DebugTextActor->GetTextRender())
		{
			// Set the text
			TextComponent->SetText(Text);
			
			// Set the color
			TextComponent->SetTextRenderColor(Color);
			
			// Set the scale
			FVector ScaleVec(Scale, Scale, Scale);
			DebugTextActor->SetActorScale3D(ScaleVec);
		}
	}
	
	return DebugTextActor;
}

