// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DeathBarrier.generated.h"

UCLASS()
class MPSHOOTER_API ADeathBarrier : public AActor
{
	GENERATED_BODY()
	
public:	
	ADeathBarrier();	

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	UPROPERTY(EditAnywhere)
	class UBoxComponent* DeathBarrier;

	
};
