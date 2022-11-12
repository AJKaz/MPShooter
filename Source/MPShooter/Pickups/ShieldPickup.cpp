// Fill out your copyright notice in the Description page of Project Settings.


#include "ShieldPickup.h"
#include "MPShooter/Character/ShooterCharacter.h"
#include "MPShooter/ShooterComponents/BuffComponent.h"

void AShieldPickup::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) {
	Super::OnSphereOverlap(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);

	AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(OtherActor);
	if (ShooterCharacter) {
		UBuffComponent* Buff = ShooterCharacter->GetBuff();
		if (Buff) {
			// Heal player
			Buff->RegenShield(ShieldRegenAmount, ShieldRegenTime);
		}
	}
	Destroy();
}