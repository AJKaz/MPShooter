// Fill out your copyright notice in the Description page of Project Settings.


#include "CombatComponent.h"
#include "MPShooter/Weapons/Weapon.h"
#include "MPShooter/Character/ShooterCharacter.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Components/SphereComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "MPShooter/PlayerController/ShooterPlayerController.h"
#include "Camera/CameraComponent.h"
#include "TimerManager.h"
#include "Sound/SoundCue.h"
#include "MPShooter/Weapons/Projectile.h"
#include "MPShooter/Character/ShooterAnimInstance.h"
#include "MPShooter/Weapons/Shotgun.h"

UCombatComponent::UCombatComponent() {
	PrimaryComponentTick.bCanEverTick = true;

	BaseWalkSpeed = 1000.f;
	AimWalkSpeed = 700.f;
	//SetIsReplicatedByDefault(true);
}

void UCombatComponent::BeginPlay() {
	Super::BeginPlay();
	if (Character) {
		Character->GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;
		if (Character->GetFollowCamera()) {
			DefaultFOV = Character->GetFollowCamera()->FieldOfView;
			CurrentFOV = DefaultFOV;
		}
		if (Character->HasAuthority()) {
			InitializeCarriedAmmo();
		}
	}
}

void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Character && Character->IsLocallyControlled()) {
		FHitResult HitResult;
		TraceUnderCrosshairs(HitResult);
		HitTarget = HitResult.ImpactPoint;

		// Change FOV
		InterpFOV(DeltaTime);

		// Display Crosshairs
		SetHUDCrosshairs(DeltaTime);
	}
}

void UCombatComponent::SetHUDCrosshairs(float DeltaTime) {
	if (Character == nullptr || Character->Controller == nullptr) return;

	// If controller is null, set it to player's controller
	Controller = Controller == nullptr ? Cast<AShooterPlayerController>(Character->Controller) : Controller;
	if (Controller) {
		// If HUD is null, set it to controller's HUD
		HUD = HUD == nullptr ? Cast<AShooterHUD>(Controller->GetHUD()) : HUD;
		if (HUD) {
			// Sets HUD crosshair texture from weapon crosshairs texture

			if (EquippedWeapon) {
				HUDPackage.CrosshairsCenter = EquippedWeapon->CrosshairsCenter;
				HUDPackage.CrosshairsLeft = EquippedWeapon->CrosshairsLeft;
				HUDPackage.CrosshairsRight = EquippedWeapon->CrosshairsRight;
				HUDPackage.CrosshairsBottom = EquippedWeapon->CrosshairsBottom;
				HUDPackage.CrosshairsTop = EquippedWeapon->CrosshairsTop;
			}
			else {
				HUDPackage.CrosshairsCenter = nullptr;
				HUDPackage.CrosshairsLeft = nullptr;
				HUDPackage.CrosshairsRight = nullptr;
				HUDPackage.CrosshairsBottom = nullptr;
				HUDPackage.CrosshairsTop = nullptr;
			}
			// Calculate Crosshair Spread for movement error:
			// Map [0, maxSpeed] -> [0,1]
			FVector2D WalkSpeedRange(0.f, Character->GetCharacterMovement()->MaxWalkSpeed);
			FVector2D VelocityMultiplayerRange(0.f, 1.f);
			FVector Velocity = Character->GetVelocity();
			Velocity.Z = 0.f;
			CrosshairsVelocityFactor = FMath::GetMappedRangeValueClamped(WalkSpeedRange, VelocityMultiplayerRange, Velocity.Size());

			// Make jumping error slowly expand crosshair:
			if (Character->GetCharacterMovement()->IsFalling()) {
				CrosshairsInAirFactor = FMath::FInterpTo(CrosshairsInAirFactor, 2.f, DeltaTime, 2.f);
			}
			else {
				CrosshairsInAirFactor = FMath::FInterpTo(CrosshairsInAirFactor, 0.f, DeltaTime, 30.f);
			}

			// Shrink crosshairs on aiming
			if (bAiming) {
				CrosshairsAimFactor = FMath::FInterpTo(CrosshairsAimFactor, 0.58f, DeltaTime, 30.f);
			}
			else {
				CrosshairsAimFactor = FMath::FInterpTo(CrosshairsAimFactor, 0.f, DeltaTime, 30.f);
			}

			// Add firing error to crosshairs (more in FireButtonPressed()
			CrosshairShootingFactor = FMath::FInterpTo(CrosshairShootingFactor, 0.f, DeltaTime, 40.f);

			HUDPackage.CrosshairSpread =
				0.5f +
				CrosshairsVelocityFactor +
				CrosshairsInAirFactor -
				CrosshairsAimFactor +
				CrosshairShootingFactor;

			HUD->SetHUDPackage(HUDPackage);
		}
	}
}

void UCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCombatComponent, EquippedWeapon);
	DOREPLIFETIME(UCombatComponent, SecondaryWeapon);
	DOREPLIFETIME(UCombatComponent, bAiming);
	DOREPLIFETIME_CONDITION(UCombatComponent, CarriedAmmo, COND_OwnerOnly);
	DOREPLIFETIME(UCombatComponent, CombatState);
	DOREPLIFETIME(UCombatComponent, Grenades);
}

void UCombatComponent::InterpFOV(float DeltaTime) {
	if (EquippedWeapon == nullptr) return;

	if (bAiming) {
		// Interpolate FOV to weapon's zoomed FOV
		CurrentFOV = FMath::FInterpTo(CurrentFOV, EquippedWeapon->GetZoomedFOV(), DeltaTime, EquippedWeapon->GetZoomInterpSpeed());
	}
	else {
		CurrentFOV = FMath::FInterpTo(CurrentFOV, DefaultFOV, DeltaTime, ZoomInterpSpeed);
	}
	// If character is valid, change FOV
	if (Character && Character->GetFollowCamera()) {
		Character->GetFollowCamera()->SetFieldOfView(CurrentFOV);
	}

}

void UCombatComponent::FireButtonPressed(bool bPressed) {
	bFireButtonPressed = bPressed;
	if (bFireButtonPressed) {
		Fire();
	}
}

void UCombatComponent::ShotgunShellReload() {
	if (Character && Character->HasAuthority()) {
		UpdateShotgunAmmoValues();
	}
}


void UCombatComponent::Fire() {
	if (CanFire()) {
		bCanFire = false;
		if (EquippedWeapon) {
			CrosshairShootingFactor = 0.75f;
			switch (EquippedWeapon->FireType) {
			case EFireType::EFT_HitScan:
				FireHitScanWeapon();
				break;
			case EFireType::EFT_Projectile:
				FireProjectileWeapon();
				break;
			case EFireType::EFT_Shotgun:
				FireShotgun();
				break;
			}
		}
		StartFireTimer();
	}
}

void UCombatComponent::FireProjectileWeapon() {
	if (EquippedWeapon && Character) {
		HitTarget = EquippedWeapon->bUseScatter ? EquippedWeapon->TraceEndWithScatter(HitTarget) : HitTarget;
		if (!Character->HasAuthority()) LocalFire(HitTarget);
		ServerFire(HitTarget, EquippedWeapon->FireDelay);
	}
}

void UCombatComponent::FireHitScanWeapon() {
	if (EquippedWeapon && Character) {
		HitTarget = EquippedWeapon->bUseScatter ? EquippedWeapon->TraceEndWithScatter(HitTarget) : HitTarget;
		if (!Character->HasAuthority()) LocalFire(HitTarget);
		ServerFire(HitTarget, EquippedWeapon->FireDelay);
	}
}

void UCombatComponent::FireShotgun() {
	AShotgun* Shotgun = Cast<AShotgun>(EquippedWeapon);
	if (Shotgun && Character) {
		TArray<FVector_NetQuantize> HitTargets;
		Shotgun->ShotgunTraceEndWithScatter(HitTarget, HitTargets);
		if (!Character->HasAuthority()) LocalShotgunFire(HitTargets);
		ServerShotgunFire(HitTargets, EquippedWeapon->FireDelay);
	}
}

