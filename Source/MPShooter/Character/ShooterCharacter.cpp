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
#include "MPShooter/ShooterComponents/BuffComponent.h"
#include "Components/BoxComponent.h"
#include "MPShooter/ShooterComponents/LagCompensationComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "MPShooter/GameState/ShooterGameState.h"
#include "MPShooter/PlayerStart/TeamPlayerStart.h"

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

	// Construct combat component
	Combat = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	Combat->SetIsReplicated(true);

	// Construct Buff Component
	Buff = CreateDefaultSubobject<UBuffComponent>(TEXT("BuffComponent"));
	Buff->SetIsReplicated(true);

	// Construct Lag CompensationComponent
	LagCompensation = CreateDefaultSubobject<ULagCompensationComponent>(TEXT("LagCompensation"));

	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;

	// Jumping Variables:	
	GetCharacterMovement()->AirControl = 0.88f;		// set to 1.0 for full in air control
	GetCharacterMovement()->JumpZVelocity = 1600.f;
	GetCharacterMovement()->GravityScale = 3.3f;
	GetCharacterMovement()->RotationRate.Yaw = 850.f;

	TurningInPlace = ETurningInPlace::ETIP_NotTurning;
	NetUpdateFrequency = 66.f;
	MinNetUpdateFrequency = 33.f;

	// Dissolve Effect for Elimmed
	DissolveTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("DissolveTimelineComponent"));

	// Setup and Attach Grenade to socket
	AttachedGrenade = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Attached Grenade"));
	AttachedGrenade->SetupAttachment(GetMesh(), FName("GrenadeSocket"));
	AttachedGrenade->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	/**
	* Hitboxes for server-side rewind
	*/
	head = CreateDefaultSubobject<UBoxComponent>(TEXT("head"));
	head->SetupAttachment(GetMesh(), FName("head"));
	HitCollisionBoxes.Add(FName("head"), head);
	pelvis = CreateDefaultSubobject<UBoxComponent>(TEXT("pelvis"));
	pelvis->SetupAttachment(GetMesh(), FName("pelvis"));
	HitCollisionBoxes.Add(FName("pelvis"), pelvis);

	spine_02 = CreateDefaultSubobject<UBoxComponent>(TEXT("spine_02"));
	spine_02->SetupAttachment(GetMesh(), FName("spine_02"));
	HitCollisionBoxes.Add(FName("spine_02"), spine_02);
	spine_03 = CreateDefaultSubobject<UBoxComponent>(TEXT("spine_03"));
	spine_03->SetupAttachment(GetMesh(), FName("spine_03"));
	HitCollisionBoxes.Add(FName("spine_03"), spine_03);


	upperarm_l = CreateDefaultSubobject<UBoxComponent>(TEXT("upperarm_l"));
	upperarm_l->SetupAttachment(GetMesh(), FName("upperarm_l"));
	HitCollisionBoxes.Add(FName("upperarm_l"), upperarm_l);
	upperarm_r = CreateDefaultSubobject<UBoxComponent>(TEXT("upperarm_r"));
	upperarm_r->SetupAttachment(GetMesh(), FName("upperarm_r"));
	HitCollisionBoxes.Add(FName("upperarm_r"), upperarm_r);

	lowerarm_l = CreateDefaultSubobject<UBoxComponent>(TEXT("lowerarm_l"));
	lowerarm_l->SetupAttachment(GetMesh(), FName("lowerarm_l"));
	HitCollisionBoxes.Add(FName("lowerarm_l"), lowerarm_l);
	lowerarm_r = CreateDefaultSubobject<UBoxComponent>(TEXT("lowerarm_r"));
	lowerarm_r->SetupAttachment(GetMesh(), FName("lowerarm_r"));
	HitCollisionBoxes.Add(FName("lowerarm_r"), lowerarm_r);


	hand_l = CreateDefaultSubobject<UBoxComponent>(TEXT("hand_l"));
	hand_l->SetupAttachment(GetMesh(), FName("hand_l"));
	HitCollisionBoxes.Add(FName("hand_l"), hand_l);
	hand_r = CreateDefaultSubobject<UBoxComponent>(TEXT("hand_r"));
	hand_r->SetupAttachment(GetMesh(), FName("hand_r"));
	HitCollisionBoxes.Add(FName("hand_r"), hand_r);

	backpack = CreateDefaultSubobject<UBoxComponent>(TEXT("backpack"));
	backpack->SetupAttachment(GetMesh(), FName("backpack"));
	HitCollisionBoxes.Add(FName("backpack"), backpack);
	blanket = CreateDefaultSubobject<UBoxComponent>(TEXT("blanket"));
	blanket->SetupAttachment(GetMesh(), FName("backpack"));
	HitCollisionBoxes.Add(FName("blanket"), blanket);

	thigh_l = CreateDefaultSubobject<UBoxComponent>(TEXT("thigh_l"));
	thigh_l->SetupAttachment(GetMesh(), FName("thigh_l"));
	HitCollisionBoxes.Add(FName("thigh_l"), thigh_l);
	thigh_r = CreateDefaultSubobject<UBoxComponent>(TEXT("thigh_r"));
	thigh_r->SetupAttachment(GetMesh(), FName("thigh_r"));
	HitCollisionBoxes.Add(FName("thigh_r"), thigh_r);

	calf_l = CreateDefaultSubobject<UBoxComponent>(TEXT("calf_l"));
	calf_l->SetupAttachment(GetMesh(), FName("calf_l"));
	HitCollisionBoxes.Add(FName("calf_l"), calf_l);
	calf_r = CreateDefaultSubobject<UBoxComponent>(TEXT("calf_r"));
	calf_r->SetupAttachment(GetMesh(), FName("calf_r"));
	HitCollisionBoxes.Add(FName("calf_r"), calf_r);

	foot_l = CreateDefaultSubobject<UBoxComponent>(TEXT("foot_l"));
	foot_l->SetupAttachment(GetMesh(), FName("foot_l"));
	HitCollisionBoxes.Add(FName("foot_l"), foot_l);
	foot_r = CreateDefaultSubobject<UBoxComponent>(TEXT("foot_r"));
	foot_r->SetupAttachment(GetMesh(), FName("foot_r"));
	HitCollisionBoxes.Add(FName("foot_r"), foot_r);

	// Set collision responses and type
	for (auto Box : HitCollisionBoxes) {
		if (Box.Value) {
			Box.Value->SetCollisionObjectType(ECC_HitBox);
			Box.Value->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
			Box.Value->SetCollisionResponseToChannel(ECC_HitBox, ECollisionResponse::ECR_Block);
			Box.Value->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

}

void AShooterCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AShooterCharacter, OverlappingWeapon, COND_OwnerOnly);
	DOREPLIFETIME(AShooterCharacter, Health);
	DOREPLIFETIME(AShooterCharacter, Shield);
	DOREPLIFETIME(AShooterCharacter, bDisableGameplay);
}

