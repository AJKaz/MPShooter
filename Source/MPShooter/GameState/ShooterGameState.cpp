// Fill out your copyright notice in the Description page of Project Settings.


#include "ShooterGameState.h"
#include "Net/UnrealNetwork.h"
#include "MPShooter/PlayerState/ShooterPlayerState.h"
#include "MPShooter/PlayerController/ShooterPlayerController.h"

void AShooterGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AShooterGameState, TopScoringPlayers);
	DOREPLIFETIME(AShooterGameState, RedTeamScore);
	DOREPLIFETIME(AShooterGameState, BlueTeamScore);
}

void AShooterGameState::UpdateTopScore(AShooterPlayerState* ScoringPlayer) {
	if (TopScoringPlayers.Num() == 0) {
		TopScoringPlayers.Add(ScoringPlayer);
		TopScore = ScoringPlayer->GetScore();
	}
	else if(ScoringPlayer->GetScore() == TopScore) {
		TopScoringPlayers.AddUnique(ScoringPlayer);
	}
	else if (ScoringPlayer->GetScore() > TopScore) {
		TopScoringPlayers.Empty();
		TopScoringPlayers.AddUnique(ScoringPlayer);
		TopScore = ScoringPlayer->GetScore();
	}

}

void AShooterGameState::RedTeamScores() {
	++RedTeamScore;
	AShooterPlayerController* SPlayer = Cast<AShooterPlayerController>(GetWorld()->GetFirstPlayerController());
	if (SPlayer) {
		SPlayer->SetHUDRedTeamScore(RedTeamScore);
	}
}

void AShooterGameState::BlueTeamScores() {
	++BlueTeamScore;
	AShooterPlayerController* SPlayer = Cast<AShooterPlayerController>(GetWorld()->GetFirstPlayerController());
	if (SPlayer) {
		SPlayer->SetHUDBlueTeamScore(BlueTeamScore);
	}
}

void AShooterGameState::OnRep_RedTeamScore() {
	AShooterPlayerController* SPlayer = Cast<AShooterPlayerController>(GetWorld()->GetFirstPlayerController());
	if (SPlayer) {
		SPlayer->SetHUDRedTeamScore(RedTeamScore);
	}
}

void AShooterGameState::OnRep_BlueTeamScore() {
	AShooterPlayerController* SPlayer = Cast<AShooterPlayerController>(GetWorld()->GetFirstPlayerController());
	if (SPlayer) {
		SPlayer->SetHUDBlueTeamScore(BlueTeamScore);
	}
}