bool UCombatComponent::CanFire() {
	if (EquippedWeapon == nullptr) return false;
	// IF character is reloading a shotgun, let them shoot after each pellet reloads (if they click)
	if (!EquippedWeapon->IsEmpty() && bCanFire && CombatState == ECombatState::ECS_Reloading && EquippedWeapon->GetWeaponType() == EWeaponType::EWT_Shotgun) return true;
	if (bLocallyReloading) return false;
	return !EquippedWeapon->IsEmpty() && bCanFire && CombatState == ECombatState::ECS_Unoccupied;
}

void UCombatComponent::PickupAmmo(EWeaponType WeaponType, int32 AmmoAmount) {
	// Ensure WeaponType key exists, then add ammo to that weapon's carried ammo amount
	if (CarriedAmmoMap.Contains(WeaponType)) {
		// Clamp to ensure you cant get more than max carried ammo (currently 999)
		CarriedAmmoMap[WeaponType] = FMath::Clamp(CarriedAmmoMap[WeaponType] + AmmoAmount, 0, MaxCarriedAmmo);
		UpdateCarriedAmmo();
	}

	// Check if weapon is empty & just picked up ammo for that weapon
	// If so, reload weapon
	if (EquippedWeapon && EquippedWeapon->IsEmpty() && EquippedWeapon->GetWeaponType() == WeaponType) {
		Reload();
	}
}


void UCombatComponent::StartFireTimer() {
	if (EquippedWeapon == nullptr || Character == nullptr) return;

	Character->GetWorldTimerManager().SetTimer(
		FireTimer,
		this,
		&UCombatComponent::FireTimerFinished,
		EquippedWeapon->FireDelay
	);
}

void UCombatComponent::FireTimerFinished() {
	if (EquippedWeapon == nullptr) return;
	bCanFire = true;
	if (bFireButtonPressed && EquippedWeapon->bAutomatic) {
		Fire();
	}
	ReloadEmptyWeapon();
}

void UCombatComponent::ServerFire_Implementation(const FVector_NetQuantize& TraceHitTarget, float FireDelay) {
	MulticastFire(TraceHitTarget);
}

bool UCombatComponent::ServerFire_Validate(const FVector_NetQuantize& TraceHitTarget, float FireDelay) {
	if (EquippedWeapon) {
		bool bNearlyEqual = FMath::IsNearlyEqual(EquippedWeapon->FireDelay, FireDelay, 0.001f);
		return bNearlyEqual;
	}
	return true;
}

void UCombatComponent::MulticastFire_Implementation(const FVector_NetQuantize& TraceHitTarget) {
	// Return IF on server or on client not controlling this character
	if (Character && Character->IsLocallyControlled() && !Character->HasAuthority()) return;
	LocalFire(TraceHitTarget);
}

void UCombatComponent::ServerShotgunFire_Implementation(const TArray<FVector_NetQuantize>& TraceHitTargets, float FireDelay) {
	MulticastShotgunFire(TraceHitTargets);
}

bool UCombatComponent::ServerShotgunFire_Validate(const TArray<FVector_NetQuantize>& TraceHitTargets, float FireDelay) {
	if (EquippedWeapon) {
		bool bNearlyEqual = FMath::IsNearlyEqual(EquippedWeapon->FireDelay, FireDelay, 0.001f);
		return bNearlyEqual;
	}
	return true;
}

void UCombatComponent::MulticastShotgunFire_Implementation(const TArray<FVector_NetQuantize>& TraceHitTargets) {
	if (Character && Character->IsLocallyControlled() && !Character->HasAuthority()) return;
	LocalShotgunFire(TraceHitTargets);
}

void UCombatComponent::LocalFire(const FVector_NetQuantize& TraceHitTarget) {
	if (EquippedWeapon == nullptr) return;

	if (Character && CombatState == ECombatState::ECS_Unoccupied) {
		Character->PlayFireMontage(bAiming);
		EquippedWeapon->Fire(TraceHitTarget);
	}
}

