// Fill out your copyright notice in the Description page of Project Settings.


#include "ShooterGameMode.h"
#include "MPShooter/Character/ShooterCharacter.h"
#include "MPShooter/PlayerController/ShooterPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "MPShooter/PlayerState/ShooterPlayerState.h"

void AShooterGameMode::PlayerEliminated(AShooterCharacter* ElimmedCharacter, AShooterPlayerController* VictimController, AShooterPlayerController* AttackerController) {
	AShooterPlayerState* AttackerPlayerState = AttackerController ? Cast<AShooterPlayerState>(AttackerController->PlayerState) : nullptr;
	AShooterPlayerState* VictumPlayerState = VictimController ? Cast<AShooterPlayerState>(VictimController->PlayerState) : nullptr;

	if (AttackerPlayerState && AttackerPlayerState != VictumPlayerState) {
		AttackerPlayerState->AddToScore(1.f);
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
