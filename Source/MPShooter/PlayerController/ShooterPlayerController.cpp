// Fill out your copyright notice in the Description page of Project Settings.


#include "ShooterPlayerController.h"
#include "MPShooter/HUD/ShooterHUD.h"
#include "MPShooter/HUD/CharacterOverlay.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"


void AShooterPlayerController::BeginPlay() {
	Super::BeginPlay();

	ShooterHUD = Cast<AShooterHUD>(GetHUD());
}

void AShooterPlayerController::SetHUDHealth(float Health, float MaxHealth) {
	ShooterHUD = ShooterHUD == nullptr ? Cast<AShooterHUD>(GetHUD()) : ShooterHUD;	
	if (ShooterHUD && ShooterHUD->CharacterOverlay && ShooterHUD->CharacterOverlay->HealthBar && ShooterHUD->CharacterOverlay->HealthText) {
		const float HealthPercent = Health / MaxHealth;
		ShooterHUD->CharacterOverlay->HealthBar->SetPercent(HealthPercent);
		// Round float health values up to int values and set text
		FString HealthText = FString::Printf(TEXT("%d/%d"), FMath::CeilToInt(Health), FMath::CeilToInt(MaxHealth));
		ShooterHUD->CharacterOverlay->HealthText->SetText(FText::FromString(HealthText));
	}
}
