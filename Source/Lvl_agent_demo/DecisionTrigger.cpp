// Fill out your copyright notice in the Description page of Project Settings.

#include "DecisionTrigger.h"
#include "DecisionManagerComponent.h"


// Sets default values
ADecisionTrigger::ADecisionTrigger()
{
     // Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;
    
    TriggerCollider = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerCollider"));
    SetRootComponent(TriggerCollider);

    TriggerCollider->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TriggerCollider->SetCollisionObjectType(ECC_WorldDynamic);
    TriggerCollider->SetCollisionResponseToAllChannels(ECR_Ignore);

    TriggerCollider->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    TriggerCollider->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Overlap);

    TriggerCollider->SetGenerateOverlapEvents(true);

    TriggerCollider->SetHiddenInGame(false);

}

// Called when the game starts or when spawned
void ADecisionTrigger::BeginPlay()
{
    Super::BeginPlay();
    TriggerCollider->OnComponentBeginOverlap.AddDynamic(
        this,
        &ADecisionTrigger::OnTriggerBeginOverlap
    );
    // UE_LOG(LogTemp, Display, TEXT("[Decision Trigger] Alive!"));
}

// Called every frame
void ADecisionTrigger::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

}

void ADecisionTrigger::OnTriggerBeginOverlap(
    UPrimitiveComponent* OverlappedComp,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32,
    bool,
    const FHitResult&
)
{
    // UE_LOG(LogTemp, Display, TEXT("[Decision Trigger] Collided!"));
    
    if (!OtherActor) return;

    APawn* Pawn = Cast<APawn>(OtherActor);
    if (!Pawn) return;

    if (AllowedPawnClass && !Pawn->IsA(AllowedPawnClass))
    {
        UE_LOG(LogTemp, Warning, TEXT("Not designated pawn collided with decision trigger!!!"));
        return;
    }
    
    UE_LOG(LogTemp, Display, TEXT("[Decision Trigger] Designated pawn collided with trigger box!"));

    if (UDecisionManagerComponent* Manager =
        Pawn->FindComponentByClass<UDecisionManagerComponent>())
    {
        Manager->HandleColliderHit();
    }
}

