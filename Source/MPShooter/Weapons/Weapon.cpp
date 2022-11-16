// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "MPShooter/Character/ShooterCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Animation/AnimationAsset.h"
#include "Casing.h"
#include "Engine/SkeletalMeshSocket.h"
#include "MPShooter/ShooterComponents/CombatComponent.h"
#include "MPShooter/PlayerController/ShooterPlayerController.h"
#include "Components/TextBlock.h"


AWeapon::AWeapon() {
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(true);

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	SetRootComponent(WeaponMesh);
	WeaponMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
	WeaponMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Ignore);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Set weapon's colored outline
	WeaponMesh->SetCustomDepthStencilValue(CUSTOM_DEPTH_BLUE);
	WeaponMesh->MarkRenderStateDirty();
	EnableCustomDepth(true);

	// Detects overlaps with characters
	AreaSphere = CreateDefaultSubobject<USphereComponent>(TEXT("AreaSphere"));
	AreaSphere->SetupAttachment(RootComponent);
	AreaSphere->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	AreaSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PickupWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("PickupWidget"));
	PickupWidget->SetupAttachment(RootComponent);

}

void AWeapon::BeginPlay() {
	Super::BeginPlay();

	// If on server, enable collision:
	if (HasAuthority()) {
		AreaSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		AreaSphere->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);
		AreaSphere->OnComponentBeginOverlap.AddDynamic(this, &AWeapon::OnSphereOverlap);
		AreaSphere->OnComponentEndOverlap.AddDynamic(this, &AWeapon::OnSphereEndOverlap);
	}

	if (PickupWidget) {
		PickupWidget->SetVisibility(false);
	}
}

void AWeapon::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

}

void AWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AWeapon, WeaponState);
	DOREPLIFETIME(AWeapon, Ammo);
}

void AWeapon::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) {
	AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(OtherActor);
	if (ShooterCharacter) {
		ShooterCharacter->SetOverlappingWeapon(this);
	}
}

void AWeapon::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex) {
	AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(OtherActor);
	if (ShooterCharacter) {
		ShooterCharacter->SetOverlappingWeapon(nullptr);
	}
}

void AWeapon::SetHUDAmmo() {
	ShooterOwnerCharacter = ShooterOwnerCharacter == nullptr ? Cast<AShooterCharacter>(GetOwner()) : ShooterOwnerCharacter;
	if (ShooterOwnerCharacter) {
		ShooterOwnerController = ShooterOwnerController == nullptr ? Cast<AShooterPlayerController>(ShooterOwnerCharacter->Controller) : ShooterOwnerController;
		if (ShooterOwnerController) {
			ShooterOwnerController->SetHUDWeaponAmmo(Ammo);
		}
	}
}

void AWeapon::SpendRound() {
	Ammo = FMath::Clamp(Ammo - 1, 0, MagCapacity);
	SetHUDAmmo();
}

void AWeapon::OnRep_Ammo() {
	ShooterOwnerCharacter = ShooterOwnerCharacter == nullptr ? Cast<AShooterCharacter>(GetOwner()) : ShooterOwnerCharacter;
	if (ShooterOwnerCharacter && ShooterOwnerCharacter->GetCombat() && IsFull()) {
		ShooterOwnerCharacter->GetCombat()->JumpToShotgunEnd();
	}

	SetHUDAmmo();
}

void AWeapon::OnRep_Owner() {
	Super::OnRep_Owner();
	if (Owner == nullptr) {
		ShooterOwnerCharacter = nullptr;
		ShooterOwnerController = nullptr;
	}
	else {
		ShooterOwnerCharacter = ShooterOwnerCharacter == nullptr ? Cast<AShooterCharacter>(Owner) : ShooterOwnerCharacter;
		if (ShooterOwnerCharacter && ShooterOwnerCharacter->GetEquippedWeapon() && ShooterOwnerCharacter->GetEquippedWeapon() == this) {
			SetHUDAmmo();
		}
	}
}