void UCombatComponent::LocalShotgunFire(const TArray<FVector_NetQuantize>& TraceHitTargets) {
	AShotgun* Shotgun = Cast<AShotgun>(EquippedWeapon);
	if (Shotgun == nullptr || Character == nullptr) return;

	if (CombatState == ECombatState::ECS_Reloading || CombatState == ECombatState::ECS_Unoccupied) {
		Character->PlayFireMontage(bAiming);
		Shotgun->FireShotgun(TraceHitTargets);
		CombatState = ECombatState::ECS_Unoccupied;
		bLocallyReloading = false;
	}

}

void UCombatComponent::EquipWeapon(AWeapon* WeaponToEquip) {
	if (Character == nullptr || WeaponToEquip == nullptr) return;
	if (CombatState != ECombatState::ECS_Unoccupied) return;

	if (EquippedWeapon != nullptr && SecondaryWeapon == nullptr) {
		// Have primary weapon, but not secondary weapon
		// Equip this weapon as secondary
		EquipSecondaryWeapon(WeaponToEquip);
	}
	else {
		EquipPrimaryWeapon(WeaponToEquip);
	}

	Character->GetCharacterMovement()->bOrientRotationToMovement = false;
	Character->bUseControllerRotationYaw = true;
}

void UCombatComponent::FinishSwap() {
	if (Character) {
		if (Character->HasAuthority()) CombatState = ECombatState::ECS_Unoccupied;
		Character->bFinishedSwapping = true;
	}
}

void UCombatComponent::FinishSwapAttachedWeapons() {
	// Attach primary weapon
	EquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped);
	AttachActorToRightHand(EquippedWeapon);

	// Update HUD
	EquippedWeapon->SetHUDAmmo();
	UpdateCarriedAmmo();
	PlayEquipWeaponSound(EquippedWeapon);

	// Attach secondary weapon
	SecondaryWeapon->SetWeaponState(EWeaponState::EWS_EquippedSecondary);
	AttachActorToBackpack(SecondaryWeapon);
}

void UCombatComponent::SwapWeapons() {
	if (CombatState != ECombatState::ECS_Unoccupied || Character == nullptr) return;

	Character->PlaySwapMontage();
	CombatState = ECombatState::ECS_SwappingWeapons;

	// Swaps primary and secondary weapon
	AWeapon* TempWeapon = EquippedWeapon;
	EquippedWeapon = SecondaryWeapon;
	SecondaryWeapon = TempWeapon;	
}

void UCombatComponent::EquipPrimaryWeapon(AWeapon* WeaponToEquip) {
	if (WeaponToEquip == nullptr) return;

	// IF Character has a weapon equipped, drop it
	DropEquippedWeapon();

	EquippedWeapon = WeaponToEquip;
	EquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped);

	// Attach Weapon to Character's RightHandSockete
	AttachActorToRightHand(EquippedWeapon);

	// Make character owner of weapon:
	EquippedWeapon->SetOwner(Character);
	EquippedWeapon->SetHUDAmmo();

	UpdateCarriedAmmo();
	PlayEquipWeaponSound(WeaponToEquip);
	ReloadEmptyWeapon();
}

void UCombatComponent::EquipSecondaryWeapon(AWeapon* WeaponToEquip) {
	if (WeaponToEquip == nullptr) return;
	SecondaryWeapon = WeaponToEquip;
	SecondaryWeapon->SetWeaponState(EWeaponState::EWS_EquippedSecondary);

	// Attaches weapon to character's backpack
	AttachActorToBackpack(WeaponToEquip);
	PlayEquipWeaponSound(WeaponToEquip);
	
	SecondaryWeapon->SetOwner(Character);		
}

void UCombatComponent::OnRep_EquippedWeapon() {
	if (EquippedWeapon && Character) {
		EquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped);

		AttachActorToRightHand(EquippedWeapon);

		// Lock camera free rotation
		Character->GetCharacterMovement()->bOrientRotationToMovement = false;
		Character->bUseControllerRotationYaw = true;
		
		PlayEquipWeaponSound(EquippedWeapon);

		// Update HUD Ammo
		EquippedWeapon->SetHUDAmmo();
	}
}

