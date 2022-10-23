// Fill out your copyright notice in the Description page of Project Settings.


#include "ShooterCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/WidgetComponent.h"
#include "Net/UnrealNetwork.h"
#include "MPShooter/Weapons/Weapon.h"
#include "MPShooter/ShooterComponents/CombatComponent.h"


AShooterCharacter::AShooterCharacter() {
	PrimaryActorTick.bCanEverTick = true;

	// Setup spring arm from player to camera
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetMesh());
	// Controls distance camera is from player:
	CameraBoom->TargetArmLength = 550.f;
	CameraBoom->bUsePawnControlRotation = true;

	// Attach Camera to camera arm (CameraBoom)
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Make character independent of camera rotation (for now)
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;

	// Let character move whilst in air (set to 1.f for 100% movement):
	GetCharacterMovement()->AirControl = 0.75f;

	OverheadWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("OverheadWidget"));
	OverheadWidget->SetupAttachment(RootComponent);

	Combat = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	Combat->SetIsReplicated(true);

	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;

}

void AShooterCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AShooterCharacter, OverlappingWeapon, COND_OwnerOnly);
}

void AShooterCharacter::BeginPlay() {
	Super::BeginPlay();

}

void AShooterCharacter::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

}


void AShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) {
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);

	PlayerInputComponent->BindAxis("MoveForward", this, &ThisClass::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ThisClass::MoveRight);

	PlayerInputComponent->BindAxis("Turn", this, &ThisClass::Turn);
	PlayerInputComponent->BindAxis("LookUp", this, &ThisClass::LookUp);

	PlayerInputComponent->BindAction("Interact", IE_Pressed, this, &ThisClass::InteractButtonPressed);

	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &ThisClass::CrouchButtonPressed);

	PlayerInputComponent->BindAction("ADS", IE_Pressed, this, &ThisClass::ADSButtonPressed);
	PlayerInputComponent->BindAction("ADS", IE_Released, this, &ThisClass::ADSButtonReleased);
}

void AShooterCharacter::PostInitializeComponents() {
	Super::PostInitializeComponents();
	if (Combat) {
		Combat->Character = this;
	}
}

void AShooterCharacter::MoveForward(float Value) {
	if (Controller != nullptr && Value != 0.f) {
		const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
		// Parallel vector to ground:
		const FVector Direction(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X));
		AddMovementInput(Direction, Value);
	}
}

void AShooterCharacter::MoveRight(float Value) {
	if (Controller != nullptr && Value != 0.f) {
		const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
		// Parallel vector to ground:
		const FVector Direction(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y));
		AddMovementInput(Direction, Value);
	}
}

void AShooterCharacter::Turn(float Value) {
	AddControllerYawInput(Value);
}

void AShooterCharacter::LookUp(float Value) {
	AddControllerPitchInput(Value);
}

void AShooterCharacter::InteractButtonPressed() {
	if (Combat) {
		if (HasAuthority()) {
			Combat->EquipWeapon(OverlappingWeapon);
		}
		else {
			ServerInteractButtonPressed();
		}
	}
}

void AShooterCharacter::CrouchButtonPressed() {
	if (bIsCrouched) {
		UnCrouch();
	}
	else {
		Crouch();
	}		
}

void AShooterCharacter::ADSButtonPressed() {
	if (Combat) {
		Combat->SetAiming(true);
	}
}

void AShooterCharacter::ADSButtonReleased() {
	if (Combat) {
		Combat->SetAiming(false);
	}
}

void AShooterCharacter::ServerInteractButtonPressed_Implementation() {
	if (Combat) {
		Combat->EquipWeapon(OverlappingWeapon);
	}
}

void AShooterCharacter::SetOverlappingWeapon(AWeapon* Weapon) {	
	if (OverlappingWeapon) {
		OverlappingWeapon->ShowPickupWidget(false);
	}
	
	OverlappingWeapon = Weapon;

	// True if called on character being controlled by host
	if (IsLocallyControlled() && OverlappingWeapon) {
		OverlappingWeapon->ShowPickupWidget(true);		
	}
}

bool AShooterCharacter::IsWeaponEquipped() {
	return (Combat && Combat->EquippedWeapon);
}

bool AShooterCharacter::IsAiming() {
	return (Combat && Combat->bAiming);
}

void AShooterCharacter::OnRep_OverlappingWeapon(AWeapon* LastWeapon) {
	if (OverlappingWeapon) {
		OverlappingWeapon->ShowPickupWidget(true);
	}
	if (LastWeapon) {
		LastWeapon->ShowPickupWidget(false);
	}
}






