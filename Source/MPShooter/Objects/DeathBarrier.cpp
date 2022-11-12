// Fill out your copyright notice in the Description page of Project Settings.


#include "DeathBarrier.h"
#include "Components/BoxComponent.h"
#include "MPShooter/GameMode/ShooterGameMode.h"
#include "MPShooter/PlayerController/ShooterPlayerController.h"
#include "MPShooter/GameMode/ShooterGameMode.h"
#include "MPShooter/Character/ShooterCharacter.h"

ADeathBarrier::ADeathBarrier() {
	PrimaryActorTick.bCanEverTick = false;

	// Setup component and add overlap binding
	DeathBarrier = CreateDefaultSubobject<UBoxComponent>(TEXT("DeathBarrier"));
	RootComponent = DeathBarrier;
}


void ADeathBarrier::BeginPlay() {
	Super::BeginPlay();
	DeathBarrier->OnComponentBeginOverlap.AddDynamic(this, &ADeathBarrier::OverlapBegin);
}

void ADeathBarrier::OverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) {
	// If player overlaps this box, kill them
	AShooterGameMode* ShooterGameMode = GetWorld()->GetAuthGameMode<AShooterGameMode>();
	AShooterCharacter* Character = Cast<AShooterCharacter>(OtherActor);
	if (ShooterGameMode && Character && Character->GetController()) {
		AShooterPlayerController* Controller = Cast<AShooterPlayerController>(Character->GetController());				
		if (Controller) {
			ShooterGameMode->PlayerEliminated(Character, Controller);
		}
	}
}


