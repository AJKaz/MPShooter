// Fill out your copyright notice in the Description page of Project Settings.


#include "BuffComponent.h"
#include "MPShooter/Character/ShooterCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MPShooter/ShooterComponents/CombatComponent.h"

UBuffComponent::UBuffComponent() {
	PrimaryComponentTick.bCanEverTick = true;

}

void UBuffComponent::BeginPlay() {
	Super::BeginPlay();
}

void UBuffComponent::SetInitialSpeed(float BaseSpeed, float CrouchSpeed) {
	InitialBaseSpeed = BaseSpeed;
	InitialCrouchSpeed = CrouchSpeed;
}

void UBuffComponent::SetInitialJumpVelocity(float Velocity) {
	InitialJumpVelocity = Velocity;
}

void UBuffComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	HealRampUp(DeltaTime);
	ShieldRampUp(DeltaTime);
}

void UBuffComponent::Heal(float HealAmount, float HealingTime) {
	bHealing = true;
	HealingRate = HealAmount / HealingTime;
	AmountToHeal += HealAmount;
}

void UBuffComponent::RegenShield(float ShieldAmount, float RegenTime) {
	bRegeningShield = true;
	ShieldRegenRate = ShieldAmount / RegenTime;
	ShieldRegenAmount += ShieldAmount;
}

void UBuffComponent::HealRampUp(float DeltaTime) {
	// If not healing, or character is dead return
	if (!bHealing || Character == nullptr || Character->IsElimmed()) return;

	// Figure out how much to heal THIS frame
	const float HealThisFrame = HealingRate * DeltaTime;
	
	// Heal Character, clamping value between 0 and MaxHealth
	Character->SetHealth(FMath::Clamp(Character->GetHealth() + HealThisFrame, 0.f, Character->GetMaxHealth()));

	Character->UpdateHUDHealth();
	AmountToHeal -= HealThisFrame;

	// Stop healing
	if (AmountToHeal <= 0.f || Character->GetHealth() >= Character->GetMaxHealth()) {
		bHealing = false;
		AmountToHeal = 0.f;
	}
}

void UBuffComponent::ShieldRampUp(float DeltaTime) {
	// If not regening, or character is dead return
	if (!bRegeningShield || Character == nullptr || Character->IsElimmed()) return;

	// Figure out how much to regen THIS frame
	const float RegenThisFrame = ShieldRegenRate * DeltaTime;

	// Regen shield, clamping value between 0 and MaxShield
	Character->SetShield(FMath::Clamp(Character->GetShield() + RegenThisFrame, 0.f, Character->GetMaxShield()));

	Character->UpdateHUDShield();
	ShieldRegenAmount -= RegenThisFrame;

	// Stop regening shield
	if (ShieldRegenAmount <= 0.f || Character->GetShield() >= Character->GetMaxShield()) {
		bRegeningShield = false;
		ShieldRegenAmount = 0.f;
	}
}

void UBuffComponent::BuffSpeed(float BuffBaseSpeed, float BuffCrouchSpeed, float BuffDuration) {
	if (Character == nullptr) return;
	
	// Increases players speed for BuffDuration time
	Character->GetWorldTimerManager().SetTimer(SpeedBuffTimer, this, &UBuffComponent::ResetSpeed, BuffDuration);

	// Set character's walk and crouch speeds
	if (Character->GetCharacterMovement()) {
		Character->GetCharacterMovement()->MaxWalkSpeed = BuffBaseSpeed;
		Character->GetCharacterMovement()->MaxWalkSpeedCrouched = BuffCrouchSpeed;
		MulticastSpeedBuff(BuffBaseSpeed, BuffCrouchSpeed);
	}
}


void UBuffComponent::ResetSpeed() {
	if (Character && Character->GetCharacterMovement()) {
		Character->GetCharacterMovement()->MaxWalkSpeed = InitialBaseSpeed;
		Character->GetCharacterMovement()->MaxWalkSpeedCrouched = InitialCrouchSpeed;
		MulticastSpeedBuff(InitialBaseSpeed, InitialCrouchSpeed);
	}	
}

void UBuffComponent::MulticastSpeedBuff_Implementation(float BaseSpeed, float CrouchSpeed) {
	if (Character && Character->GetCharacterMovement()) {
		Character->GetCharacterMovement()->MaxWalkSpeed = BaseSpeed;
		Character->GetCharacterMovement()->MaxWalkSpeedCrouched = CrouchSpeed;
		if (Character->GetCombat()) {
			Character->GetCombat()->SetSpeed(BaseSpeed);
		}
	}
}

void UBuffComponent::BuffJump(float BuffJumpVelocity, float BuffDuration) {
	if (Character == nullptr) return;

	// Increases players jump velocity for BuffDuration time
	Character->GetWorldTimerManager().SetTimer(JumpBuffTimer, this, &UBuffComponent::ResetJump, BuffDuration);

	// Change character's jump Z velocity
	if (Character->GetCharacterMovement()) {
		Character->GetCharacterMovement()->JumpZVelocity = BuffJumpVelocity;		
	}
	MulticastJumpBuff(BuffJumpVelocity);
}

void UBuffComponent::MulticastJumpBuff_Implementation(float JumpVelocity) {
	// Change character's jump Z velocity
	if (Character && Character->GetCharacterMovement()) {
		Character->GetCharacterMovement()->JumpZVelocity = JumpVelocity;
	}
}

void UBuffComponent::ResetJump() {
	// Change character's jump Z velocity
	if (Character && Character->GetCharacterMovement()) {
		Character->GetCharacterMovement()->JumpZVelocity = InitialJumpVelocity;
		MulticastJumpBuff(InitialJumpVelocity);
	}
}