// Fill out your copyright notice in the Description page of Project Settings.


#include "Shotgun.h"
#include "Engine/SkeletalMeshSocket.h"
#include "MPShooter/Character/ShooterCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundCue.h"
#include "MPShooter/PlayerController/ShooterPlayerController.h"
#include "MPShooter/ShooterComponents/LagCompensationComponent.h"
#include "Kismet/KismetMathLibrary.h"


void AShotgun::FireShotgun(const TArray<FVector_NetQuantize>& HitTargets) {
	AWeapon::Fire(FVector());

	// Get gun's owner
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn == nullptr) return;
	AController* InstigatorController = OwnerPawn->GetController();

	// Perform Line Trace
	const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName("MuzzleFlash");
	if (MuzzleFlashSocket) {
		const FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
		const FVector Start = SocketTransform.GetLocation();
	
		// Maps hit character to num of times hit
		TMap<AShooterCharacter*, uint32> HitMap;
		
		for (FVector_NetQuantize HitTarget : HitTargets) {
			FHitResult FireHit;
			WeaponTraceHit(Start, HitTarget, FireHit);

			AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(FireHit.GetActor());
			if (ShooterCharacter) {
				// If pellet hit an enemy, increment the counter for the times it's hit that enemy
				// If enemy hasn't been previously hit, add them to map and set hit count to 1
				if (HitMap.Contains(ShooterCharacter)) {
					HitMap[ShooterCharacter]++;
				}
				else {
					HitMap.Emplace(ShooterCharacter, 1);
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
		}

		TArray<AShooterCharacter*> HitCharacters;
		// Apply damage (to each character that's hit if there r more than 1) based on amount of hits
		for (auto HitPair : HitMap) {
			if (HitPair.Key && InstigatorController) {
				if (HasAuthority() && !bUseServerSideRewind) {
					// Hit character, deal damage
					UGameplayStatics::ApplyDamage(
						HitPair.Key,				// Character that was hit
						Damage * HitPair.Value,		// Multiplies damage by num times hit
						InstigatorController,
						this,
						UDamageType::StaticClass()
					);
				}
				HitCharacters.Add(HitPair.Key);
			}
		}

		if (!HasAuthority() && bUseServerSideRewind) {
			ShooterOwnerCharacter = ShooterOwnerCharacter == nullptr ? Cast<AShooterCharacter>(OwnerPawn) : ShooterOwnerCharacter;
			ShooterOwnerController = ShooterOwnerController == nullptr ? Cast<AShooterPlayerController>(InstigatorController) : ShooterOwnerController;
			if (ShooterOwnerController && ShooterOwnerCharacter && ShooterOwnerCharacter->GetLagCompensation() && ShooterOwnerCharacter->IsLocallyControlled()) {
				ShooterOwnerCharacter->GetLagCompensation()->ShotgunServerScoreRequest(HitCharacters, Start, HitTargets, ShooterOwnerController->GetServerTime() - ShooterOwnerController->SingleTripTime, this);
			}
		}

	}
}

void AShotgun::ShotgunTraceEndWithScatter(const FVector& HitTarget, TArray<FVector_NetQuantize>& HitTargets) {
	// Perform Line Trace
	const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName("MuzzleFlash");
	if (MuzzleFlashSocket == nullptr) return;

	// Get Socket muzzle socket
	const FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
	const FVector TraceStart = SocketTransform.GetLocation();

	const FVector ToTargetNormalized = (HitTarget - TraceStart).GetSafeNormal();
	const FVector SphereCenter = TraceStart + ToTargetNormalized * DistanceToSphere;

	
	for (uint32 i = 0; i < NumberOfPellets; i++) {
		const FVector RandVec = UKismetMathLibrary::RandomUnitVector() * FMath::FRandRange(0.f, SphereRadius);
		const FVector EndLoc = SphereCenter + RandVec;
		FVector ToEndLoc = EndLoc - TraceStart;
		ToEndLoc = TraceStart + ToEndLoc * TRACE_LENGTH / ToEndLoc.Size();
		HitTargets.Add(ToEndLoc);
	}	
}
