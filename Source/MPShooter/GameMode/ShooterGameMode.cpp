// Fill out your copyright notice in the Description page of Project Settings.


#include "ShooterGameMode.h"
#include "MPShooter/Character/ShooterCharacter.h"
#include "MPShooter/PlayerController/ShooterPlayerController.h"


void AShooterGameMode::PlayerEliminated(AShooterCharacter* ElimmedCharacter, AShooterPlayerController* VictimController, AShooterPlayerController* AttackerController) {
	if (ElimmedCharacter) {
		ElimmedCharacter->Elim();
	}
	
}
