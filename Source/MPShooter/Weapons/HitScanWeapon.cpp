// Fill out your copyright notice in the Description page of Project Settings.


#include "HitScanWeapon.h"
#include "Engine/SkeletalMeshSocket.h"
#include "MPShooter/Character/ShooterCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundCue.h"
#include "WeaponTypes.h"
#include "DrawDebugHelpers.h"
#include "MPShooter/ShooterComponents/LagCompensationComponent.h"
#include "MPShooter/PlayerController/ShooterPlayerController.h"

void AHitScanWeapon::Fire(const FVector& HitTarget) {
	Super::Fire(HitTarget);

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn == nullptr) return;
	AController* InstigatorController = OwnerPawn->GetController();

	// Perform Line Trace
	const USkeletalMeshSocket* MuzzleFlashSocket = GetWeaponMesh()->GetSocketByName("MuzzleFlash");
	if (MuzzleFlashSocket) {
		FTransform SocketTransform = MuzzleFlashSocket->GetSocketTransform(GetWeaponMesh());
		FVector Start = SocketTransform.GetLocation();
		
		FHitResult FireHit;
		WeaponTraceHit(Start, HitTarget, FireHit);

		AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(FireHit.GetActor());
		// Apply damage
		if (ShooterCharacter && InstigatorController) {
			bool bCauseAuthDamage = !bUseServerSideRewind || OwnerPawn->IsLocallyControlled();
			if (HasAuthority() && bCauseAuthDamage) {
				// Check if hit headshot
				const float DamageToApply = FireHit.BoneName.ToString() == FString("head") ? HeadshotDamage : Damage;

				// Hit character, deal damage, no need for ServerSideRewind cause on server
				UGameplayStatics::ApplyDamage(
					ShooterCharacter,
					DamageToApply,
					InstigatorController,
					this,
					UDamageType::StaticClass()
				);
			}
			if (!HasAuthority() && bUseServerSideRewind) {
				ShooterOwnerCharacter = ShooterOwnerCharacter == nullptr ? Cast<AShooterCharacter>(OwnerPawn) : ShooterOwnerCharacter;
				ShooterOwnerController = ShooterOwnerController == nullptr ? Cast<AShooterPlayerController>(InstigatorController) : ShooterOwnerController;
				if (ShooterOwnerController && ShooterOwnerCharacter && ShooterOwnerCharacter->GetLagCompensation() && ShooterOwnerCharacter->IsLocallyControlled()) {
					ShooterOwnerCharacter->GetLagCompensation()->ServerScoreRequest(
						ShooterCharacter,
						Start,
						HitTarget,
						ShooterOwnerController->GetServerTime() - ShooterOwnerController->SingleTripTime,
						this
					);
				}
			}
			
		}
		// Spawn impact particles
		if (ImpactParticles) {
			UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ImpactParticles, FireHit.ImpactPoint, FireHit.ImpactNormal.Rotation());
		}
		// Play hit sound
		if (HitSound) {
			UGameplayStatics::PlaySoundAtLocation(this, HitSound, FireHit.ImpactPoint);
		}				
		// Display Muzzle Flash
		if (MuzzleFlash) {
			UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), MuzzleFlash, SocketTransform);
		}
		// Play Fire Sound
		if (FireSound) {
			UGameplayStatics::PlaySoundAtLocation(this, FireSound, GetActorLocation());
		}

	}
}

void AHitScanWeapon::WeaponTraceHit(const FVector& TraceStart, const FVector& HitTarget, FHitResult& OutHit) {	
	UWorld* World = GetWorld();
	if (World) {
		// Perform Line Trace (using scatter if bUseScatter is true)
		FVector End = TraceStart + (HitTarget - TraceStart) * 1.25;		
		World->LineTraceSingleByChannel(OutHit, TraceStart, End, ECollisionChannel::ECC_Visibility);
		FVector BeamEnd = End;
		if (OutHit.bBlockingHit) {
			BeamEnd = OutHit.ImpactPoint;						
		}
		else {
			OutHit.ImpactPoint = End;
		}

		// Draw debug sphere for each shot
		// DrawDebugSphere(GetWorld(), BeamEnd, 16.f, 12, FColor::Orange, true);

		// Spawn Beam particles
		if (BeamParticles) {
			UParticleSystemComponent* Beam = UGameplayStatics::SpawnEmitterAtLocation(
				World,
				BeamParticles,
				TraceStart,
				FRotator::ZeroRotator,
				true
			);
			if (Beam) {
				Beam->SetVectorParameter(FName("Target"), BeamEnd);
			}
		}
	}
}

