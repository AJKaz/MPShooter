// Fill out your copyright notice in the Description page of Project Settings.


#include "DeathBarrier.h"
#include "Components/BoxComponent.h"

ADeathBarrier::ADeathBarrier() {
	PrimaryActorTick.bCanEverTick = false;

}


void ADeathBarrier::BeginPlay() {
	Super::BeginPlay();

	DeathBarrier = CreateDefaultSubobject<UBoxComponent>(TEXT("DeathBarrier"));
}

void ADeathBarrier::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) {
	UE_LOG(LogTemp, Warning, TEXT("Activated"));
}


