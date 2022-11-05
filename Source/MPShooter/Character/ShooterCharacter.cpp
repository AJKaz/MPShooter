// Fill out your copyright notice in the Description page of Project Settings.


#include "ShooterCharacter.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/WidgetComponent.h"
#include "Net/UnrealNetwork.h"
#include "MPShooter/Weapons/Weapon.h"
#include "MPShooter/ShooterComponents/CombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "ShooterAnimInstance.h"
#include "MPShooter/MPShooter.h"
#include "MPShooter/PlayerController/ShooterPlayerController.h"
#include "MPShooter/GameMode/ShooterGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Sound/SoundCue.h"
#include "Particles/ParticleSystemComponent.h"
#include "MPShooter/PlayerState/ShooterPlayerState.h"
#include "MPShooter/Weapons/WeaponTypes.h"

AShooterCharacter::AShooterCharacter() {
	PrimaryActorTick.bCanEverTick = true;

	// Ensure character will always spawn after death (if gamemode allows for it)
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// Setup spring arm from player to camera
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetMesh());
	// Controls distance camera is from player:
	CameraBoom->TargetArmLength = 625.f;
	CameraBoom->bUsePawnControlRotation = true;

	// Attach Camera to camera arm (CameraBoom)
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Set camera to ignore collision with players
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);
	GetMesh()->SetCollisionObjectType(ECC_SkeletalMesh);
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Camera, ECollisionResponse::ECR_Ignore);	
	GetMesh()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
	
	// Make character independent of camera rotation (for now)
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;	

	OverheadWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("OverheadWidget"));
	OverheadWidget->SetupAttachment(RootComponent);

	Combat = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	Combat->SetIsReplicated(true);

	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;

	// Jumping Variables:	
	GetCharacterMovement()->AirControl = 0.88f;		// set to 1.0 for full in air control
	GetCharacterMovement()->JumpZVelocity = 1650.f;
	GetCharacterMovement()->GravityScale = 3.3f;
	GetCharacterMovement()->RotationRate.Yaw = 850.f;

	TurningInPlace = ETurningInPlace::ETIP_NotTurning;
	NetUpdateFrequency = 66.f;
	MinNetUpdateFrequency = 33.f;

	// Dissolve Effect for Elimmed
	DissolveTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("DissolveTimelineComponent"));

}

void AShooterCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AShooterCharacter, OverlappingWeapon, COND_OwnerOnly);
	DOREPLIFETIME(AShooterCharacter, Health);
	DOREPLIFETIME(AShooterCharacter, bDisableGameplay);
}

void AShooterCharacter::BeginPlay() {
	Super::BeginPlay();

	UpdateHUDHealth();
	if (HasAuthority()) {
		OnTakeAnyDamage.AddDynamic(this, &AShooterCharacter::ReceiveDamage);
	}
	ShooterPlayerController = Cast<AShooterPlayerController>(Controller);
	if (ShooterPlayerController) {
		ShooterPlayerController->HideDeathMessage();
	}
	
}

void AShooterCharacter::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

	RotateInPlace(DeltaTime);
	HideCameraIfCharacterClose();
	PollInit();
}

void AShooterCharacter::RotateInPlace(float DeltaTime) {
	if (bDisableGameplay) {
		bUseControllerRotationYaw = false;
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
		return;
	}
	if (GetLocalRole() > ENetRole::ROLE_SimulatedProxy && IsLocallyControlled()) {
		AimOffset(DeltaTime);
	}
	else {
		TimeSinceLastMovementReplication += DeltaTime;
		// ensures that simulated proxy's rotation updates every 0.2 seconds
		if (TimeSinceLastMovementReplication > 0.2f) {
			OnRep_ReplicatedMovement();
		}
		CalculateAO_Pitch();
	}
}

void AShooterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) {
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	/* Movement Bindings */
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ThisClass::Jump);
	PlayerInputComponent->BindAxis("MoveForward", this, &ThisClass::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ThisClass::MoveRight);
	PlayerInputComponent->BindAxis("Turn", this, &ThisClass::Turn);
	PlayerInputComponent->BindAxis("LookUp", this, &ThisClass::LookUp);
	PlayerInputComponent->BindAction("Crouch", IE_Pressed, this, &ThisClass::CrouchButtonPressed);
	PlayerInputComponent->BindAction("Crouch", IE_Released, this, &ThisClass::CrouchButtonReleased);

	/* Combat Bindings */
	PlayerInputComponent->BindAction("ADS", IE_Pressed, this, &ThisClass::ADSButtonPressed);
	PlayerInputComponent->BindAction("ADS", IE_Released, this, &ThisClass::ADSButtonReleased);
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &ThisClass::FireButtonPressed);
	PlayerInputComponent->BindAction("Fire", IE_Released, this, &ThisClass::FireButtonReleased);
	PlayerInputComponent->BindAction("Reload", IE_Pressed, this, &ThisClass::ReloadButtonPressed);

	/* Misc Bindings */
	PlayerInputComponent->BindAction("Interact", IE_Pressed, this, &ThisClass::InteractButtonPressed);
	//PlayerInputComponent->BindAction("Drop", IE_Pressed, this, &ThisClass::DropButtonPressed);
}

void AShooterCharacter::PostInitializeComponents() {
	Super::PostInitializeComponents();
	if (Combat) {
		Combat->Character = this;
	}
}

