// Fill out your copyright notice in the Description page of Project Settings.


#include "JumpPickup.h"
#include "MPShooter/Character/ShooterCharacter.h"
#include "MPShooter/ShooterComponents/BuffComponent.h"

void AJumpPickup::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) {
	Super::OnSphereOverlap(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);

	AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(OtherActor);
	if (ShooterCharacter) {
		UBuffComponent* Buff = ShooterCharacter->GetBuff();
		if (Buff) {
			// Buff player's speed
			Buff->BuffJump(JumpZVelocityBuff, JumpBuffDuration);
		}
	}
	Destroy();
}