void UCombatComponent::OnRep_SecondaryWeapon() {
	if (SecondaryWeapon && Character) {
		SecondaryWeapon->SetWeaponState(EWeaponState::EWS_EquippedSecondary);
		AttachActorToBackpack(SecondaryWeapon);
		PlayEquipWeaponSound(SecondaryWeapon);
	}
}


void UCombatComponent::DropEquippedWeapon() {
	// If we have a weapon equipped, drop it
	if (EquippedWeapon) {
		EquippedWeapon->Dropped();
	}
}

void UCombatComponent::AttachActorToRightHand(AActor* ActorToAttach) {
	if (Character == nullptr || Character->GetMesh() == nullptr || ActorToAttach == nullptr) return;
	// Get Character's socket called "RightHandSocket"
	const USkeletalMeshSocket* HandSocket = Character->GetMesh()->GetSocketByName(FName("RightHandSocket"));
	if (HandSocket) {
		// Attach weapon to right hand socket:
		HandSocket->AttachActor(ActorToAttach, Character->GetMesh());
	}
}

void UCombatComponent::AttachActorToLeftHand(AActor* ActorToAttach) {
	if (Character == nullptr || Character->GetMesh() == nullptr || ActorToAttach == nullptr || EquippedWeapon == nullptr) return;
	// If attaching pistol/smg to left hand, use pistol's left hand socket, else use normal left hand socket
	bool bUsePistolSocket = EquippedWeapon->GetWeaponType() == EWeaponType::EWT_Pistol || EquippedWeapon->GetWeaponType() == EWeaponType::EWT_SubmachineGun;
	FName SocketName = bUsePistolSocket ? FName("PistolSocket") : FName("LeftHandSocket");
	// Get Character's socket called "LeftHandSocket"
	const USkeletalMeshSocket* HandSocket = Character->GetMesh()->GetSocketByName(SocketName);
	if (HandSocket) {
		// Attach weapon to right hand socket:
		HandSocket->AttachActor(ActorToAttach, Character->GetMesh());
	}
}

void UCombatComponent::AttachActorToBackpack(AActor* ActorToAttach) {
	if (Character == nullptr || Character->GetMesh() == nullptr || ActorToAttach == nullptr) return;

	// Gets backpack socket and attaches actor to that socket
	const USkeletalMeshSocket* BackpackSocket = Character->GetMesh()->GetSocketByName(FName("BackpackSocket"));
	if (BackpackSocket) {
		BackpackSocket->AttachActor(ActorToAttach, Character->GetMesh());
	}
}

void UCombatComponent::UpdateCarriedAmmo() {
	if (EquippedWeapon == nullptr) return;		

	// Set HUD Variables
	if (CarriedAmmoMap.Contains(EquippedWeapon->GetWeaponType())) {
		CarriedAmmo = CarriedAmmoMap[EquippedWeapon->GetWeaponType()];
	}
	Controller = Controller == nullptr ? Cast<AShooterPlayerController>(Character->Controller) : Controller;
	if (Controller) {
		Controller->SetHUDCarriedAmmo(CarriedAmmo);
	}
}

void UCombatComponent::OnRep_CarriedAmmo() {
	Controller = Controller == nullptr ? Cast<AShooterPlayerController>(Character->Controller) : Controller;
	if (Controller) {
		Controller->SetHUDCarriedAmmo(CarriedAmmo);
	}
	bool bJumpToShotgunEnd =
		CombatState == ECombatState::ECS_Reloading &&
		EquippedWeapon != nullptr &&
		EquippedWeapon->GetWeaponType() == EWeaponType::EWT_Shotgun &&
		CarriedAmmo == 0;
	if (bJumpToShotgunEnd) {
		JumpToShotgunEnd();
	}
}

void UCombatComponent::PlayEquipWeaponSound(AWeapon* WeaponToEquip) {
	// Plays the weapon's equip sound (if there is one)
	if (Character && WeaponToEquip && WeaponToEquip->EquipSound) {
		UGameplayStatics::PlaySoundAtLocation(this, WeaponToEquip->EquipSound, Character->GetActorLocation());
	}
}