void AWeapon::SetWeaponState(EWeaponState State) {
	WeaponState = State;
	OnWeaponStateSet();
}

void AWeapon::OnRep_WeaponState() {
	OnWeaponStateSet();
}

void AWeapon::OnWeaponStateSet() {
	switch (WeaponState) {
	case EWeaponState::EWS_Equipped:
		OnEquipped();
		break;
	case EWeaponState::EWS_EquippedSecondary:
		OnEquippedSecondary();
		break;
	case EWeaponState::EWS_Dropped:
		OnDropped();
		break;
	}
}

void AWeapon::OnEquipped() {
	ShowPickupWidget(false);
	AreaSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetSimulatePhysics(false);
	WeaponMesh->SetEnableGravity(false);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Enable physics for SMG to have dangly strap
	if (WeaponType == EWeaponType::EWT_SubmachineGun) {
		WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		WeaponMesh->SetEnableGravity(true);
		WeaponMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	}
	// Disable custom depth (glowing outline)		
	EnableCustomDepth(false);
}

void AWeapon::OnEquippedSecondary() {
	ShowPickupWidget(false);
	AreaSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetSimulatePhysics(false);
	WeaponMesh->SetEnableGravity(false);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Enable physics for SMG to have dangly strap
	if (WeaponType == EWeaponType::EWT_SubmachineGun) {
		WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		WeaponMesh->SetEnableGravity(true);
		WeaponMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	}
	// Disable custom depth (glowing outline)		
	EnableCustomDepth(false);
}

void AWeapon::OnDropped() {
	if (HasAuthority()) {
		AreaSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
	WeaponMesh->SetSimulatePhysics(true);
	WeaponMesh->SetEnableGravity(true);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	WeaponMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
	WeaponMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Ignore);
	WeaponMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	// Enable custom depth (glowing outline)
	WeaponMesh->SetCustomDepthStencilValue(CUSTOM_DEPTH_BLUE);
	WeaponMesh->MarkRenderStateDirty();
	EnableCustomDepth(true);
}

void AWeapon::ShowPickupWidget(bool bShowWidget) {
	if (PickupWidget) {
		PickupWidget->SetVisibility(bShowWidget);
	}
}

void AWeapon::Fire(const FVector& HitTarget) {
	if (FireAnimation) {
		// Play fire animation
		WeaponMesh->PlayAnimation(FireAnimation, false);
	}
	if (CasingClass) {
		// Spawn bullet casing at socket called AmmoEject

		const USkeletalMeshSocket* AmmoEjectSocket = WeaponMesh->GetSocketByName(FName("AmmoEject"));
		if (AmmoEjectSocket) {
			FTransform SocketTransform = AmmoEjectSocket->GetSocketTransform(WeaponMesh);

			// Spawn Projectile:		
			UWorld* World = GetWorld();
			if (World) {
				World->SpawnActor<ACasing>(CasingClass, SocketTransform.GetLocation(), SocketTransform.GetRotation().Rotator());
			}
		}

	}
	SpendRound();
}

void AWeapon::Dropped() {
	SetWeaponState(EWeaponState::EWS_Dropped);

	// Detach weapon from character:
	FDetachmentTransformRules DetachRules(EDetachmentRule::KeepWorld, true);
	WeaponMesh->DetachFromComponent(DetachRules);
	SetOwner(nullptr);
	ShooterOwnerCharacter = nullptr;
	ShooterOwnerController = nullptr;
}

bool AWeapon::IsEmpty() {
	return Ammo <= 0;
}

bool AWeapon::IsFull() {
	return Ammo == MagCapacity;
}

void AWeapon::AddAmmo(int32 AmmoToAdd) {
	Ammo = FMath::Clamp(Ammo - AmmoToAdd, 0, MagCapacity);
	SetHUDAmmo();
}

void AWeapon::EnableCustomDepth(bool bEnable) {
	if (WeaponMesh) {
		WeaponMesh->SetRenderCustomDepth(bEnable);
	}
}