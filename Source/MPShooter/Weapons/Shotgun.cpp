// Fill out your copyright notice in the Description page of Project Settings.


#include "Shotgun.h"
#include "Engine/SkeletalMeshSocket.h"
#include "MPShooter/Character/ShooterCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundCue.h"

void AShotgun::Fire(const FVector& HitTarget) {	

	AWeapon::Fire(HitTarget);

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn == nullptr) return;
	AController* InstigatorController = OwnerPawn->GetController();

	// Perform Line Trace
	const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName("MuzzleFlash");
	if (MuzzleFlashSocket) {
		FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
		FVector Start = SocketTransform.GetLocation();
				
		TMap<AShooterCharacter*, uint32> HitMap;
		for (uint32 i = 0; i < NumberOfPellets; i++) {
			FHitResult FireHit;
			WeaponTraceHit(Start, HitTarget, FireHit);
			AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(FireHit.GetActor());
			if (ShooterCharacter && HasAuthority() && InstigatorController) {
				// If pellet hit an enemy, increment the counter for the times it's hit that enemy
				// If enemy hasn't been previously hit, add them to map and set hit count to 1
				if (HitMap.Contains(ShooterCharacter)) {
					HitMap[ShooterCharacter]++;
				}
				else {
					HitMap.Emplace(ShooterCharacter, 1);
				}
			}
			// Spawn impact particles
			if (ImpactParticles) {
				UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ImpactParticles, FireHit.ImpactPoint, FireHit.ImpactNormal.Rotation());
			}
			// Play hit sound
			if (HitSound) {
				UGameplayStatics::PlaySoundAtLocation(this, HitSound, FireHit.ImpactPoint, 0.5f, FMath::FRandRange(-.5f, .5f));
			}
		}

		// Apply damage (to each character that's hit if there r more than 1) based on amount of hits
		for (auto HitPair : HitMap) {
			if (HitPair.Key && HasAuthority() && InstigatorController) {
				// Hit character, deal damage
				UGameplayStatics::ApplyDamage(
					HitPair.Key,
					Damage * HitPair.Value,
					InstigatorController,
					this,
					UDamageType::StaticClass()
				);
			}
		}

	}
}
