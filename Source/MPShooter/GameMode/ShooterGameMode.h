// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "ShooterGameMode.generated.h"

/**
 * 
 */
UCLASS()
class MPSHOOTER_API AShooterGameMode : public AGameMode
{
	GENERATED_BODY()
	
public:

	virtual void PlayerEliminated(class AShooterCharacter* ElimmedCharacter, class AShooterPlayerController* VictimController, AShooterPlayerController* AttackerController);

};