void UCombatComponent::ReloadEmptyWeapon() {
	// Reload weapon IF mag is empty
	if (EquippedWeapon && EquippedWeapon->IsEmpty()) {
		Reload();
	}
}

int32 UCombatComponent::AmountToReload() {
	if (EquippedWeapon == nullptr) return 0;

	int32 RoomInMag = EquippedWeapon->GetMagCapacity() - EquippedWeapon->GetAmmo();
	if (CarriedAmmoMap.Contains(EquippedWeapon->GetWeaponType())) {
		int32 AmountCarried = CarriedAmmoMap[EquippedWeapon->GetWeaponType()];
		int32 Least = FMath::Min(RoomInMag, AmountCarried);
		return FMath::Clamp(RoomInMag, 0, Least);
	}
	return 0;
}

void UCombatComponent::Reload() {
	if (CarriedAmmo > 0 && CombatState == ECombatState::ECS_Unoccupied && EquippedWeapon && !EquippedWeapon->IsFull() && !bLocallyReloading) {
		ServerReload();
		HandleReload();
		bLocallyReloading = true;
	}
}

void UCombatComponent::ServerReload_Implementation() {
	if (Character == nullptr || EquippedWeapon == nullptr) return;
	CombatState = ECombatState::ECS_Reloading;
	if(!Character->IsLocallyControlled()) HandleReload();
}

void UCombatComponent::FinishReloading() {
	if (Character == nullptr) return;
	bLocallyReloading = false;
	if (Character->HasAuthority()) {
		CombatState = ECombatState::ECS_Unoccupied;
		UpdateAmmoValues();
	}
	if (bFireButtonPressed) {
		Fire();
	}
}


void UCombatComponent::UpdateAmmoValues() {
	if (Character == nullptr || EquippedWeapon == nullptr) return;

	// Figure out how much ammo to add to mag:
	int32 ReloadAmount = AmountToReload();
	if (CarriedAmmoMap.Contains(EquippedWeapon->GetWeaponType())) {
		// Subtract amount of ammo you're reloading from that weapon's carried ammo supply
		if (!bInfiniteCarriedAmmo) CarriedAmmoMap[EquippedWeapon->GetWeaponType()] -= ReloadAmount;
		CarriedAmmo = CarriedAmmoMap[EquippedWeapon->GetWeaponType()];
	}
	// Update HUD's carried ammo value
	Controller = Controller == nullptr ? Cast<AShooterPlayerController>(Character->Controller) : Controller;
	if (Controller) {
		Controller->SetHUDCarriedAmmo(CarriedAmmo);
	}
	// Add that ammo to mag
	EquippedWeapon->AddAmmo(ReloadAmount);
}


void UCombatComponent::UpdateShotgunAmmoValues() {
	if (Character == nullptr || EquippedWeapon == nullptr) return;

	// Remove ammo from carried ammo
	if (CarriedAmmoMap.Contains(EquippedWeapon->GetWeaponType())) {
		// Subtract 1 pellet from shotgun's carried ammo supply (if InfiniteCarriedAmmo is false)
		if (!bInfiniteCarriedAmmo) CarriedAmmoMap[EquippedWeapon->GetWeaponType()] -= 1;
		// Update CarriedAmmo to correct value
		CarriedAmmo = CarriedAmmoMap[EquippedWeapon->GetWeaponType()];
	}
	// Update HUD's carried ammo value
	Controller = Controller == nullptr ? Cast<AShooterPlayerController>(Character->Controller) : Controller;
	if (Controller) {
		Controller->SetHUDCarriedAmmo(CarriedAmmo);
	}
	// Add 1 pellet to shotgun
	EquippedWeapon->AddAmmo(1);

	// Let player fire after reloading 1 pellet
	bCanFire = true;

	// Check if shotgun mag capacity is full, and stop reloading if so
	if (EquippedWeapon->IsFull() || CarriedAmmo == 0) {
		JumpToShotgunEnd();
	}
}

