// Fill out your copyright notice in the Description page of Project Settings.


#include "ShooterAnimInstance.h"
#include "ShooterCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "MPShooter/Weapons/Weapon.h"
#include "MPShooter/ShooterTypes/CombatState.h"

void UShooterAnimInstance::NativeInitializeAnimation() {
	Super::NativeInitializeAnimation();

	ShooterCharacter = Cast<AShooterCharacter>(TryGetPawnOwner());

}

void UShooterAnimInstance::NativeUpdateAnimation(float DeltaTime) {
	Super::NativeUpdateAnimation(DeltaTime);

	if (ShooterCharacter == nullptr) {
		ShooterCharacter = Cast<AShooterCharacter>(TryGetPawnOwner());
	}
	if (ShooterCharacter == nullptr) return;

	FVector Velocity = ShooterCharacter->GetVelocity();
	Velocity.Z = 0.f;
	Speed = Velocity.Size();

	bIsInAir = ShooterCharacter->GetCharacterMovement()->IsFalling();

	// Set accelerating to true IF character's acceleration is greater than 0, false otherwise
	bIsAccelerating = ShooterCharacter->GetCharacterMovement()->GetCurrentAcceleration().Size() > 0.f ? true : false;
	bWeaponEquipped = ShooterCharacter->IsWeaponEquipped();
	EquippedWeapon = ShooterCharacter->GetEquippedWeapon();
	bIsCrouched = ShooterCharacter->bIsCrouched;
	bAiming = ShooterCharacter->IsAiming();
	TurningInPlace = ShooterCharacter->GetTurningInPlace();
	bRotateRootBone = ShooterCharacter->ShouldRotateRootBone();
	bElimmed = ShooterCharacter->IsElimmed();
	bHoldingFlag = ShooterCharacter->IsHoldingFlag();

	// Offset Yaw for Strafing
	FRotator AimRotation = ShooterCharacter->GetBaseAimRotation();
	FRotator MovementRotation = UKismetMathLibrary::MakeRotFromX(ShooterCharacter->GetVelocity());
	FRotator DeltaRot = UKismetMathLibrary::NormalizedDeltaRotator(MovementRotation, AimRotation);
	DeltaRotation = FMath::RInterpTo(DeltaRotation, DeltaRot, DeltaTime, 6.f);
	YawOffset = DeltaRotation.Yaw;

	CharacterRotationLastFrame = CharacterRotation;
	CharacterRotation = ShooterCharacter->GetActorRotation();
	const FRotator Delta = UKismetMathLibrary::NormalizedDeltaRotator(CharacterRotation, CharacterRotationLastFrame);
	const float Target = Delta.Yaw / DeltaTime;
	const float Interp = FMath::FInterpTo(Lean, Target, DeltaTime, 6.f);
	Lean = FMath::Clamp(Interp, -90.f, 90.f);

	AO_Yaw = ShooterCharacter->GetAO_Yaw();
	AO_Pitch = ShooterCharacter->GetAO_Pitch();

	// Attach left hand to weapon left hand socket
	if (bWeaponEquipped && EquippedWeapon && EquippedWeapon->GetWeaponMesh() && ShooterCharacter->GetMesh()) {
		LeftHandTransform = EquippedWeapon->GetWeaponMesh()->GetSocketTransform(FName("LeftHandSocket"), ERelativeTransformSpace::RTS_World);
		FVector OutPosition;
		FRotator OutRotation;
		ShooterCharacter->GetMesh()->TransformToBoneSpace(FName("hand_r"), LeftHandTransform.GetLocation(), FRotator::ZeroRotator, OutPosition, OutRotation);
		LeftHandTransform.SetLocation(OutPosition);
		LeftHandTransform.SetRotation(FQuat(OutRotation));

		// Rotate right hand socket to aim weapon towards where crosshair is pointed
		if (ShooterCharacter->IsLocallyControlled()) {
			// Get right hand location:
			bLocallyControlled = true;
			FTransform RightHandTransform = ShooterCharacter->GetMesh()->GetSocketTransform(FName("Hand_R"), ERelativeTransformSpace::RTS_World);
			FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(FVector3d(), (RightHandTransform.GetLocation() - ShooterCharacter->GetHitTarget()));
			RightHandRotation = FMath::RInterpTo(RightHandRotation, LookAtRotation, DeltaTime, 30.f);
		}
		/*
		// Debug: Show direction weapon is aiming
		FTransform MuzzleTipTransform = EquippedWeapon->GetWeaponMesh()->GetSocketTransform(FName("MuzzleFlash"), ERelativeTransformSpace::RTS_World);
		FVector MuzzleX(FRotationMatrix(MuzzleTipTransform.GetRotation().Rotator()).GetUnitAxis(EAxis::X));	
		DrawDebugLine(GetWorld(), MuzzleTipTransform.GetLocation(), MuzzleTipTransform.GetLocation() + MuzzleX * 1000.f, FColor::Red);
		DrawDebugLine(GetWorld(), MuzzleTipTransform.GetLocation(), ShooterCharacter->GetHitTarget(), FColor::Orange);
		*/
	}
	bUseFABRIK = ShooterCharacter->GetCombatState() == ECombatState::ECS_Unoccupied;
	if (ShooterCharacter->IsLocallyControlled() && ShooterCharacter->GetCombatState() != ECombatState::ECS_ThrowingGrenade && ShooterCharacter->bFinishedSwapping) {
		bUseFABRIK = !ShooterCharacter->IsLocallyReloading();
	}
	bUseAimOffsets = ShooterCharacter->GetCombatState() == ECombatState::ECS_Unoccupied && !ShooterCharacter->GetDisableGameplay();
	bTransformRightHand = ShooterCharacter->GetCombatState() == ECombatState::ECS_Unoccupied && !ShooterCharacter->GetDisableGameplay();
}
