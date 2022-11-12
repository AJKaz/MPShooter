// Fill out your copyright notice in the Description page of Project Settings.


#include "Pickup.h"
#include "Sound/SoundCue.h"
#include "Kismet/GameplayStatics.h"
#include "MPShooter/Weapons/WeaponTypes.h"
#include "Components/SphereComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"

// Sets default values
APickup::APickup() {
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// Construct overlap sphere
	OverlapSphere = CreateDefaultSubobject<USphereComponent>(TEXT("OverlapSphere"));
	OverlapSphere->SetupAttachment(RootComponent);
	OverlapSphere->SetSphereRadius(150.f);
	OverlapSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	OverlapSphere->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	OverlapSphere->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);
	OverlapSphere->AddLocalOffset(FVector(0.f, 0.f, 85.f));

	// Construct pickup's mesh
	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	PickupMesh->SetupAttachment(OverlapSphere);
	PickupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupMesh->SetRelativeScale3D(FVector(3.f, 3.f, 3.f));
	PickupMesh->SetRenderCustomDepth(true);
	PickupMesh->SetCustomDepthStencilValue(CUSTOM_DEPTH_PURPLE);

	// Create Niagara Component
	PickupEffectComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("PickupEffectComponent"));
	PickupEffectComponent->SetupAttachment(RootComponent);
}

void APickup::BeginPlay() {
	Super::BeginPlay();

	// Bind Overlap Events ONLY on server
	if (HasAuthority()) {
		OverlapSphere->OnComponentBeginOverlap.AddDynamic(this, &APickup::OnSphereOverlap);
	}	
}

void APickup::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

	// Rotate Pickups
	if (PickupMesh && bRotate) {
		PickupMesh->AddWorldRotation(FRotator(0.f, BaseTurnRate * DeltaTime, 0.f));
	}
}

void APickup::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) {
	
}

void APickup::Destroyed() {
	Super::Destroyed();
	
	// play pickup sound
	if (PickupSound) {
		UGameplayStatics::PlaySoundAtLocation(this, PickupSound, GetActorLocation());
	}

	// Spawn Niagara System for destroyed effect (if there is one)
	if (PickupEffect) {
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, PickupEffect, GetActorLocation(), GetActorRotation());
	}
}



