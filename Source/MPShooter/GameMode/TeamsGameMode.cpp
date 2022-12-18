// Fill out your copyright notice in the Description page of Project Settings.


#include "TeamsGameMode.h"
#include "MPShooter/GameState/ShooterGameState.h"
#include "MPShooter/PlayerState/ShooterPlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "MPShooter/PlayerController/ShooterPlayerController.h"

ATeamsGameMode::ATeamsGameMode() {
	bTeamsMatch = true;
}

void ATeamsGameMode::PostLogin(APlayerController* NewPlayer) {
	Super::PostLogin(NewPlayer);

	// Sort new player onto a team
	AShooterGameState* SGameState = Cast<AShooterGameState>(UGameplayStatics::GetGameState(this));
	if (SGameState) {
		AShooterPlayerState* SPState = NewPlayer->GetPlayerState<AShooterPlayerState>();
		if (SPState && SPState->GetTeam() == ETeam::ET_NoTeam) {
			// player has no team, put them on team w/ least # of players
			if (SGameState->BlueTeam.Num() >= SGameState->RedTeam.Num()) {
				// add to red team
				SGameState->RedTeam.AddUnique(SPState);
				SPState->SetTeam(ETeam::ET_RedTeam);
			}
			else {
				// add to blue team
				SGameState->BlueTeam.AddUnique(SPState);
				SPState->SetTeam(ETeam::ET_BlueTeam);
			}
		}
	}
}

void ATeamsGameMode::HandleMatchHasStarted() {
	Super::HandleMatchHasStarted();

	// Sort players onto teams
	AShooterGameState* SGameState = Cast<AShooterGameState>(UGameplayStatics::GetGameState(this));
	if (SGameState) {
		for (auto PState : SGameState->PlayerArray) {
			AShooterPlayerState* SPState = Cast<AShooterPlayerState>(PState.Get());
			if (SPState && SPState->GetTeam() == ETeam::ET_NoTeam) {
				// player has no team, put them on team w/ least # of players
				if (SGameState->BlueTeam.Num() >= SGameState->RedTeam.Num()) {
					// add to red team
					SGameState->RedTeam.AddUnique(SPState);
					SPState->SetTeam(ETeam::ET_RedTeam);
				}
				else {
					// add to blue team
					SGameState->BlueTeam.AddUnique(SPState);
					SPState->SetTeam(ETeam::ET_BlueTeam);
				}
			}
		}
	}
}

void ATeamsGameMode::Logout(AController* Exiting) {
	// remove player from their team
	AShooterGameState* SGameState = Cast<AShooterGameState>(UGameplayStatics::GetGameState(this));
	AShooterPlayerState* SPState = Exiting->GetPlayerState<AShooterPlayerState>();
	if (SGameState && SPState) {
		if (SGameState->RedTeam.Contains(SPState)) {
			SGameState->RedTeam.Remove(SPState);
		}
		if (SGameState->BlueTeam.Contains(SPState)) {
			SGameState->BlueTeam.Remove(SPState);
		}
	}

}

float ATeamsGameMode::CalculateDamage(AController* Attacker, AController* Victim, float BaseDamage) {
	AShooterPlayerState* AttackerPState = Attacker->GetPlayerState<AShooterPlayerState>();
	AShooterPlayerState* VictimPState = Victim->GetPlayerState<AShooterPlayerState>();
	if (AttackerPState == nullptr || VictimPState == nullptr) return BaseDamage;
	if (VictimPState == AttackerPState) return BaseDamage;
	if (AttackerPState->GetTeam() == VictimPState->GetTeam()) {
		// same team, deal 0 damage
		return 0.f;
	}
	return BaseDamage;
}

void ATeamsGameMode::PlayerEliminated(AShooterCharacter* ElimmedCharacter, AShooterPlayerController* VictimController, AShooterPlayerController* AttackerController) {
	Super::PlayerEliminated(ElimmedCharacter, VictimController, AttackerController);
	AShooterGameState* SGameState = Cast<AShooterGameState>(UGameplayStatics::GetGameState(this));
	AShooterPlayerState* AttackerPState = AttackerController ? Cast<AShooterPlayerState>(AttackerController->PlayerState) : nullptr;
	if (SGameState && AttackerPState) {
		if (AttackerPState->GetTeam() == ETeam::ET_BlueTeam) {
			SGameState->BlueTeamScores();
		}
		if (AttackerPState->GetTeam() == ETeam::ET_RedTeam) {
			SGameState->RedTeamScores();
		}
	}
}
