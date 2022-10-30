// Fill out your copyright notice in the Description page of Project Settings.


#include "ShooterGameMode.h"
#include "MPShooter/Character/ShooterCharacter.h"
#include "MPShooter/PlayerController/ShooterPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "MPShooter/PlayerState/ShooterPlayerState.h"

void AShooterGameMode::PlayerEliminated(AShooterCharacter* ElimmedCharacter, AShooterPlayerController* VictimController, AShooterPlayerController* AttackerController) {
	AShooterPlayerState* AttackerPlayerState = AttackerController ? Cast<AShooterPlayerState>(AttackerController->PlayerState) : nullptr;
	AShooterPlayerState* VictimPlayerState = VictimController ? Cast<AShooterPlayerState>(VictimController->PlayerState) : nullptr;

	// Add kills to kill counter
	if (AttackerPlayerState && AttackerPlayerState != VictimPlayerState) {
		AttackerPlayerState->AddToScore(1.f);
	}
	if (VictimPlayerState) {
		VictimPlayerState->AddToDeaths(1);
	}
	// Display "Killed by (player)" message
	if (AttackerPlayerState && VictimController) {
		FString PlayerName = AttackerPlayerState->GetPlayerName();		
		VictimController->UpdateDeathMessage(PlayerName);
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
