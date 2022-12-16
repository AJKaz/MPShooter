// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "ShooterGameMode.generated.h"

namespace MatchState {
	extern MPSHOOTER_API const FName Cooldown;	// Match duration ended. Display winner and begin cooldown timer
}

/**
 * 
 */
UCLASS()
class MPSHOOTER_API AShooterGameMode : public AGameMode
{
	GENERATED_BODY()
	
public:

	AShooterGameMode();
	virtual void Tick(float DeltaTime) override;

	virtual void PlayerEliminated(class AShooterCharacter* ElimmedCharacter, class AShooterPlayerController* VictimController, AShooterPlayerController* AttackerController);
	void PlayerEliminated(AShooterCharacter* ElimmedCharacter, AShooterPlayerController* VictimController);
	
	virtual void RequestRespawn(ACharacter* ElimmedCharacter, AController* ElimmedController);

	void PlayerLeftGame(class AShooterPlayerState* PlayerLeaving);

	/**
	* Game times
	*/

	UPROPERTY(EditDefaultsOnly)
	float WarmupTime = 10.f;

	UPROPERTY(EditDefaultsOnly)
	float MatchTime = 120.f;

	float LevelStartingTime = 0.f;

	UPROPERTY(EditDefaultsOnly)
	float CooldownTime = 10.f;

protected:

	virtual void BeginPlay() override;
	virtual void OnMatchStateSet() override;

private:

	float CountdownTime = 0.f; 

public:
	FORCEINLINE float GetCountdownTime() const { return CountdownTime; }

};
