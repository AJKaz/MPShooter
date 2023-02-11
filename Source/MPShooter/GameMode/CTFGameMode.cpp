// Fill out your copyright notice in the Description page of Project Settings.


#include "CTFGameMode.h"
#include "MPShooter/Weapons/Flag.h"
#include "MPShooter/CaptureTheFlag/FlagZone.h"
#include "MPShooter/GameState/ShooterGameState.h"

void ACTFGameMode::PlayerEliminated(class AShooterCharacter* ElimmedCharacter, class AShooterPlayerController* VictimController, AShooterPlayerController* AttackerController) {
	AShooterGameMode::PlayerEliminated(ElimmedCharacter, VictimController, AttackerController);
}

void ACTFGameMode::FlagCaptured(AFlag* Flag, AFlagZone* Zone) {
	if (Flag->GetTeam() != Zone->Team) {
		AShooterGameState* SGameState = Cast<AShooterGameState>(GameState);
		if (SGameState) {
			if (Zone->Team == ETeam::ET_BlueTeam) {
				SGameState->BlueTeamScores();
			}
			else if (Zone->Team == ETeam::ET_RedTeam) {
				SGameState->RedTeamScores();
			}
		}
	}
}
