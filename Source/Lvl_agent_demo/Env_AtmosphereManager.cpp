// Fill out your copyright notice in the Description page of Project Settings.


#include "Env_AtmosphereManager.h"
#include "RecommendationManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/DirectionalLight.h"

// Called when the game starts or when spawned
void AEnv_AtmosphereManager::BeginPlay()
{
	Super::BeginPlay();

	if (!bUseAtmosphereControl) return;

	UWorld* World = GetWorld();
	if (!World) return;

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance) return;

	RecMgr = GameInstance->GetSubsystem<URecommendationManager>();
	if (!RecMgr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AtmosphereManager] RecommendationManager subsystem not found."));
		return;
	}

	const int32 CurrentSceneID = RecMgr->GetCurrentSceneID();
	UE_LOG(LogTemp, Warning, TEXT("[AtmosphereManager] Current SceneID: %d"), CurrentSceneID);
	ApplyAtmosphereForScene(CurrentSceneID);
	
}

void AEnv_AtmosphereManager::ApplyAtmosphereForScene(int32 SceneID)
{
	if (!SunLight)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AtmosphereManager] SunLight reference not set."));
		return;
	}

	FRotator Rot = SunLight->GetActorRotation();
	switch (SceneID)
	{
	case 1:
		Rot.Roll = 120.f;
		Rot.Pitch = -75.f;
		Rot.Yaw = 60.f;
		break;
	case 2:
		Rot.Roll = 120.f;
		Rot.Pitch = -0.f;
		Rot.Yaw = 215.f;
		break;
	case 3:
		Rot.Roll = 120.f;
		Rot.Pitch = -30.f;
		Rot.Yaw = 60.f;
		break;
	default:
		UE_LOG(LogTemp, Warning, TEXT("[AtmosphereManager] Unknown SceneID: %d"), SceneID);
		Rot.Pitch = -60.f;
		break;
	}

	SunLight->SetActorRotation(Rot);
}

