// Fill out your copyright notice in the Description page of Project Settings.


#include "ShooterPlayerController.h"
#include "MPShooter/HUD/ShooterHUD.h"
#include "MPShooter/HUD/CharacterOverlay.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "MPShooter/Character/ShooterCharacter.h"


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

void AShooterPlayerController::SetHUDScore(float Score) {
	ShooterHUD = ShooterHUD == nullptr ? Cast<AShooterHUD>(GetHUD()) : ShooterHUD;
	if (ShooterHUD && ShooterHUD->CharacterOverlay && ShooterHUD->CharacterOverlay->ScoreAmount) {
		FString ScoreText = FString::Printf(TEXT("%d"), FMath::FloorToInt(Score));
		ShooterHUD->CharacterOverlay->ScoreAmount->SetText(FText::FromString(ScoreText));
	}
}

void AShooterPlayerController::SetHUDDeaths(int32 Deaths) {
	ShooterHUD = ShooterHUD == nullptr ? Cast<AShooterHUD>(GetHUD()) : ShooterHUD;
	if (ShooterHUD && ShooterHUD->CharacterOverlay && ShooterHUD->CharacterOverlay->DeathsAmount) {
		FString DeathsText = FString::Printf(TEXT("%d"), Deaths);
		ShooterHUD->CharacterOverlay->DeathsAmount->SetText(FText::FromString(DeathsText));
	}
}

void AShooterPlayerController::UpdateDeathMessage(const FString KilledBy) {
	ShooterHUD = ShooterHUD == nullptr ? Cast<AShooterHUD>(GetHUD()) : ShooterHUD;
	if (ShooterHUD &&
		ShooterHUD->CharacterOverlay &&
		ShooterHUD->CharacterOverlay->DeathMessage &&
		ShooterHUD->CharacterOverlay->KilledBy) {
		ShooterHUD->CharacterOverlay->KilledBy->SetText(FText::FromString(KilledBy));
		ShooterHUD->CharacterOverlay->KilledBy->SetVisibility(ESlateVisibility::Visible);
		ShooterHUD->CharacterOverlay->DeathMessage->SetVisibility(ESlateVisibility::Visible);
	}
}

void AShooterPlayerController::HideDeathMessage() {
	ShooterHUD = ShooterHUD == nullptr ? Cast<AShooterHUD>(GetHUD()) : ShooterHUD;
	if (ShooterHUD &&
		ShooterHUD->CharacterOverlay &&
		ShooterHUD->CharacterOverlay->DeathMessage &&
		ShooterHUD->CharacterOverlay->KilledBy) {
		ShooterHUD->CharacterOverlay->KilledBy->SetVisibility(ESlateVisibility::Collapsed);
		ShooterHUD->CharacterOverlay->DeathMessage->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void AShooterPlayerController::OnPossess(APawn* InPawn) {
	Super::OnPossess(InPawn);

	AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(InPawn);
	if (ShooterCharacter) {
		SetHUDHealth(ShooterCharacter->GetHealth(), ShooterCharacter->GetMaxHealth());
	}
	HideDeathMessage();
	
}
