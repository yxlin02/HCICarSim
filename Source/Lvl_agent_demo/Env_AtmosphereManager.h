// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Env_AtmosphereManager.generated.h"

class ADirectionalLight;
class URecommendationManager;

UCLASS()
class LVL_AGENT_DEMO_API AEnv_AtmosphereManager : public AActor
{
	GENERATED_BODY()

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category="Atmosphere")
	bool bUseAtmosphereControl = true;

	UPROPERTY(EditAnywhere, Category="Atmosphere")
	ADirectionalLight* SunLight = nullptr;

public:	
	URecommendationManager* RecMgr = nullptr;

	void ApplyAtmosphereForScene(int32 SceneID);

};
