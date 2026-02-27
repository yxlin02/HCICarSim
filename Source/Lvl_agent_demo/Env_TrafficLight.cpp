// Fill out your copyright notice in the Description page of Project Settings.


#include "Env_TrafficLight.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "Engine/World.h"

// Sets default values
AEnv_TrafficLight::AEnv_TrafficLight()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = false;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;

    PoleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PoleMesh"));
    PoleMesh->SetupAttachment(RootComponent);

    ForwardLightMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ForwardLightMesh"));
    ForwardLightMesh->SetupAttachment(RootComponent);

    LeftLightMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftLightMesh"));
    LeftLightMesh ->SetupAttachment(RootComponent);

    CurrentPhase = EGlobalTrafficPhase::NS_Straight_Ped_Green;

    ParamName_OnOff = TEXT("OnOff");

    ForwardRedMID = ForwardYellowMID = ForwardGreenMID = nullptr;
    LeftRedMID   = LeftYellowMID   = LeftGreenMID   = nullptr;

}

// Called when the game starts or when spawned
void AEnv_TrafficLight::BeginPlay()
{
	Super::BeginPlay();
    InitDynamicMaterials();

    ApplyPhase(CurrentPhase);
	
}

void AEnv_TrafficLight::InitDynamicMaterials()
{
    // 5=Red, 3=Yellow, 6=Green
    if (ForwardLightMesh)
    {
        ForwardRedMID =
            ForwardLightMesh->CreateAndSetMaterialInstanceDynamic(5);
        ForwardYellowMID =
            ForwardLightMesh->CreateAndSetMaterialInstanceDynamic(3);
        ForwardGreenMID =
            ForwardLightMesh->CreateAndSetMaterialInstanceDynamic(6);
    }

    if (LeftLightMesh)
    {
        LeftRedMID =
            LeftLightMesh->CreateAndSetMaterialInstanceDynamic(5);
        LeftYellowMID =
            LeftLightMesh->CreateAndSetMaterialInstanceDynamic(3);
        LeftGreenMID =
            LeftLightMesh->CreateAndSetMaterialInstanceDynamic(6);
    }

    TurnDirectionOff(ForwardRedMID, ForwardYellowMID, ForwardGreenMID);
    TurnDirectionOff(LeftRedMID,   LeftYellowMID,   LeftGreenMID);
}

void AEnv_TrafficLight::TurnDirectionOff(
    UMaterialInstanceDynamic* RedMID,
    UMaterialInstanceDynamic* YellowMID,
    UMaterialInstanceDynamic* GreenMID)
{
    SetOff(RedMID);
    SetOff(YellowMID);
    SetOff(GreenMID);
}

