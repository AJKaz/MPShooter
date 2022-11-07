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

UCombatComponent::UCombatComponent() {
	PrimaryComponentTick.bCanEverTick = true;

	BaseWalkSpeed = 1000.f;
	AimWalkSpeed = 700.f;
	SetIsReplicatedByDefault(true);
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

		ServerFire(HitTarget);
		if (EquippedWeapon) {
			CrosshairShootingFactor = 0.75f;
			bCanFire = false;
		}
		StartFireTimer();
	}
}

bool UCombatComponent::CanFire() {
	if (EquippedWeapon == nullptr) return false;
	// IF character is reloading a shotgun, let them shoot after each pellet reloads (if they click)
	if (!EquippedWeapon->IsEmpty() && bCanFire && CombatState == ECombatState::ECS_Reloading && EquippedWeapon->GetWeaponType() == EWeaponType::EWT_Shotgun) {
		return true;
	}
	return !EquippedWeapon->IsEmpty() && bCanFire && CombatState == ECombatState::ECS_Unoccupied;
}

void UCombatComponent::OnRep_CarriedAmmo() {
	Controller = Controller == nullptr ? Cast<AShooterPlayerController>(Character->Controller) : Controller;
	if (Controller) {
		Controller->SetHUDCarriedAmmo(CarriedAmmo);
	}
	// If character has a SHOTGUN, is reloading, and has no more carried shotgun ammo, stop reloading
	if (CombatState == ECombatState::ECS_Reloading && 
		EquippedWeapon != nullptr &&
		EquippedWeapon->GetWeaponType() == EWeaponType::EWT_Shotgun && 
		CarriedAmmo == 0) {
		JumpToShotgunEnd();
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

void UCombatComponent::ServerFire_Implementation(const FVector_NetQuantize& TraceHitTarget) {
	MulticastFire(TraceHitTarget);
}

void UCombatComponent::MulticastFire_Implementation(const FVector_NetQuantize& TraceHitTarget) {
	if (EquippedWeapon == nullptr) return;

	// Let player shoot while reloading a shotgun
	if (Character && CombatState == ECombatState::ECS_Reloading && EquippedWeapon->GetWeaponType() == EWeaponType::EWT_Shotgun) {
		Character->PlayFireMontage(bAiming);
		EquippedWeapon->Fire(TraceHitTarget);
		CombatState = ECombatState::ECS_Unoccupied;
		return;
	}

	if (Character && CombatState == ECombatState::ECS_Unoccupied) {
		Character->PlayFireMontage(bAiming);
		EquippedWeapon->Fire(TraceHitTarget);
	}
}

void UCombatComponent::EquipWeapon(AWeapon* WeaponToEquip) {
	if (Character == nullptr || WeaponToEquip == nullptr || CombatState != ECombatState::ECS_Unoccupied) return;

	// IF Character has a weapon equipped, drop it
	DropEquippedWeapon();

	EquippedWeapon = WeaponToEquip;
	EquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped);

	// Attach Weapon to Character's RightHandSockete
	AttachActorToRightHand(EquippedWeapon);

	// Make character owner of weapon:
	EquippedWeapon->SetOwner(Character);	
	
	UpdateCarriedAmmo();	
	PlayEquipWeaponSound();
	ReloadEmptyWeapon();
		
	Character->GetCharacterMovement()->bOrientRotationToMovement = false;
	Character->bUseControllerRotationYaw = true;
}

void UCombatComponent::OnRep_EquippedWeapon() {
	if (EquippedWeapon && Character) {
		EquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped);

		AttachActorToRightHand(EquippedWeapon);
		PlayEquipWeaponSound();

		Character->GetCharacterMovement()->bOrientRotationToMovement = false;
		Character->bUseControllerRotationYaw = true;
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
	FName SocketName = bUsePistolSocket ? FName("PistolSocket") : FName("LefthandSocket");
	// Get Character's socket called "LeftHandSocket"
	const USkeletalMeshSocket* HandSocket = Character->GetMesh()->GetSocketByName(SocketName);
	if (HandSocket) {
		// Attach weapon to right hand socket:
		HandSocket->AttachActor(ActorToAttach, Character->GetMesh());
	}
}

void UCombatComponent::UpdateCarriedAmmo() {
	if (EquippedWeapon == nullptr) return;
	// Set HUD Variables
	EquippedWeapon->SetHUDAmmo();
	if (CarriedAmmoMap.Contains(EquippedWeapon->GetWeaponType())) {
		CarriedAmmo = CarriedAmmoMap[EquippedWeapon->GetWeaponType()];
	}
	Controller = Controller == nullptr ? Cast<AShooterPlayerController>(Character->Controller) : Controller;
	if (Controller) {
		Controller->SetHUDCarriedAmmo(CarriedAmmo);
	}
}

void UCombatComponent::PlayEquipWeaponSound() {	
	// Plays the weapon's equip sound (if there is one)
	if (Character && EquippedWeapon && EquippedWeapon->EquipSound) {
		UGameplayStatics::PlaySoundAtLocation(this, EquippedWeapon->EquipSound, Character->GetActorLocation());
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
	if (Character == nullptr || EquippedWeapon == nullptr) return;

	if (EquippedWeapon && !EquippedWeapon->IsFull() && CarriedAmmo > 0 && CombatState == ECombatState::ECS_Unoccupied) {
		ServerReload();
	}
}

void UCombatComponent::ServerReload_Implementation() {
	CombatState = ECombatState::ECS_Reloading;
	HandleReload();
}

void UCombatComponent::FinishReloading() {
	if (Character == nullptr) return;
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
		if(!bInfiniteCarriedAmmo) CarriedAmmoMap[EquippedWeapon->GetWeaponType()] -= ReloadAmount;
		CarriedAmmo = CarriedAmmoMap[EquippedWeapon->GetWeaponType()];
	}
	// Update HUD's carried ammo value
	Controller = Controller == nullptr ? Cast<AShooterPlayerController>(Character->Controller) : Controller;
	if (Controller) {
		Controller->SetHUDCarriedAmmo(CarriedAmmo);
	}
	// Add that ammo to mag
	EquippedWeapon->AddAmmo(-ReloadAmount);
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
	EquippedWeapon->AddAmmo(-1);
	
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
			HandleReload();
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
	}
}

void UCombatComponent::HandleReload() {
	// Play Reload Montage
	Character->PlayReloadMontage();
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
	if(Character && Character->IsLocallyControlled()) ServerLaunchGrenade(HitTarget);
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

void UCombatComponent::InitializeCarriedAmmo() {
	CarriedAmmoMap.Emplace(EWeaponType::EWT_AssaultRifle, StartingARAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_RocketLauncher, StartingRocketAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_Pistol, StartingPistolAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_SniperRifle, StartingSniperAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_SubmachineGun, StartingSMGAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_Shotgun, StartingShotgunAmmo);
	CarriedAmmoMap.Emplace(EWeaponType::EWT_GrenadeLauncher, StartingGrenadeLauncherAmmo);
}

