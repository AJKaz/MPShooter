// Fill out your copyright notice in the Description page of Project Settings.


#include "HitScanWeapon.h"
#include "Engine/SkeletalMeshSocket.h"
#include "MPShooter/Character/ShooterCharacter.h"
#include "Kismet/GameplayStatics.h"

void AHitScanWeapon::Fire(const FVector& HitTarget) {
	Super::Fire(HitTarget);

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn == nullptr) return;
	AController* InstigatorController = OwnerPawn->GetController();

	// Perform Line Trace
	const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName("MuzzleFlash");
	if (MuzzleFlashSocket && InstigatorController) {

		FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
		FVector Start = SocketTransform.GetLocation();
		FVector End = Start + (HitTarget - Start) * 1.25;

		FHitResult FireHit;
		UWorld* World = GetWorld();
		if (World) {
			World->LineTraceSingleByChannel(
				FireHit,
				Start,
				End,
				ECollisionChannel::ECC_Visibility
			);
			if (FireHit.bBlockingHit) {
				AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(FireHit.GetActor());
				if (ShooterCharacter && HasAuthority()) {
					// Hit character, deal damage
					UGameplayStatics::ApplyDamage(
						ShooterCharacter,
						Damage,
						InstigatorController,
						this,
						UDamageType::StaticClass()
					);
				}
				// Spawn impact particles
				if (ImpactParticles) {
					UGameplayStatics::SpawnEmitterAtLocation(
						World,
						ImpactParticles,
						FireHit.ImpactPoint,
						FireHit.ImpactNormal.Rotation()
					);
				}

			}
		}

	}
}