void AShooterCharacter::BeginPlay() {
	Super::BeginPlay();

	SpawnDefaultWeapon();
	UpdateHUDAmmo();

	UpdateHUDHealth();
	UpdateHUDShield();

	if (HasAuthority()) {
		OnTakeAnyDamage.AddDynamic(this, &AShooterCharacter::ReceiveDamage);
	}
	ShooterPlayerController = Cast<AShooterPlayerController>(Controller);
	if (ShooterPlayerController) {
		ShooterPlayerController->HideDeathMessage();
	}
	if (AttachedGrenade) {
		AttachedGrenade->SetVisibility(false);
	}	
}

void AShooterCharacter::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

	RotateInPlace(DeltaTime);
	HideCameraIfCharacterClose();
	PollInit();
}

void AShooterCharacter::RotateInPlace(float DeltaTime) {
	if (Combat && Combat->bHoldingFlag) {
		bUseControllerRotationYaw = false;
		GetCharacterMovement()->bOrientRotationToMovement = true;
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
		return;
	}
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
	PlayerInputComponent->BindAction("ThrowGrenade", IE_Pressed, this, &AShooterCharacter::GrenadeButtonPressed);
	PlayerInputComponent->BindAction("SwapWeapon", IE_Pressed, this, &ThisClass::SwapWeaponButtonPressed);

	/* Misc Bindings */
	PlayerInputComponent->BindAction("Interact", IE_Pressed, this, &ThisClass::InteractButtonPressed);
	//PlayerInputComponent->BindAction("Drop", IE_Pressed, this, &ThisClass::DropButtonPressed);
}