void AEnv_TrafficLight::ApplyPhase(EGlobalTrafficPhase NewPhase)
{
    CurrentPhase = NewPhase;

    // 先都关掉 / 置红
    TurnDirectionOff(ForwardRedMID, ForwardYellowMID, ForwardGreenMID);
    TurnDirectionOff(LeftRedMID,   LeftYellowMID,   LeftGreenMID);

    switch (NewPhase)
    {
        // ① 南北直行 + 行人绿，东西全部红
        case EGlobalTrafficPhase::NS_Straight_Ped_Green:
        {
            if (Axis == ETrafficAxis::NorthSouth)
            {
                if (TrafficRole == ETrafficLightRole::Vehicle)
                {
                    // 直行绿灯
                    SetOff(ForwardRedMID);
                    SetOn(ForwardGreenMID);
                    SetOff(LeftGreenMID);
                    SetOn(LeftRedMID);
                }
//                else if (TrafficRole == ETrafficLightRole::Pedestrian)
//                {
//                    // 行人绿灯
//                    if (PedRedMID)  SetOff(PedRedMID);
//                    if (PedGreenMID) SetOn(PedGreenMID);
//                }
                // 左转保持红灯，不动
            }
            else
            {
                SetOn(ForwardRedMID);
                SetOn(LeftRedMID);
            }
            // EastWest 轴：全部红灯，什么都不用做
            break;
        }

        // ② 南北左转绿，南北直行/行人红，东西全部红
        case EGlobalTrafficPhase::NS_Left_Green:
        {
            if (Axis == ETrafficAxis::NorthSouth)
            {
                if (TrafficRole == ETrafficLightRole::Vehicle)
                {
                    // 直行刚结束，可以走黄灯过渡到红（可选）
                    EnterForwardYellow();   // 你已经写好的逻辑
                    SetOff(LeftRedMID);
                    SetOn(LeftGreenMID);
                }
//                else if (TrafficRole == ETrafficLightRole::Pedestrian)
//                {
//                    // 行人变红
//                    if (PedGreenMID) SetOff(PedGreenMID);
//                    if (PedRedMID)   SetOn(PedRedMID);
//                }
            }
            else
            {
                SetOn(ForwardRedMID);
                SetOn(LeftRedMID);
            }
            break;
        }

        // ③ 东西直行 + 行人绿，南北全部红
        case EGlobalTrafficPhase::EW_Straight_Ped_Green:
        {
            if (Axis == ETrafficAxis::EastWest)
            {
                if (TrafficRole == ETrafficLightRole::Vehicle)
                {
                    SetOff(ForwardRedMID);
                    SetOn(ForwardGreenMID);
                    SetOff(LeftGreenMID);
                    SetOn(LeftRedMID);
                }
//                else if (TrafficRole == ETrafficLightRole::Pedestrian)
//                {
//                    if (PedRedMID)   SetOff(PedRedMID);
//                    if (PedGreenMID) SetOn(PedGreenMID);
//                }
            }
            else
            {
                SetOn(ForwardRedMID);
                SetOn(LeftRedMID);
            }
            break;
        }

        // ④ 东西左转绿，东西直行/行人红，南北全部红
        case EGlobalTrafficPhase::EW_Left_Green:
        {
            if (Axis == ETrafficAxis::EastWest)
            {
                if (TrafficRole == ETrafficLightRole::Vehicle)
                {
                    EnterForwardYellow();
                    SetOff(LeftRedMID);
                    SetOn(LeftGreenMID);
                }
//                else if (TrafficRole == ETrafficLightRole::Pedestrian)
//                {
//                    if (PedGreenMID) SetOff(PedGreenMID);
//                    if (PedRedMID)   SetOn(PedRedMID);
//                }
            }
            else
            {
                SetOn(ForwardRedMID);
                SetOn(LeftRedMID);
            }
            break;
        }

        default:
            break;
    }
}


// Foward Yellow - Red
void AEnv_TrafficLight::EnterForwardYellow()
{
    SetOff(ForwardGreenMID);

    SetOn(ForwardYellowMID);

    GetWorldTimerManager().SetTimer(
        YellowTimerHandle,
        this,
        &AEnv_TrafficLight::EnterForwardRed,
        YellowDuration,
        false
    );
}

void AEnv_TrafficLight::EnterForwardRed()
{
    SetOff(ForwardYellowMID);

    SetOn(ForwardRedMID);
}

// Left Yellow - Red
void AEnv_TrafficLight::EnterLeftYellow()
{
    SetOff(LeftGreenMID);

    SetOn(LeftYellowMID);

    GetWorldTimerManager().SetTimer(
        YellowTimerHandle,
        this,
        &AEnv_TrafficLight::EnterLeftRed,
        YellowDuration,
        false
    );
}

void AEnv_TrafficLight::EnterLeftRed()
{
    SetOff(LeftYellowMID);

    SetOn(LeftRedMID);
}

void AEnv_TrafficLight::SetOn(UMaterialInstanceDynamic* MID)
{
    if (MID)
    {
        MID->SetScalarParameterValue(ParamName_OnOff, 1.f);
    }
}

void AEnv_TrafficLight::SetOff(UMaterialInstanceDynamic* MID)
{
    if (MID)
    {
        MID->SetScalarParameterValue(ParamName_OnOff, 0.f);
    }
}
