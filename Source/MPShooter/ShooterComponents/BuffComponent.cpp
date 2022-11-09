// Fill out your copyright notice in the Description page of Project Settings.


#include "BuffComponent.h"
#include "MPShooter/Character/ShooterCharacter.h"

UBuffComponent::UBuffComponent() {
	PrimaryComponentTick.bCanEverTick = true;

}

void UBuffComponent::BeginPlay() {
	Super::BeginPlay();
}

void UBuffComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	HealRampUp(DeltaTime);
}

void UBuffComponent::Heal(float HealAmount, float HealingTime) {
	bHealing = true;
	HealingRate = HealAmount / HealingTime;
	AmountToHeal += HealAmount;
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