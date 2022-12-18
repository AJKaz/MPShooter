// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "MPShooter/ShooterTypes/Team.h"
#include "ShooterPlayerState.generated.h"

/**
 * 
 */
UCLASS()
class MPSHOOTER_API AShooterPlayerState : public APlayerState
{
	GENERATED_BODY()
	
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	/* 
	* Replication notifies
	*/
	virtual void OnRep_Score() override;

	UFUNCTION()
	void OnRep_Deaths();

	void AddToScore(float ScoreAmount);
	void AddToDeaths(int32 DeathsAmount);
	void UpdateDeathMessage(FString KillerName);

private:
	UPROPERTY()
	class AShooterCharacter* Character;
	UPROPERTY()
	class AShooterPlayerController* Controller;

	UPROPERTY(ReplicatedUsing = OnRep_Deaths)
	int32 Deaths;	

	UFUNCTION(NetMulticast, Reliable)
	void MulticastDeathMessage(const FString& KillerName);

	/**
	* Team Stuff
	*/

	UPROPERTY(ReplicatedUsing = OnRep_Team)
	ETeam Team = ETeam::ET_NoTeam;

	UFUNCTION()
	void OnRep_Team();

public:
	FORCEINLINE ETeam GetTeam() const { return Team; }
	void SetTeam(ETeam TeamToSet);

};
