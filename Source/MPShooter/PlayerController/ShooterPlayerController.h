// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ShooterPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class MPSHOOTER_API AShooterPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:

	void SetHUDHealth(float Health, float MaxHealth);
	void SetHUDScore(float Score);
	void SetHUDDeaths(int32 Deaths);
	void DisplayDeathMessage(const FString KilledBy);
	void HideDeathMessage();
	void SetHUDWeaponAmmo(int32 Ammo);
	void SetHUDCarriedAmmo(int32 Ammo);
	void SetHUDMatchCountdown(float CountdownTime);

	virtual void Tick(float DeltaTime) override;
	virtual void OnPossess(APawn* InPawn) override;

protected:

	virtual void BeginPlay() override;
	void SetHUDTime();

private:
	UPROPERTY()
	class AShooterHUD* ShooterHUD;

	float MatchTime = 120.f;
	uint32 CountdownInt = 0;

};
