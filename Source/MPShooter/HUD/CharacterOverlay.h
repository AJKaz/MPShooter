// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CharacterOverlay.generated.h"

/**
 * 
 */
UCLASS()
class MPSHOOTER_API UCharacterOverlay : public UUserWidget
{
	GENERATED_BODY()
	
public:
	/* Health */
	UPROPERTY(meta = (BindWidget))
	class UProgressBar* HealthBar;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* HealthText;
	
	/* Shield */
	UPROPERTY(meta = (BindWidget))
	UProgressBar* ShieldBar;

	/* Kills (score) */
	UPROPERTY(meta = (BindWidget))
	UTextBlock* ScoreAmount;

	/* Deaths */
	UPROPERTY(meta = (BindWidget))
	UTextBlock* DeathsAmount;

	/* Death message*/
	UPROPERTY(meta = (BindWidget))
	UTextBlock* KilledBy;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* DeathMessage;

	/* HUD Ammo*/
	UPROPERTY(meta = (BindWidget))
	UTextBlock* WeaponAmmoAmount;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* CarriedAmmoAmount;
	
	UPROPERTY(meta = (BindWidget))
	UTextBlock* GrenadesText;

	/* Timers */
	UPROPERTY(meta = (BindWidget))
	UTextBlock* MatchCountdownText;

	

};
