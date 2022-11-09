// Fill out your copyright notice in the Description page of Project Settings.


#include "HealthPickup.h"
#include "MPShooter/Character/ShooterCharacter.h"
#include "MPShooter/ShooterComponents/BuffComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"

AHealthPickup::AHealthPickup() {
	bReplicates = true;

	// Create Niagara Component
	PickupEffectComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("PickupEffectComponent"));
	PickupEffectComponent->SetupAttachment(RootComponent);
}

void AHealthPickup::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) {
	Super::OnSphereOverlap(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);

	AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(OtherActor);
	if (ShooterCharacter) {
		UBuffComponent* Buff = ShooterCharacter->GetBuff();
		if (Buff) {
			// Heal player
			Buff->Heal(HealAmount, HealingTime);
		}
	}
	Destroy();
}

void AHealthPickup::Destroyed() {	
	// Spawn Niagara System for destroyed effect
	if (PickupEffect) {
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, PickupEffect, GetActorLocation(), GetActorRotation());
	}
	Super::Destroyed();
}