void AShooterCharacter::PlayFireMontage(bool bAiming) {
	if (Combat == nullptr || Combat->EquippedWeapon == nullptr) return;
	
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && FireWeaponMontage) {
		AnimInstance->Montage_Play(FireWeaponMontage);
		FName SectionName = bAiming ? FName("RifleAim") : FName("RifleHip");
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void AShooterCharacter::PlayReloadMontage() {
	if (Combat == nullptr || Combat->EquippedWeapon == nullptr) return;

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && ReloadMontage) {
		AnimInstance->Montage_Play(ReloadMontage);
		FName SectionName;
		switch (Combat->EquippedWeapon->GetWeaponType()) {
			case EWeaponType::EWT_AssaultRifle:
				SectionName = FName("Rifle");
				break;
			case EWeaponType::EWT_RocketLauncher:
				SectionName = FName("Rifle");
				break;
			case EWeaponType::EWT_Pistol:
				SectionName = FName("Rifle");
				break;
			case EWeaponType::EWT_SniperRifle:
				SectionName = FName("Rifle");
				break;
			case EWeaponType::EWT_SubmachineGun:
				SectionName = FName("Rifle");
				break;
			case EWeaponType::EWT_Shotgun:
				SectionName = FName("Rifle");
				break;
			case EWeaponType::EWT_GrenadeLauncher:
				SectionName = FName("Rifle");
				break;
			default:
				SectionName = FName("Rifle");
				break;
		}
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void AShooterCharacter::PlayElimMontage() {
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && ElimMontage) {
		AnimInstance->Montage_Play(ElimMontage);		
	}
}

void AShooterCharacter::PlayHitReactMontage() {
	if (Combat == nullptr || Combat->EquippedWeapon == nullptr) return;

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && HitReactMontage) {
		AnimInstance->Montage_Play(HitReactMontage);
		FName SectionName("FromFront");
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void AShooterCharacter::MoveForward(float Value) {
	if (bDisableGameplay) return;
	if (Controller != nullptr && Value != 0.f) {
		const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
		// Parallel vector to ground:
		const FVector Direction(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X));
		AddMovementInput(Direction, Value);
	}
}

void AShooterCharacter::MoveRight(float Value) {
	if (bDisableGameplay) return;
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
	if (bDisableGameplay) return;
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
	if (bDisableGameplay) return;
	Crouch();		
}

void AShooterCharacter::CrouchButtonReleased() {
	if (bDisableGameplay) return;
	UnCrouch();
}

void AShooterCharacter::ADSButtonPressed() {
	if (bDisableGameplay) return;
	if (Combat) {
		Combat->SetAiming(true);
	}
}

void AShooterCharacter::ADSButtonReleased() {
	if (bDisableGameplay) return;
	if (Combat) {
		Combat->SetAiming(false);
	}
}

void AShooterCharacter::Jump() {	
	if (bDisableGameplay) return;
	Super::Jump();
}

void AShooterCharacter::FireButtonPressed() {
	if (bDisableGameplay) return;
	if (Combat) {
		Combat->FireButtonPressed(true);
	}
}

void AShooterCharacter::FireButtonReleased() {
	if (bDisableGameplay) return;
	if (Combat) {
		Combat->FireButtonPressed(false);
	}
}

void AShooterCharacter::ReloadButtonPressed() {
	if (bDisableGameplay) return;
	if (Combat) {
		Combat->Reload();
	}
}

void AShooterCharacter::DropButtonPressed() {	
	if (Combat && Combat->EquippedWeapon) {
		Combat->EquippedWeapon->Dropped();
	}
}

float AShooterCharacter::CalculateSpeed() {
	FVector Velocity = GetVelocity();
	Velocity.Z = 0.f;
	return Velocity.Size();	 
}

void AShooterCharacter::AimOffset(float DeltaTime) {
	if (Combat && Combat->EquippedWeapon == nullptr) return;

	float Speed = CalculateSpeed();
	bool bIsInAir = GetCharacterMovement()->IsFalling();

	// if standing still and not jumping
	if (Speed == 0.f && !bIsInAir) {
		bRotateRootBone = true;
		FRotator CurrentAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		FRotator DeltaAimRotation = UKismetMathLibrary::NormalizedDeltaRotator(CurrentAimRotation, StartingAimRotation);
		AO_Yaw = DeltaAimRotation.Yaw;
		if (TurningInPlace == ETurningInPlace::ETIP_NotTurning) {
			InterpAO_Yaw = AO_Yaw;
		}
		bUseControllerRotationYaw = true;
		TurnInPlace(DeltaTime);
	}
	// If running or jumping:
	if (Speed > 0.f || bIsInAir) {
		bRotateRootBone = false;
		StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		AO_Yaw = 0.f;
		bUseControllerRotationYaw = true;
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
	}
	CalculateAO_Pitch();
}

void AShooterCharacter::CalculateAO_Pitch() {
	AO_Pitch = GetBaseAimRotation().Pitch;
	if (AO_Pitch > 90.f && !IsLocallyControlled()) {
		// Map pitch from range [270, 360) to [-90, 0)
		FVector2D InRange(270.f, 360.f);
		FVector2D OutRange(-90.f, 0.f);
		AO_Pitch = FMath::GetMappedRangeValueClamped(InRange, OutRange, AO_Pitch);
	}
}

void AShooterCharacter::SimProxiesTurn() {
	if (Combat == nullptr || Combat->EquippedWeapon == nullptr) return;
	
	bRotateRootBone = false;
	float Speed = CalculateSpeed();
	if (Speed > 0.f) {
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
		return;
	}

	ProxyRotationLastFrame = ProxyRotation;
	ProxyRotation = GetActorRotation();
	ProxyYaw = UKismetMathLibrary::NormalizedDeltaRotator(ProxyRotation, ProxyRotationLastFrame).Yaw;
	// If simulated proxy has turned enough, show turning animation for other clients
	if (FMath::Abs(ProxyYaw) > TurnThreshold) {
		if (ProxyYaw > TurnThreshold) {
			TurningInPlace = ETurningInPlace::ETIP_Right;
		}
		else if (ProxyYaw < -TurnThreshold) {
			TurningInPlace = ETurningInPlace::ETIP_Left;
		}
		else {
			TurningInPlace = ETurningInPlace::ETIP_NotTurning;
		}
		return;
	}
	TurningInPlace = ETurningInPlace::ETIP_NotTurning;

}

void AShooterCharacter::Elim() {
	// Drop Weapon:
	if (Combat && Combat->EquippedWeapon) {
		Combat->EquippedWeapon->Dropped();
	}

	MulticastElim();
	GetWorldTimerManager().SetTimer(ElimTimer, this, &AShooterCharacter::ElimTimerFinished, ElimDelay);
}

void AShooterCharacter::MulticastElim_Implementation() {
	if (ShooterPlayerController) {
		ShooterPlayerController->SetHUDWeaponAmmo(0);
	}
	
	bElimmed = true;
	PlayElimMontage();

	// Start Character's Dissolve Effect
	if (DissolveMaterialInstance) {
		DynamicDissolveMaterialInstance = UMaterialInstanceDynamic::Create(DissolveMaterialInstance, this);
		GetMesh()->SetMaterial(0, DynamicDissolveMaterialInstance);
		DynamicDissolveMaterialInstance->SetScalarParameterValue(TEXT("Dissolve"), 0.55f);
		DynamicDissolveMaterialInstance->SetScalarParameterValue(TEXT("Glow"), 200.f);
	}
	StartDissolve();

	// Disable character movement & shooting
	GetCharacterMovement()->DisableMovement();
	GetCharacterMovement()->StopMovementImmediately();
	bDisableGameplay = true;
	if (Combat) {
		Combat->FireButtonPressed(false);
	}

	// Disable collision
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Spawn elim bot
	if (ElimBotEffect) {
		FVector ElimBotSpawnPoint(GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z + 200.f);
		ElimBotComponent = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ElimBotEffect, ElimBotSpawnPoint, GetActorRotation());		
	}
	// Spawn elim bot sound
	if (ElimBotSound) {
		UGameplayStatics::SpawnSoundAtLocation(this, ElimBotSound, GetActorLocation());
	}

	// Hide Sniper scope widget (if there is one)
	if (IsLocallyControlled() && Combat && Combat->bAiming &&  Combat->EquippedWeapon && Combat->EquippedWeapon->GetWeaponType() == EWeaponType::EWT_SniperRifle) {
		ShowSniperScopeWidget(false);
	}
}

