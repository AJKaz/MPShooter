// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BuffComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MPSHOOTER_API UBuffComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UBuffComponent();
	friend class AShooterCharacter;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void SetInitialSpeed(float BaseSpeed, float CrouchSpeed);
	void SetInitialJumpVelocity(float Velocity);

	void Heal(float HealAmount, float HealingTime);
	void RegenShield(float ShieldAmount, float RegenTime);
	void BuffSpeed(float BuffBaseSpeed, float BuffCrouchSpeed, float BuffDuration);
	void BuffJump(float BuffJumpVelocity, float BuffDuration);

protected:
	virtual void BeginPlay() override;

	/* Healing */
	void HealRampUp(float DeltaTime);

	/* Shield Regen */
	void ShieldRampUp(float DeltaTime);

private:
	UPROPERTY()
	class AShooterCharacter* Character;

	/**
	* Healing Variables
	*/
	bool bHealing = false;
	float HealingRate = 0.f;
	float AmountToHeal = 0.f;

	/**  
	* Sheild Variables
	*/
	bool bRegeningShield = false;
	float ShieldRegenRate = 0.f;
	float ShieldRegenAmount = 0.f;

	/**
	* Speed Buff
	*/
	FTimerHandle SpeedBuffTimer;
	void ResetSpeed();
	float InitialBaseSpeed;
	float InitialCrouchSpeed;
	
	UFUNCTION(NetMulticast, Reliable)
	void MulticastSpeedBuff(float BaseSpeed, float CrouchSpeed);

	/**
	* Jump Buff
	*/
	FTimerHandle JumpBuffTimer;
	void ResetJump();
	float InitialJumpVelocity;
	UFUNCTION(NetMulticast, Reliable)
	void MulticastJumpBuff(float JumpVelocity);
};