void UCombatComponent::JumpToShotgunEnd() {
	// Jump to ShotgunEnd Montage Section
	UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
	if (AnimInstance && Character->GetReloadMontage()) {
		AnimInstance->Montage_JumpToSection(FName("ShotgunEnd"));
	}
}

void UCombatComponent::OnRep_CombatState() {
	switch (CombatState) {
	case ECombatState::ECS_Reloading:
		if(Character && !Character->IsLocallyControlled()) HandleReload();
		break;
	case ECombatState::ECS_Unoccupied:
		if (bFireButtonPressed) {
			Fire();
		}
		break;
	case ECombatState::ECS_ThrowingGrenade:
		if (Character && !Character->IsLocallyControlled()) {
			Character->PlayThrowGrenadeMontage();
			AttachActorToLeftHand(EquippedWeapon);
			ShowAttachedGrenade(true);
		}
		break;
	case ECombatState::ECS_SwappingWeapons:
		if (Character && !Character->IsLocallyControlled()) {
			Character->PlaySwapMontage();
		}
		break;
	}
}

void UCombatComponent::HandleReload() {
	// Play Reload Montage
	if(Character) Character->PlayReloadMontage();
}

void UCombatComponent::TraceUnderCrosshairs(FHitResult& TraceHitResult) {
	FVector2D ViewportSize;
	if (GEngine && GEngine->GameViewport) {
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}
	// Sets crosshair location to center of screen:
	FVector2D CrosshairLocation(ViewportSize.X / 2.f, ViewportSize.Y / 2.f);
	FVector CrosshairWorldPosition;
	FVector CrosshairWorldDirection;
	bool bScreenToWorld = UGameplayStatics::DeprojectScreenToWorld(
		UGameplayStatics::GetPlayerController(this, 0),
		CrosshairLocation,
		CrosshairWorldPosition,
		CrosshairWorldDirection
	);
	if (bScreenToWorld) {
		// Successfully got WORLD coords of screen center, perform line trace now
		FVector Start = CrosshairWorldPosition;

		if (Character) {
			float DistanceToCharacter = (Character->GetActorLocation() - Start).Size();
			Start += CrosshairWorldDirection * (DistanceToCharacter + 100.f);
		}

		FVector End = Start + CrosshairWorldDirection * TRACE_LENGTH;
		GetWorld()->LineTraceSingleByChannel(
			TraceHitResult,
			Start,
			End,
			ECollisionChannel::ECC_Visibility
		);
		if (!TraceHitResult.bBlockingHit) TraceHitResult.ImpactPoint = End;

		// If crosshair is overlapping with player, draw it red
		if (TraceHitResult.GetActor() && TraceHitResult.GetActor()->Implements<UInteractWithCrosshairsInterface>()) {
			HUDPackage.CrosshairsColor = FLinearColor::Red;
		}
		else {
			HUDPackage.CrosshairsColor = FLinearColor::White;
		}

	}
}

void UCombatComponent::SetAiming(bool bIsAiming) {
	if (Character == nullptr || EquippedWeapon == nullptr) return;
	bAiming = bIsAiming;
	ServerSetAiming(bIsAiming);
	if (Character) {
		Character->GetCharacterMovement()->MaxWalkSpeed = bIsAiming ? AimWalkSpeed : BaseWalkSpeed;
	}

	if (Character->IsLocallyControlled() && EquippedWeapon->GetWeaponType() == EWeaponType::EWT_SniperRifle) {
		Character->ShowSniperScopeWidget(bIsAiming);

		// Hide Local Mesh & Gun while zoomed in with sniper
		Character->ShowLocalMesh(!bIsAiming);
		Character->bSniperAiming = bAiming;

		EquippedWeapon->GetWeaponMesh()->bOwnerNoSee = bAiming;
	}
	if (Character->IsLocallyControlled()) bAimButtonPressed = bIsAiming;
}

void UCombatComponent::OnRep_Aiming() {
	if (Character && Character->IsLocallyControlled()) {
		bAiming = bAimButtonPressed;
	}
}