void AShooterCharacter::ElimTimerFinished() {
	// Respawn Character after elim timer is finished
	AShooterGameMode* ShooterGameMode = GetWorld()->GetAuthGameMode<AShooterGameMode>();
	if (ShooterGameMode) {
		ShooterGameMode->RequestRespawn(this, Controller);
	}
}

void AShooterCharacter::Destroyed() {
	Super::Destroyed();
	// Destroy elim bot
	if (ElimBotComponent) {
		ElimBotComponent->DestroyComponent();
	}
	AShooterGameMode* ShooterGameMode = Cast<AShooterGameMode>(UGameplayStatics::GetGameMode(this));
	bool bMatchNotInProgress = ShooterGameMode && ShooterGameMode->GetMatchState() != MatchState::InProgress;
	if (Combat && Combat->EquippedWeapon && bMatchNotInProgress) {
		Combat->EquippedWeapon->Destroy();
	}

}

void AShooterCharacter::ReceiveDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatorController, AActor* DamageCauser) {
	Health = FMath::Clamp(Health - Damage, 0.f, MaxHealth);
	UpdateHUDHealth();
	PlayHitReactMontage();

	// If health is 0, call game mode's player eliminated
	if (Health == 0.f) {
		AShooterGameMode* ShooterGameMode = GetWorld()->GetAuthGameMode<AShooterGameMode>();
		if (ShooterGameMode) {
			ShooterPlayerController = ShooterPlayerController == nullptr ? Cast<AShooterPlayerController>(Controller) : ShooterPlayerController;
			AShooterPlayerController* AttackerController = Cast<AShooterPlayerController>(InstigatorController);
			ShooterGameMode->PlayerEliminated(this, ShooterPlayerController, AttackerController);
		}
	}
}

void AShooterCharacter::OnRep_Health() {
	UpdateHUDHealth();
	PlayHitReactMontage();
}

