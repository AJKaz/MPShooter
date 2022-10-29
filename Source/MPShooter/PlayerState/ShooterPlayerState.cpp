// Fill out your copyright notice in the Description page of Project Settings.


#include "ShooterPlayerState.h"
#include "MPShooter/Character/ShooterCharacter.h"
#include "MPShooter/PlayerController/ShooterPlayerController.h"
#include "Net/UnrealNetwork.h"

void AShooterPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AShooterPlayerState, Deaths);
	DOREPLIFETIME(AShooterPlayerState, KilledBy);
}


void AShooterPlayerState::AddToScore(float ScoreAmount) {
	SetScore(GetScore() + ScoreAmount);
	Character = Character == nullptr ? Cast<AShooterCharacter>(GetPawn()) : Character;
	if (Character) {
		Controller = Controller == nullptr ? Cast<AShooterPlayerController>(Character->Controller) : Controller;
		if (Controller) {
			Controller->SetHUDScore(GetScore());
		}
	}
}

void AShooterPlayerState::OnRep_Score() {
	Super::OnRep_Score();

	Character = Character == nullptr ? Cast<AShooterCharacter>(GetPawn()) : Character;
	if (Character) {
		Controller = Controller == nullptr ? Cast<AShooterPlayerController>(Character->Controller) : Controller;
		if (Controller) {
			Controller->SetHUDScore(Score);
		}
	}
}

void AShooterPlayerState::AddToDeaths(int32 DeathsAmount) {
	Deaths += DeathsAmount;
	Character = Character == nullptr ? Cast<AShooterCharacter>(GetPawn()) : Character;
	if (Character) {
		Controller = Controller == nullptr ? Cast<AShooterPlayerController>(Character->Controller) : Controller;
		if (Controller) {
			Controller->SetHUDDeaths(Deaths);
		}
	}
}

void AShooterPlayerState::OnRep_Deaths() {
	Character = Character == nullptr ? Cast<AShooterCharacter>(GetPawn()) : Character;
	if (Character) {
		Controller = Controller == nullptr ? Cast<AShooterPlayerController>(Character->Controller) : Controller;
		if (Controller) {
			Controller->SetHUDDeaths(Deaths);
		}
	}
}

void AShooterPlayerState::UpdateDeathMessage(FString KillerName) {
	KilledBy = KillerName;
	Character = Character == nullptr ? Cast<AShooterCharacter>(GetPawn()) : Character;
	if (Character) {
		Controller = Controller == nullptr ? Cast<AShooterPlayerController>(Character->Controller) : Controller;
		if (Controller) {
			Controller->UpdateDeathMessage(KilledBy);
		}
	}
}

void AShooterPlayerState::OnRep_KilledBy() {
	Character = Character == nullptr ? Cast<AShooterCharacter>(GetPawn()) : Character;
	if (Character) {
		Controller = Controller == nullptr ? Cast<AShooterPlayerController>(Character->Controller) : Controller;
		if (Controller) {
			Controller->UpdateDeathMessage(KilledBy);
		}
	}
}