void AShooterCharacter::PostInitializeComponents() {
	Super::PostInitializeComponents();
	if (Combat) {
		Combat->Character = this;
	}
	// Setup Buff Component
	if (Buff) {
		// Setup variables for buffs
		Buff->Character = this;
		Buff->SetInitialSpeed(GetCharacterMovement()->MaxWalkSpeed, GetCharacterMovement()->MaxWalkSpeedCrouched);
		Buff->SetInitialJumpVelocity(GetCharacterMovement()->JumpZVelocity);
	}
	// Setup Lag Compensation Component
	if (LagCompensation) {
		LagCompensation->Character = this;
		if (Controller) {
			LagCompensation->Controller = Cast<AShooterPlayerController>(Controller);
		}
	}
}

void AShooterCharacter::MulticastGainedTheLead_Implementation() {
	if (CrownSystem == nullptr) return;
	if (CrownComponent == nullptr) {
		// spawn crown component
		CrownComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			CrownSystem,
			GetMesh(),
			FName(),
			GetActorLocation() + FVector(0.f, 0.f, 110.f),
			GetActorRotation(),
			EAttachLocation::KeepWorldPosition,
			false);
	}
	if (CrownComponent) {
		// activate crown component
		CrownComponent->Activate();
	}
}

void AShooterCharacter::MulticastLostTheLead_Implementation() {
	// Destroy crown
	if (CrownComponent) {
		CrownComponent->DestroyComponent();
	}
}