void AShooterCharacter::UpdateHUDHealth() {
	ShooterPlayerController = ShooterPlayerController == nullptr ? Cast<AShooterPlayerController>(Controller) : ShooterPlayerController;
	if (ShooterPlayerController) {
		ShooterPlayerController->SetHUDHealth(Health, MaxHealth);
	}
}

/// <summary>
/// Poll any relevant classes and initialize HUD
/// </summary>
void AShooterCharacter::PollInit() {
	if (ShooterPlayerState == nullptr) {
		ShooterPlayerState = GetPlayerState<AShooterPlayerState>();
		if (ShooterPlayerState) {
			ShooterPlayerState->AddToScore(0.f);
			ShooterPlayerState->AddToDeaths(0);
		}
	}
}

void AShooterCharacter::OnRep_ReplicatedMovement() {
	Super::OnRep_ReplicatedMovement();
	SimProxiesTurn();
	TimeSinceLastMovementReplication = 0.f;

}

void AShooterCharacter::TurnInPlace(float DeltaTime) {	
	if (AO_Yaw > 55.f) {
		TurningInPlace = ETurningInPlace::ETIP_Right;
	}
	else if (AO_Yaw < -55.f) {
		TurningInPlace = ETurningInPlace::ETIP_Left;
	}
	// If we are turning
	if (TurningInPlace != ETurningInPlace::ETIP_NotTurning) {
		InterpAO_Yaw = FMath::FInterpTo(InterpAO_Yaw, 0.f, DeltaTime, 5.f);
		AO_Yaw = InterpAO_Yaw;
		if (FMath::Abs(AO_Yaw) < 15.f) {
			TurningInPlace = ETurningInPlace::ETIP_NotTurning;
			StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		}
	}
}

void AShooterCharacter::ServerInteractButtonPressed_Implementation() {
	if (Combat) {
		Combat->EquipWeapon(OverlappingWeapon);
	}
}

void AShooterCharacter::StartDissolve() {
	DissolveTrack.BindDynamic(this, &AShooterCharacter::UpdateDissolveMaterial);
	if (DissolveCurve && DissolveTimeline) {
		DissolveTimeline->AddInterpFloat(DissolveCurve, DissolveTrack);
		DissolveTimeline->Play();
	}
}

void AShooterCharacter::UpdateDissolveMaterial(float DissolveValue) {
	if (DynamicDissolveMaterialInstance) {
		DynamicDissolveMaterialInstance->SetScalarParameterValue(TEXT("Dissolve"), DissolveValue);
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

AWeapon* AShooterCharacter::GetEquippedWeapon() {
	if (Combat == nullptr) return nullptr;
	return Combat->EquippedWeapon;
}

void AShooterCharacter::OnRep_OverlappingWeapon(AWeapon* LastWeapon) {
	if (OverlappingWeapon) {
		OverlappingWeapon->ShowPickupWidget(true);
	}
	if (LastWeapon) {
		LastWeapon->ShowPickupWidget(false);
	}
}

FVector AShooterCharacter::GetHitTarget() const {
	if (Combat == nullptr) return FVector();
	return Combat->HitTarget;
}

void AShooterCharacter::HideCameraIfCharacterClose() {
	if (!IsLocallyControlled()) return;

	// If camera is squished up against player, hide character and weapon
	if ((FollowCamera->GetComponentLocation() - GetActorLocation()).Size() < CameraThreshold) {
		GetMesh()->SetVisibility(false);
		if (Combat && Combat->EquippedWeapon && Combat->EquippedWeapon->GetWeaponMesh()) {
			Combat->EquippedWeapon->GetWeaponMesh()->bOwnerNoSee = true;
		}
	}
	else {
		GetMesh()->SetVisibility(true);
		if (Combat && Combat->EquippedWeapon && Combat->EquippedWeapon->GetWeaponMesh()) {
			Combat->EquippedWeapon->GetWeaponMesh()->bOwnerNoSee = false;
		}
	}
}

ECombatState AShooterCharacter::GetCombatState() const {
	if (Combat == nullptr) return ECombatState::ECS_MAX;
	return Combat->CombatState;
}