void UCombatComponent::ServerSetAiming_Implementation(bool bIsAiming) {
	bAiming = bIsAiming;
	if (Character) {
		Character->GetCharacterMovement()->MaxWalkSpeed = bIsAiming ? AimWalkSpeed : BaseWalkSpeed;
	}
}

void UCombatComponent::OnRep_Grenades() {
	UpdateHUDGrenades();
}

void UCombatComponent::UpdateHUDGrenades() {
	Controller = Controller == nullptr ? Cast<AShooterPlayerController>(Character->Controller) : Controller;
	if (Controller) {
		Controller->SetHUDGrenades(Grenades);
	}

}

bool UCombatComponent::ShouldSwapWeapons() {
	return (EquippedWeapon != nullptr && SecondaryWeapon != nullptr);
}

void UCombatComponent::ThrowGrenade() {
	if (Grenades == 0 || CombatState != ECombatState::ECS_Unoccupied || EquippedWeapon == nullptr) return;
	CombatState = ECombatState::ECS_ThrowingGrenade;
	// Play grenade throwing montage
	if (Character) {
		Character->PlayThrowGrenadeMontage();
		AttachActorToLeftHand(EquippedWeapon);
		ShowAttachedGrenade(true);
		// Call server RPC if not the server
		if (!Character->HasAuthority()) {
			ServerThrowGrenade();
		}
		else {
			// Subtract 1 from character's grenade amount, then update HUD
			Grenades = FMath::Clamp(Grenades - 1, 0, MaxGrenades);
			UpdateHUDGrenades();
		}
	}

}

void UCombatComponent::ServerThrowGrenade_Implementation() {
	if (Grenades == 0) return;
	CombatState = ECombatState::ECS_ThrowingGrenade;
	// Play grenade throwing montage
	if (Character) {
		Character->PlayThrowGrenadeMontage();
		AttachActorToLeftHand(EquippedWeapon);
		ShowAttachedGrenade(true);
	}
	// Subtract 1 from character's grenade amount, then update HUD
	Grenades = FMath::Clamp(Grenades - 1, 0, MaxGrenades);
	UpdateHUDGrenades();
}

void UCombatComponent::ShowAttachedGrenade(bool bShowGrenade) {
	if (Character && Character->GetAttachedGrenade()) {
		Character->GetAttachedGrenade()->SetVisibility(bShowGrenade);
	}
}

void UCombatComponent::ThrowGrenadeFinished() {
	CombatState = ECombatState::ECS_Unoccupied;
	AttachActorToRightHand(EquippedWeapon);
}

void UCombatComponent::LaunchGrenade() {
	// Hide character's grenade static mesh
	ShowAttachedGrenade(false);
	if (Character && Character->IsLocallyControlled()) ServerLaunchGrenade(HitTarget);
}

void UCombatComponent::ServerLaunchGrenade_Implementation(const FVector_NetQuantize& Target) {
	// Spawn grenade
	if (Character && GrenadeClass && Character->GetAttachedGrenade()) {
		const FVector StartingLocation = Character->GetAttachedGrenade()->GetComponentLocation();
		FVector ToTarget = Target - StartingLocation;
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = Character;
		SpawnParams.Instigator = Character;
		UWorld* World = GetWorld();
		if (World) {
			World->SpawnActor<AProjectile>(GrenadeClass, StartingLocation, ToTarget.Rotation(), SpawnParams);
		}
	}
}

void UCombatComponent::SetSpeed(float BaseSpeed) {
	AimWalkSpeed = BaseSpeed - 300.f;
	BaseWalkSpeed = BaseSpeed;
}

void UCombatComponent::InitializeCarriedAmmo() {
	CarriedAmmoMap.Emplace(EWeaponType::EWT_AssaultRifle, StartingARAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_RocketLauncher, StartingRocketAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_Pistol, StartingPistolAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_SniperRifle, StartingSniperAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_SubmachineGun, StartingSMGAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_Shotgun, StartingShotgunAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_GrenadeLauncher, StartingGrenadeLauncherAmmo);
}

