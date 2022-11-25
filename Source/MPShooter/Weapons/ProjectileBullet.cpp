// Fill out your copyright notice in the Description page of Project Settings.


#include "ProjectileBullet.h"
#include "Kismet/GameplayStatics.h"
#include "MPShooter/Character/ShooterCharacter.h"
#include "MPShooter/PlayerController/ShooterPlayerController.h"
#include "MPShooter/ShooterComponents/LagCompensationComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"

AProjectileBullet::AProjectileBullet() {
	ProjectileMovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovementComponent"));
	ProjectileMovementComponent->bRotationFollowsVelocity = true;
	ProjectileMovementComponent->SetIsReplicated(true);
	ProjectileMovementComponent->InitialSpeed = InitialSpeed;
	ProjectileMovementComponent->MaxSpeed = InitialSpeed;
}

#if WITH_EDITOR
void AProjectileBullet::PostEditChangeProperty(FPropertyChangedEvent& Event) {
	Super::PostEditChangeProperty(Event);

	// Changes Intial ProjectileMovementComponent's Speed and Max Speed every time I change Initial Speed in blueprints
	FName PropertyName = Event.Property != nullptr ? Event.Property->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AProjectileBullet, InitialSpeed)) {
		if (ProjectileMovementComponent) {
			ProjectileMovementComponent->InitialSpeed = InitialSpeed;
			ProjectileMovementComponent->MaxSpeed = InitialSpeed;
		}
	}
}
#endif

void AProjectileBullet::BeginPlay() {
	Super::BeginPlay();

	/*
	// Predict projectile's path
	FPredictProjectilePathParams PathParams;
	PathParams.bTraceWithChannel = true;
	PathParams.bTraceWithCollision = true;
	PathParams.DrawDebugTime = 5.f;
	PathParams.DrawDebugType = EDrawDebugTrace::ForDuration;
	PathParams.LaunchVelocity = GetActorForwardVector() * InitialSpeed;
	PathParams.MaxSimTime = 4.f;
	PathParams.ProjectileRadius = 5.f;
	PathParams.SimFrequency = 30.f;
	PathParams.StartLocation = GetActorLocation();
	PathParams.TraceChannel = ECollisionChannel::ECC_Visibility;
	PathParams.ActorsToIgnore.Add(this);

	FPredictProjectilePathResult PathResult;
	UGameplayStatics::PredictProjectilePath(this, PathParams, PathResult);
	*/
}


void AProjectileBullet::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit) {
	// Apply damage to player
	AShooterCharacter* OwnerCharacter = Cast<AShooterCharacter>(GetOwner());
	if (OwnerCharacter) {
		AShooterPlayerController* OwnerController = Cast<AShooterPlayerController>(OwnerCharacter->Controller);
		if (OwnerController) {
			if (OwnerCharacter->HasAuthority() && !bUseServerSideRewind) {
				// on server, no ssr, apply damage
				UGameplayStatics::ApplyDamage(OtherActor, Damage, OwnerController, this, UDamageType::StaticClass());
				Super::OnHit(HitComp, OtherActor, OtherComp, NormalImpulse, Hit);
				return;
			}
			AShooterCharacter* HitCharacter = Cast<AShooterCharacter>(OtherActor);
			if (bUseServerSideRewind && OwnerCharacter->GetLagCompensation() && OwnerCharacter->IsLocallyControlled() && HitCharacter) {
				OwnerCharacter->GetLagCompensation()->ProjectileScoreRequest(HitCharacter, TraceStart, InitialVelocity, OwnerController->GetServerTime() - OwnerController->SingleTripTime);
			}
		}		
	}
	

	Super::OnHit(HitComp, OtherActor, OtherComp, NormalImpulse, Hit);
}
