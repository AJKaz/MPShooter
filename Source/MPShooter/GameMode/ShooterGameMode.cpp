// Fill out your copyright notice in the Description page of Project Settings.


#include "ShooterGameMode.h"
#include "MPShooter/Character/ShooterCharacter.h"
#include "MPShooter/PlayerController/ShooterPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "MPShooter/PlayerState/ShooterPlayerState.h"
#include "MPShooter/GameState/ShooterGameState.h"

namespace MatchState {
	const FName Cooldown = FName("Cooldown");
}

AShooterGameMode::AShooterGameMode() {
	bDelayedStart = true;
}

void AShooterGameMode::BeginPlay() {
	Super::BeginPlay();

	LevelStartingTime = GetWorld()->GetTimeSeconds();
}

void AShooterGameMode::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

	// Check to see if in waiting to start state
	if (MatchState == MatchState::WaitingToStart) {
		CountdownTime = WarmupTime - GetWorld()->GetTimeSeconds() + LevelStartingTime;
		if (CountdownTime <= 0.f) {
			StartMatch();
		}
	}
	else if (MatchState == MatchState::InProgress) {
		CountdownTime = WarmupTime + MatchTime - GetWorld()->GetTimeSeconds() + LevelStartingTime;
		if (CountdownTime <= 0.f) {
			SetMatchState(MatchState::Cooldown);
		}
	}
	else if (MatchState == MatchState::Cooldown) {
		CountdownTime = CooldownTime + WarmupTime + MatchTime - GetWorld()->GetTimeSeconds() + LevelStartingTime;
		if (CountdownTime <= 0.f) {
			RestartGame();
		}
	}
}

void AShooterGameMode::OnMatchStateSet() {
	Super::OnMatchStateSet();

	// Loop thru all player controllers and set the match state
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It) {
		AShooterPlayerController* ShooterPlayer = Cast<AShooterPlayerController>(*It);
		if (ShooterPlayer) {
			ShooterPlayer->OnMatchStateSet(MatchState);
		}
	}
}

void AShooterGameMode::PlayerEliminated(AShooterCharacter* ElimmedCharacter, AShooterPlayerController* VictimController, AShooterPlayerController* AttackerController) {
	AShooterPlayerState* AttackerPlayerState = AttackerController ? Cast<AShooterPlayerState>(AttackerController->PlayerState) : nullptr;
	AShooterPlayerState* VictimPlayerState = VictimController ? Cast<AShooterPlayerState>(VictimController->PlayerState) : nullptr;

	AShooterGameState* ShooterGameState = GetGameState<AShooterGameState>();

	// Add kills to kill counter
	if (AttackerPlayerState && AttackerPlayerState != VictimPlayerState && ShooterGameState) {
		AttackerPlayerState->AddToScore(1.f);
		ShooterGameState->UpdateTopScore(AttackerPlayerState);
	}
	if (VictimPlayerState) {
		VictimPlayerState->AddToDeaths(1);
	}
	// Display "Killed by (player)" message
	if (AttackerPlayerState && VictimPlayerState) {
		FString PlayerName = AttackerPlayerState->GetPlayerName();		
		VictimPlayerState->UpdateDeathMessage(PlayerName);
	}

	if (ElimmedCharacter) {
		ElimmedCharacter->Elim();
	}
}

void AShooterGameMode::RequestRespawn(ACharacter* ElimmedCharacter, AController* ElimmedController) {
	// First destroy old character, then attempt to respawn character
	if (ElimmedCharacter) {
		ElimmedCharacter->Reset();
		ElimmedCharacter->Destroy();
	}
	// Respawn character at random point in world
	if (ElimmedController) {
		TArray<AActor*> PlayerSpawns;
		UGameplayStatics::GetAllActorsOfClass(this, APlayerStart::StaticClass(), PlayerSpawns);
		// Generate random spawn point
		int32 Selection = FMath::RandRange(0, PlayerSpawns.Num() - 1);
		RestartPlayerAtPlayerStart(ElimmedController, PlayerSpawns[Selection]);
	}
}