void AShooterCharacter::SetTeamColor(ETeam Team) {
	if (GetMesh() == nullptr || OriginalMaterial == nullptr) return;
	switch (Team) {
	case ETeam::ET_NoTeam:
		GetMesh()->SetMaterial(0, OriginalMaterial);
		DissolveMaterialInstance = BlueDissolveMatInst;
		break;
	case ETeam::ET_RedTeam:
		GetMesh()->SetMaterial(0, RedMaterial);
		DissolveMaterialInstance = RedDissolveMatInst;
		break;
	case ETeam::ET_BlueTeam:
		GetMesh()->SetMaterial(0, BlueMaterial);
		DissolveMaterialInstance = BlueDissolveMatInst;
		break;
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
			SectionName = FName("RocketLauncher");
			break;
		case EWeaponType::EWT_Pistol:
			SectionName = FName("Pistol");
			break;
		case EWeaponType::EWT_SniperRifle:
			SectionName = FName("SniperRifle");
			break;
		case EWeaponType::EWT_SubmachineGun:
			SectionName = FName("Pistol");
			break;
		case EWeaponType::EWT_Shotgun:
			SectionName = FName("Shotgun");
			break;
		case EWeaponType::EWT_GrenadeLauncher:
			SectionName = FName("GrenadeLauncher");
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

void AShooterCharacter::PlayThrowGrenadeMontage() {
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && ThrowGrenadeMontage) {
		AnimInstance->Montage_Play(ThrowGrenadeMontage);
	}
}

void AShooterCharacter::PlaySwapMontage() {
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && SwapMontage) {
		AnimInstance->Montage_Play(SwapMontage);
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
	if (Combat && !Combat->bHoldingFlag) {		
		ServerInteractButtonPressed();
	}
}

void AShooterCharacter::ServerInteractButtonPressed_Implementation() {
	if (Combat && OverlappingWeapon) {
		Combat->EquipWeapon(OverlappingWeapon);
	}
}

void AShooterCharacter::CrouchButtonPressed() {
	if (bDisableGameplay) return;
	if (Combat && Combat->bHoldingFlag) return;
	Crouch();
}

void AShooterCharacter::CrouchButtonReleased() {
	if (bDisableGameplay) return;
	if (Combat && Combat->bHoldingFlag) return;
	UnCrouch();
}

void AShooterCharacter::ADSButtonPressed() {
	if (bDisableGameplay) return;
	if (Combat && !Combat->bHoldingFlag) {
		Combat->SetAiming(true);
	}
}

void AShooterCharacter::ADSButtonReleased() {
	if (bDisableGameplay) return;
	if (Combat && !Combat->bHoldingFlag) {
		Combat->SetAiming(false);
	}
}

void AShooterCharacter::Jump() {
	if (bDisableGameplay) return;
	Super::Jump();
}

void AShooterCharacter::FireButtonPressed() {
	if (bDisableGameplay) return;
	if (Combat && !Combat->bHoldingFlag) {
		Combat->FireButtonPressed(true);
	}
}

void AShooterCharacter::FireButtonReleased() {
	if (bDisableGameplay) return;
	if (Combat && !Combat->bHoldingFlag) {
		Combat->FireButtonPressed(false);
	}
}

void AShooterCharacter::ReloadButtonPressed() {
	if (bDisableGameplay) return;
	if (Combat && !Combat->bHoldingFlag) {
		Combat->Reload();
	}
}

void AShooterCharacter::DropButtonPressed() {
	if (bDisableGameplay) return;
	if (Combat && Combat->EquippedWeapon) {
		Combat->EquippedWeapon->Dropped();
	}
}

void AShooterCharacter::GrenadeButtonPressed() {
	if (bDisableGameplay) return;
	if (Combat && !Combat->bHoldingFlag) {
		Combat->ThrowGrenade();
	}
}

void AShooterCharacter::SwapWeaponButtonPressed() {
	if (bDisableGameplay) return;
	if (Combat && Combat->ShouldSwapWeapons() && Combat->CombatState == ECombatState::ECS_Unoccupied && !Combat->bHoldingFlag) {
		ServerSwapWeaponButtonPressed();
		bFinishedSwapping = false;
		if (!HasAuthority()) {
			PlaySwapMontage();
			Combat->CombatState = ECombatState::ECS_SwappingWeapons;
		}
	}
}

void AShooterCharacter::ServerSwapWeaponButtonPressed_Implementation() {
	if (Combat) {
		Combat->SwapWeapons();
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

void AShooterCharacter::DropOrDestroyWeapon(AWeapon* Weapon) {
	if (Weapon == nullptr) return;
	if (Weapon->bDestroyWeapon) {
		// Destroy weapon
		Weapon->Destroy();
	}
	else {
		// Drop Weapon
		Weapon->Dropped();
	}
}

void AShooterCharacter::DropOrDestroyWeapons() {
	// Drop Weapon, or destroy if it's default starting weapon
	if (Combat) {
		if (Combat->EquippedWeapon) DropOrDestroyWeapon(Combat->EquippedWeapon);
		if (Combat->SecondaryWeapon) DropOrDestroyWeapon(Combat->SecondaryWeapon);
		if (Combat->TheFlag) Combat->TheFlag->Dropped();
	}
}

void AShooterCharacter::Elim(bool bPLayerLeftGame) {
	DropOrDestroyWeapons();
	MulticastElim(bPLayerLeftGame);
}

void AShooterCharacter::MulticastElim_Implementation(bool bPLayerLeftGame) {
	bLeftGame = bPLayerLeftGame;
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
	if (IsLocallyControlled() && Combat && Combat->bAiming && Combat->EquippedWeapon && Combat->EquippedWeapon->GetWeaponType() == EWeaponType::EWT_SniperRifle) {
		ShowSniperScopeWidget(false);
	}

	// remove crown (if player has it)
	if (CrownComponent) {
		CrownComponent->DestroyComponent();
	}

	GetWorldTimerManager().SetTimer(ElimTimer, this, &AShooterCharacter::ElimTimerFinished, ElimDelay);
}

void AShooterCharacter::ElimTimerFinished() {
	// Respawn Character after elim timer is finished
	ShooterGameMode = ShooterGameMode == nullptr ? GetWorld()->GetAuthGameMode<AShooterGameMode>() : ShooterGameMode;
	if (ShooterGameMode && !bLeftGame) {
		ShooterGameMode->RequestRespawn(this, Controller);
	}
	if (bLeftGame && IsLocallyControlled()) {
		// left game, broadcast delegate to disconnect
		OnLeftGame.Broadcast();
	}
}

void AShooterCharacter::ServerLeaveGame_Implementation() {
	ShooterGameMode = ShooterGameMode == nullptr ? GetWorld()->GetAuthGameMode<AShooterGameMode>() : ShooterGameMode;
	ShooterPlayerState = ShooterPlayerState == nullptr ? GetPlayerState<AShooterPlayerState>() : ShooterPlayerState;
	if (ShooterGameMode && ShooterPlayerState) {
		ShooterGameMode->PlayerLeftGame(ShooterPlayerState);
	}
}

void AShooterCharacter::Destroyed() {
	Super::Destroyed();
	// Destroy elim bot
	if (ElimBotComponent) {
		ElimBotComponent->DestroyComponent();
	}
	ShooterGameMode = ShooterGameMode == nullptr ? GetWorld()->GetAuthGameMode<AShooterGameMode>() : ShooterGameMode;
	bool bMatchNotInProgress = ShooterGameMode && ShooterGameMode->GetMatchState() != MatchState::InProgress;
	if (Combat && Combat->EquippedWeapon && bMatchNotInProgress) {
		Combat->EquippedWeapon->Destroy();
	}

}

void AShooterCharacter::ReceiveDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatorController, AActor* DamageCauser) {
	ShooterGameMode = ShooterGameMode == nullptr ? GetWorld()->GetAuthGameMode<AShooterGameMode>() : ShooterGameMode;
	if (bElimmed || ShooterGameMode == nullptr) return;
	Damage = ShooterGameMode->CalculateDamage(InstigatorController, Controller, Damage);

	// Calculate how much damage to deal to health and shields
	float DamageToHealth = Damage;
	if (Shield > 0.f) {
		if (Shield >= Damage) {
			// Shield absorbs all of the damage, damage shield
			Shield = FMath::Clamp(Shield - Damage, 0.f, MaxShield);
			DamageToHealth = 0.f;
		}
		else {
			// Shield absorbs SOME damage, but not all	
			DamageToHealth = FMath::Clamp(DamageToHealth - Shield, 0.f, Damage);
			Shield = 0.f;
		}
	}

	Health = FMath::Clamp(Health - DamageToHealth, 0.f, MaxHealth);

	UpdateHUDHealth();
	UpdateHUDShield();
	PlayHitReactMontage();

	// If health is 0, call game mode's player eliminated (player is dead)
	if (Health == 0.f) {
		if (ShooterGameMode) {
			ShooterPlayerController = ShooterPlayerController == nullptr ? Cast<AShooterPlayerController>(Controller) : ShooterPlayerController;
			AShooterPlayerController* AttackerController = Cast<AShooterPlayerController>(InstigatorController);
			ShooterGameMode->PlayerEliminated(this, ShooterPlayerController, AttackerController);
		}
	}
}

void AShooterCharacter::OnRep_Health(float LastHealth) {
	UpdateHUDHealth();
	// If character was damaged, play HitReactMontage
	if (Health < LastHealth) PlayHitReactMontage();
}

void AShooterCharacter::OnRep_Shield(float LastShield) {
	UpdateHUDShield();
	// If character was damaged, play HitReactMontage
	if (Shield < LastShield) PlayHitReactMontage();
}

void AShooterCharacter::UpdateHUDHealth() {
	ShooterPlayerController = ShooterPlayerController == nullptr ? Cast<AShooterPlayerController>(Controller) : ShooterPlayerController;
	if (ShooterPlayerController) {
		ShooterPlayerController->SetHUDHealth(Health, MaxHealth);
	}
}

void AShooterCharacter::UpdateHUDShield() {
	ShooterPlayerController = ShooterPlayerController == nullptr ? Cast<AShooterPlayerController>(Controller) : ShooterPlayerController;
	if (ShooterPlayerController) {
		ShooterPlayerController->SetHUDShield(Shield, MaxShield);
	}
}

void AShooterCharacter::UpdateHUDAmmo() {
	ShooterPlayerController = ShooterPlayerController == nullptr ? Cast<AShooterPlayerController>(Controller) : ShooterPlayerController;
	if (ShooterPlayerController && Combat && Combat->EquippedWeapon) {
		ShooterPlayerController->SetHUDCarriedAmmo(Combat->CarriedAmmo);
		ShooterPlayerController->SetHUDWeaponAmmo(Combat->EquippedWeapon->GetAmmo());
	}
}

void AShooterCharacter::PollInit() {
	// Poll any relevant classes and initialize HUD
	if (ShooterPlayerState == nullptr) {
		ShooterPlayerState = GetPlayerState<AShooterPlayerState>();
		if (ShooterPlayerState) {
			OnPlayerStateInitialzed();
			AShooterGameState* ShooterGameState = Cast<AShooterGameState>(UGameplayStatics::GetGameState(this));
			if (ShooterGameState && ShooterGameState->TopScoringPlayers.Contains(ShooterPlayerState)) {
				MulticastGainedTheLead();
			}
		}
	}
}

void AShooterCharacter::OnPlayerStateInitialzed() {
	ShooterPlayerState->AddToScore(0.f);
	ShooterPlayerState->AddToDeaths(0);
	SetTeamColor(ShooterPlayerState->GetTeam());
	SetSpawnPoint();
}

void AShooterCharacter::SetSpawnPoint() {
	if (HasAuthority() && ShooterPlayerState && ShooterPlayerState->GetTeam() != ETeam::ET_NoTeam) {
		TArray<AActor*> PlayerStarts;
		UGameplayStatics::GetAllActorsOfClass(this, ATeamPlayerStart::StaticClass(), PlayerStarts);
		TArray<ATeamPlayerStart*> TeamPlayerStarts;
		// find all player starts on player's team
		for (auto Start : PlayerStarts) {
			ATeamPlayerStart* TeamStart = Cast<ATeamPlayerStart>(Start);
			if (TeamStart && TeamStart->Team == ShooterPlayerState->GetTeam()) {
				TeamPlayerStarts.Add(TeamStart);
			}
		}
		// choose a random start
		if (TeamPlayerStarts.Num() > 0) {
			ATeamPlayerStart* ChosenPlayerStart = TeamPlayerStarts[FMath::RandRange(0, TeamPlayerStarts.Num() - 1)];
			SetActorLocationAndRotation(ChosenPlayerStart->GetActorLocation(), ChosenPlayerStart->GetActorRotation());
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
		ShowLocalMesh(false);
		// hide primary weapon
		if (Combat && Combat->EquippedWeapon && Combat->EquippedWeapon->GetWeaponMesh()) {
			Combat->EquippedWeapon->GetWeaponMesh()->bOwnerNoSee = true;
		}
		// hide secondary weapon
		if (Combat && Combat->SecondaryWeapon && Combat->SecondaryWeapon->GetWeaponMesh()) {
			Combat->SecondaryWeapon->GetWeaponMesh()->bOwnerNoSee = true;
		}
	}
	else {
		if (!bSniperAiming) {
			ShowLocalMesh(true);
			if (Combat && Combat->EquippedWeapon && Combat->EquippedWeapon->GetWeaponMesh()) {
				Combat->EquippedWeapon->GetWeaponMesh()->bOwnerNoSee = false;
			}
			if (Combat && Combat->SecondaryWeapon && Combat->SecondaryWeapon->GetWeaponMesh()) {
				Combat->SecondaryWeapon->GetWeaponMesh()->bOwnerNoSee = false;
			}
		}

	}
}

void AShooterCharacter::ShowLocalMesh(bool bShow) {
	GetMesh()->SetVisibility(bShow);
}

ECombatState AShooterCharacter::GetCombatState() const {
	if (Combat == nullptr) return ECombatState::ECS_MAX;
	return Combat->CombatState;
}

void AShooterCharacter::SpawnDefaultWeapon() {
	ShooterGameMode = ShooterGameMode == nullptr ? GetWorld()->GetAuthGameMode<AShooterGameMode>() : ShooterGameMode;
	UWorld* World = GetWorld();
	if (ShooterGameMode && World && !bElimmed && DefaultWeaponClass) {
		// Inside this if statement IF using shootergamemode & on server
		// Spawn default weapon
		AWeapon* StartingWeapon = World->SpawnActor<AWeapon>(DefaultWeaponClass);
		StartingWeapon->bDestroyWeapon = true;
		// Equip Weapon
		if (Combat) {
			Combat->EquipWeapon(StartingWeapon);
		}
	}
}

bool AShooterCharacter::IsLocallyReloading() {
	if (Combat == nullptr) return false;
	return Combat->bLocallyReloading;
}

bool AShooterCharacter::IsHoldingFlag() const {
	if (Combat == nullptr) return false;
	return Combat->bHoldingFlag;
}

ETeam AShooterCharacter::GetTeam() {
	ShooterPlayerState = ShooterPlayerState == nullptr ? GetPlayerState<AShooterPlayerState>() : ShooterPlayerState;
	if (ShooterPlayerState == nullptr) return ETeam::ET_NoTeam;
	return ShooterPlayerState->GetTeam();	
}
