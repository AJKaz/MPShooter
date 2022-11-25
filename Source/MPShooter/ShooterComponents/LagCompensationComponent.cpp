// Fill out your copyright notice in the Description page of Project Settings.


#include "LagCompensationComponent.h"
#include "MPShooter/Character/ShooterCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "MPShooter/Weapons/Weapon.h"
#include "MPShooter/MPShooter.h"

ULagCompensationComponent::ULagCompensationComponent() {
	PrimaryComponentTick.bCanEverTick = true;

}


void ULagCompensationComponent::BeginPlay() {
	Super::BeginPlay();
}

void ULagCompensationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	SaveFramePackage();
}

void ULagCompensationComponent::SaveFramePackage() {
	if (Character == nullptr || !Character->HasAuthority()) return;
	if (FrameHistory.Num() <= 1) {
		FFramePackage ThisFrame;
		SaveFramePackage(ThisFrame);
		FrameHistory.AddHead(ThisFrame);
	}
	else {
		// Newest time - oldest time, = amount of total time stored in list
		float HistoryLength = FrameHistory.GetHead()->GetValue().Time - FrameHistory.GetTail()->GetValue().Time;

		// If HistoryLength > MaxFrameTime, remove nodes from tail until historylength < maxframetime
		while (HistoryLength > MaxRecordTime) {
			// Too many frame packages, remove some
			FrameHistory.RemoveNode(FrameHistory.GetTail());
			HistoryLength = FrameHistory.GetHead()->GetValue().Time - FrameHistory.GetTail()->GetValue().Time;
		}
		// Save current frame in Head Node
		FFramePackage ThisFrame;
		SaveFramePackage(ThisFrame);
		FrameHistory.AddHead(ThisFrame);

		// Draw saved frame for debugging
		// ShowFramePackage(ThisFrame, FColor::Red);
	}
}

void ULagCompensationComponent::SaveFramePackage(FFramePackage& Package) {
	Character = Character == nullptr ? Cast<AShooterCharacter>(GetOwner()) : Character;
	if (Character) {
		// Store official server time & Character
		Package.Time = GetWorld()->GetTimeSeconds();
		Package.Character = Character;
		// Loop thru all pairs of collision boxes
		for (auto& BoxPair : Character->HitCollisionBoxes) {
			// Store location, rotation, and box extent info for all collision boxes
			FBoxInformation BoxInformation;
			BoxInformation.Location = BoxPair.Value->GetComponentLocation();
			BoxInformation.Rotation = BoxPair.Value->GetComponentRotation();
			BoxInformation.BoxExtent = BoxPair.Value->GetScaledBoxExtent();

			// Store in HitBoxInfo TMap
			Package.HitBoxInfo.Add(BoxPair.Key, BoxInformation);
		}
	}
}

void ULagCompensationComponent::ShowFramePackage(const FFramePackage& Package, const FColor& Color) {
	for (auto& BoxInfo : Package.HitBoxInfo) {
		// Draw debug box for all boxes
		DrawDebugBox(GetWorld(), BoxInfo.Value.Location, BoxInfo.Value.BoxExtent, FQuat(BoxInfo.Value.Rotation), Color, false, 4.f);
	}
}


void ULagCompensationComponent::ServerScoreRequest_Implementation(AShooterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation, float HitTime, AWeapon* DamageCauser) {
	FServerSideRewindResult Confirm = ServerSideRewind(HitCharacter, TraceStart, HitLocation, HitTime);

	// Got a hit, apply damage
	if (Character && HitCharacter && DamageCauser && Confirm.bHitConfirmed) {
		UGameplayStatics::ApplyDamage(
			HitCharacter,
			DamageCauser->GetDamage(),
			Character->Controller,
			DamageCauser,
			UDamageType::StaticClass()
		);
	}
}

void ULagCompensationComponent::ProjectileScoreRequest_Implementation(AShooterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize100& InitialVelocity, float HitTime) {
	FServerSideRewindResult Confirm = ProjectileServerSideRewind(HitCharacter, TraceStart, InitialVelocity, HitTime);

	// Got a hit, apply damage
	if (Character && HitCharacter && Character->GetEquippedWeapon() && Confirm.bHitConfirmed) {
		UGameplayStatics::ApplyDamage(
			HitCharacter,
			Character->GetEquippedWeapon()->GetDamage(),
			Character->Controller,
			Character->GetEquippedWeapon(),
			UDamageType::StaticClass()
		);
	}
}

void ULagCompensationComponent::ShotgunServerScoreRequest_Implementation(const TArray<AShooterCharacter*>& HitCharacters, const FVector_NetQuantize& TraceStart, const TArray<FVector_NetQuantize>& HitLocations, float HitTime, AWeapon* DamageCauser) {
	FShotgunServerSideRewindResult Confirm = ShotgunServerSideRewind(HitCharacters, TraceStart, HitLocations, HitTime);

	// Calculate total damage to deal to each character
	for (auto& HitCharacter : HitCharacters) {
		if (HitCharacter == nullptr || DamageCauser == nullptr || Character == nullptr) continue;
		float TotalDamage = 0.f;
		// Calculate head shot damage
		if (Confirm.HeadShots.Contains(HitCharacter)) {
			TotalDamage += Confirm.HeadShots[HitCharacter] * DamageCauser->GetDamage();
		}
		// Calculate body shot damage
		if (Confirm.BodyShots.Contains(HitCharacter)) {
			TotalDamage += Confirm.BodyShots[HitCharacter] * DamageCauser->GetDamage();
		}
		// Apply that total shotgun damage
		UGameplayStatics::ApplyDamage(
			HitCharacter,
			TotalDamage,
			Character->Controller,
			DamageCauser,
			UDamageType::StaticClass()
		);
	}
}

FServerSideRewindResult ULagCompensationComponent::ServerSideRewind(AShooterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation, float HitTime) {	
	FFramePackage FrameToCheck = GetFrameToCheck(HitCharacter, HitTime);
	return ConfirmHit(FrameToCheck, HitCharacter, TraceStart, HitLocation);
}

FServerSideRewindResult ULagCompensationComponent::ProjectileServerSideRewind(AShooterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize100& InitialVelocity, float HitTime) {
	FFramePackage FrameToCheck = GetFrameToCheck(HitCharacter, HitTime);
	return ProjectileConfirmHit(FrameToCheck, HitCharacter, TraceStart, InitialVelocity, HitTime);
}

FShotgunServerSideRewindResult ULagCompensationComponent::ShotgunServerSideRewind(const TArray<AShooterCharacter*>& HitCharacters, const FVector_NetQuantize& TraceStart, const TArray<FVector_NetQuantize>& HitLocations, float HitTime) {
	TArray<FFramePackage> FramesToCheck;
	for (AShooterCharacter* HitCharacter : HitCharacters) {
		FramesToCheck.Add(GetFrameToCheck(HitCharacter, HitTime));
	}
	
	return ShotgunConfirmHit(FramesToCheck, TraceStart, HitLocations);
}

FFramePackage ULagCompensationComponent::GetFrameToCheck(AShooterCharacter* HitCharacter, float HitTime) {
	bool bReturn =
		HitCharacter == nullptr ||
		HitCharacter->GetLagCompensation() == nullptr ||
		HitCharacter->GetLagCompensation()->FrameHistory.GetHead() == nullptr ||
		HitCharacter->GetLagCompensation()->FrameHistory.GetTail() == nullptr;
	if (bReturn) return FFramePackage();
	// Frame package that we check to verify a hit
	FFramePackage FrameToCheck;
	bool bShouldInterpolate = true;
	// Frame history of the HitCharacter
	const TDoubleLinkedList<FFramePackage>& History = HitCharacter->GetLagCompensation()->FrameHistory;
	const float OldestHistoryTime = History.GetTail()->GetValue().Time;
	const float NewestHistoryTime = History.GetHead()->GetValue().Time;
	if (OldestHistoryTime > HitTime) {
		// too far back - too laggy to do SSR
		return FFramePackage();
	}
	if (OldestHistoryTime == HitTime) {
		FrameToCheck = History.GetTail()->GetValue();
		bShouldInterpolate = false;
	}
	if (NewestHistoryTime <= HitTime) {
		FrameToCheck = History.GetHead()->GetValue();
		bShouldInterpolate = false;
	}
	TDoubleLinkedList<FFramePackage>::TDoubleLinkedListNode* Younger = History.GetHead();
	TDoubleLinkedList<FFramePackage>::TDoubleLinkedListNode* Older = Younger;
	while (Older->GetValue().Time > HitTime) // is Older still younger than HitTime?
	{
		// March back until: OlderTime < HitTime < YoungerTime
		if (Older->GetNextNode() == nullptr) break;
		Older = Older->GetNextNode();
		if (Older->GetValue().Time > HitTime) {
			Younger = Older;
		}
	}
	if (Older->GetValue().Time == HitTime) // highly unlikely, but we found our frame to check
	{
		FrameToCheck = Older->GetValue();
		bShouldInterpolate = false;
	}
	if (bShouldInterpolate) {
		// Interpolate between Younger and Older
		FrameToCheck = InterpBetweenFrames(Older->GetValue(), Younger->GetValue(), HitTime);
	}
	FrameToCheck.Character = HitCharacter;
	return FrameToCheck;
}


FFramePackage ULagCompensationComponent::InterpBetweenFrames(const FFramePackage& OlderFrame, const FFramePackage& YoungerFrame, float HitTime) {
	const float Distance = YoungerFrame.Time - OlderFrame.Time;
	const float InterpFraction = FMath::Clamp((HitTime - OlderFrame.Time) / Distance, 0.f, 1.f);

	FFramePackage InterpFramePackage;
	InterpFramePackage.Time = HitTime;

	// Loops thru a TMap, filling in InterpFramePackage with the box's interpolated values
	for (auto& YoungerPair : YoungerFrame.HitBoxInfo) {
		const FName& BoxInfoName = YoungerPair.Key;
		const FBoxInformation& OlderBox = OlderFrame.HitBoxInfo[BoxInfoName];
		const FBoxInformation& YoungerBox = YoungerFrame.HitBoxInfo[BoxInfoName];

		FBoxInformation InterpBoxInfo;
		// Interpolate to the box's location and rotation between the given frame
		InterpBoxInfo.Location = FMath::VInterpTo(OlderBox.Location, YoungerBox.Location, 1.f, InterpFraction);
		InterpBoxInfo.Rotation = FMath::RInterpTo(OlderBox.Rotation, YoungerBox.Rotation, 1.f, InterpFraction);
		// Box extent doesn't change frame to frame, just save either one in the box info struct
		InterpBoxInfo.BoxExtent = YoungerBox.BoxExtent;

		InterpFramePackage.HitBoxInfo.Add(BoxInfoName, InterpBoxInfo);
	}

	return InterpFramePackage;
}


FServerSideRewindResult ULagCompensationComponent::ConfirmHit(const FFramePackage& Package, AShooterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation) {
	if (HitCharacter == nullptr) return FServerSideRewindResult();

	FFramePackage CurrentFrame;
	CacheBoxPositions(HitCharacter, CurrentFrame);
	MoveBoxes(HitCharacter, Package);
	EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::NoCollision);

	// Enable collision for the head first
	UBoxComponent* HeadBox = HitCharacter->HitCollisionBoxes[FName("head")];
	HeadBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	HeadBox->SetCollisionResponseToChannel(ECC_HitBox, ECollisionResponse::ECR_Block);

	FHitResult ConfirmHitResult;
	const FVector TraceEnd = TraceStart + (HitLocation - TraceStart) * 1.25f;
	UWorld* World = GetWorld();
	if (World) {
		World->LineTraceSingleByChannel(
			ConfirmHitResult,
			TraceStart,
			TraceEnd,
			ECC_HitBox
		);
		if (ConfirmHitResult.bBlockingHit) {	// we hit the head, return early
			// Draw debug for blocking hit
			if (ConfirmHitResult.Component.IsValid()) {
				UBoxComponent* Box = Cast<UBoxComponent>(ConfirmHitResult.Component);
				if (Box) {
					DrawDebugBox(World, Box->GetComponentLocation(), Box->GetScaledBoxExtent(), FQuat(Box->GetComponentRotation()), FColor::Red, false, 8.f);
				}
			}

			ResetHitBoxes(HitCharacter, CurrentFrame);
			EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics);
			return FServerSideRewindResult{ true, true };
		}
		else {		// didn't hit head, check the rest of the boxes
			for (auto& HitBoxPair : HitCharacter->HitCollisionBoxes) {
				if (HitBoxPair.Value != nullptr) {
					HitBoxPair.Value->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
					HitBoxPair.Value->SetCollisionResponseToChannel(ECC_HitBox, ECollisionResponse::ECR_Block);
				}
			}
			World->LineTraceSingleByChannel(
				ConfirmHitResult,
				TraceStart,
				TraceEnd,
				ECC_HitBox
			);
			if (ConfirmHitResult.bBlockingHit) {
				// Draw debug for blocking hit
				if (ConfirmHitResult.Component.IsValid()) {
					UBoxComponent* Box = Cast<UBoxComponent>(ConfirmHitResult.Component);
					if (Box) {
						DrawDebugBox(World, Box->GetComponentLocation(), Box->GetScaledBoxExtent(), FQuat(Box->GetComponentRotation()), FColor::Blue, false, 8.f);
					}
				}
				ResetHitBoxes(HitCharacter, CurrentFrame);
				EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics);
				return FServerSideRewindResult{ true, false };
			}
		}
	}

	ResetHitBoxes(HitCharacter, CurrentFrame);
	EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics);
	return FServerSideRewindResult{ false, false };
}

FServerSideRewindResult ULagCompensationComponent::ProjectileConfirmHit(const FFramePackage& Package, AShooterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize100& InitialVelocity, float HitTime) {
	if (HitCharacter == nullptr) return FServerSideRewindResult();

	FFramePackage CurrentFrame;
	CacheBoxPositions(HitCharacter, CurrentFrame);
	MoveBoxes(HitCharacter, Package);
	EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::NoCollision);

	// Enable collision for the head first
	UBoxComponent* HeadBox = HitCharacter->HitCollisionBoxes[FName("head")];
	HeadBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	HeadBox->SetCollisionResponseToChannel(ECC_HitBox, ECollisionResponse::ECR_Block);

	// Predict projectile's path
	FPredictProjectilePathParams PathParams;
	PathParams.bTraceWithCollision = true;
	PathParams.MaxSimTime = MaxRecordTime;
	PathParams.LaunchVelocity = InitialVelocity;
	PathParams.StartLocation = TraceStart;
	PathParams.SimFrequency = 15.f;
	PathParams.ProjectileRadius = 5.f;
	PathParams.TraceChannel = ECC_HitBox;
	PathParams.ActorsToIgnore.Add(GetOwner());
	// Debug path
	PathParams.DrawDebugTime = 5.f;
	PathParams.DrawDebugType = EDrawDebugTrace::ForDuration;
	// Path result contains data about trace
	FPredictProjectilePathResult PathResult;
	UGameplayStatics::PredictProjectilePath(this, PathParams, PathResult);

	UWorld* World = GetWorld();
	if (PathResult.HitResult.bBlockingHit) {
		// Hit head, return early
		if (PathResult.HitResult.Component.IsValid()) {
			UBoxComponent* Box = Cast<UBoxComponent>(PathResult.HitResult.Component);
			if (Box && World) {
				DrawDebugBox(World, Box->GetComponentLocation(), Box->GetScaledBoxExtent(), FQuat(Box->GetComponentRotation()), FColor::Red, false, 8.f);
			}
		}

		ResetHitBoxes(HitCharacter, CurrentFrame);
		EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics);
		return FServerSideRewindResult{ true, true };
	}
	else {
		// Didn't hit head, check rest of body hit boxes
		// Enable collision for all hitboxes
		for (auto& HitBoxPair : HitCharacter->HitCollisionBoxes) {
			if (HitBoxPair.Value != nullptr) {
				HitBoxPair.Value->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				HitBoxPair.Value->SetCollisionResponseToChannel(ECC_HitBox, ECollisionResponse::ECR_Block);
			}
		}
		// Predict projectile path
		UGameplayStatics::PredictProjectilePath(this, PathParams, PathResult);
		if (PathResult.HitResult.bBlockingHit) {
			// Draw debug for blocking hit
			if (PathResult.HitResult.Component.IsValid()) {
				UBoxComponent* Box = Cast<UBoxComponent>(PathResult.HitResult.Component);
				if (Box && World) {
					DrawDebugBox(World, Box->GetComponentLocation(), Box->GetScaledBoxExtent(), FQuat(Box->GetComponentRotation()), FColor::Blue, false, 8.f);
				}
			}
			ResetHitBoxes(HitCharacter, CurrentFrame);
			EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics);
			return FServerSideRewindResult{ true, false };
		}

	}
	// No hit, reset boxes and collision and return false
	ResetHitBoxes(HitCharacter, CurrentFrame);
	EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics);
	return FServerSideRewindResult{ false, false };
}

FShotgunServerSideRewindResult ULagCompensationComponent::ShotgunConfirmHit(const TArray<FFramePackage>& FramePackages, const FVector_NetQuantize& TraceStart, const TArray<FVector_NetQuantize>& HitLocations) {
	FShotgunServerSideRewindResult ShotgunResult;
	TArray<FFramePackage> CurrentFrames;

	// Ensure there are no nullptr Characters
	for (auto& Frame : FramePackages) if (Frame.Character == nullptr) return FShotgunServerSideRewindResult();
	
	for (auto& Frame : FramePackages) {
		FFramePackage CurrentFrame;
		CurrentFrame.Character = Frame.Character;
		CacheBoxPositions(Frame.Character, CurrentFrame);
		MoveBoxes(Frame.Character, Frame);
		EnableCharacterMeshCollision(Frame.Character, ECollisionEnabled::NoCollision);
		CurrentFrames.Add(CurrentFrame);
	}

	// Enable collision for the head first
	for (auto& Frame : FramePackages) {
		UBoxComponent* HeadBox = Frame.Character->HitCollisionBoxes[FName("head")];
		HeadBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		HeadBox->SetCollisionResponseToChannel(ECC_HitBox, ECollisionResponse::ECR_Block);
	}
	
	// Check for headshots
	UWorld* World = GetWorld();
	for (auto& HitLocation : HitLocations) {
		FHitResult ConfirmHitResult;
		const FVector TraceEnd = TraceStart + (HitLocation - TraceStart) * 1.25f;		
		if (World) {
			// Perform line trace
			World->LineTraceSingleByChannel(
				ConfirmHitResult,
				TraceStart,
				TraceEnd,
				ECC_HitBox
			);
			// Check if we got a hit
			AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(ConfirmHitResult.GetActor());
			if (ShooterCharacter) {
				// Draw debug for blocking hit
				if (ConfirmHitResult.Component.IsValid()) {
					UBoxComponent* Box = Cast<UBoxComponent>(ConfirmHitResult.Component);
					if (Box) {
						DrawDebugBox(World, Box->GetComponentLocation(), Box->GetScaledBoxExtent(), FQuat(Box->GetComponentRotation()), FColor::Blue, false, 8.f);
					}
				}
				// Got a valid hit (and it's a headshot)
				// Add headshot count to that character in the TMap
				if (ShotgunResult.HeadShots.Contains(ShooterCharacter)) {
					ShotgunResult.HeadShots[ShooterCharacter]++;
				}
				else {
					ShotgunResult.HeadShots.Emplace(ShooterCharacter, 1);
				}
			}
		}
	}

	// Enable collision for all hitboxes and disable headbox collisions
	for (auto& Frame : FramePackages) {
		// Enable all collision boxes for all characters that were hit
		for (auto& HitBoxPair : Frame.Character->HitCollisionBoxes) {
			if (HitBoxPair.Value != nullptr) {
				HitBoxPair.Value->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				HitBoxPair.Value->SetCollisionResponseToChannel(ECC_HitBox, ECollisionResponse::ECR_Block);
			}
		}
		// Disable collisions for all character's head boxes to ensure we don't get duplicate headshots
		UBoxComponent* HeadBox = Frame.Character->HitCollisionBoxes[FName("head")];
		HeadBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	
	// Check for body shots
	for (auto& HitLocation : HitLocations) {
		FHitResult ConfirmHitResult;
		const FVector TraceEnd = TraceStart + (HitLocation - TraceStart) * 1.25f;
		if (World) {
			// Perform line trace
			World->LineTraceSingleByChannel(
				ConfirmHitResult,
				TraceStart,
				TraceEnd,
				ECC_HitBox
			);
			// Check if we got a hit
			AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(ConfirmHitResult.GetActor());
			if (ShooterCharacter) {
				// Draw debug for blocking hit
				if (ConfirmHitResult.Component.IsValid()) {
					UBoxComponent* Box = Cast<UBoxComponent>(ConfirmHitResult.Component);
					if (Box) {
						DrawDebugBox(World, Box->GetComponentLocation(), Box->GetScaledBoxExtent(), FQuat(Box->GetComponentRotation()), FColor::Blue, false, 8.f);
					}
				}
				// Got a valid body shot, add bodyshot count to that character in the TMap
				if (ShotgunResult.BodyShots.Contains(ShooterCharacter)) {
					ShotgunResult.BodyShots[ShooterCharacter]++;
				}
				else {
					ShotgunResult.BodyShots.Emplace(ShooterCharacter, 1);
				}
			}
		}
	}
	for (auto& Frame : CurrentFrames) {
		ResetHitBoxes(Frame.Character, Frame);
		EnableCharacterMeshCollision(Frame.Character, ECollisionEnabled::QueryAndPhysics);
	}

	return ShotgunResult;
}

void ULagCompensationComponent::CacheBoxPositions(AShooterCharacter* HitCharacter, FFramePackage& OutFramePackage) {
	if (HitCharacter == nullptr) return;
	// loop over hit character's collision boxes TMap
	for (TTuple<FName, UBoxComponent*>& HitBoxPair : HitCharacter->HitCollisionBoxes) {
		if (HitBoxPair.Value != nullptr) {
			FBoxInformation BoxInfo;
			BoxInfo.Location = HitBoxPair.Value->GetComponentLocation();
			BoxInfo.Rotation = HitBoxPair.Value->GetComponentRotation();
			BoxInfo.BoxExtent = HitBoxPair.Value->GetScaledBoxExtent();
			OutFramePackage.HitBoxInfo.Add(HitBoxPair.Key, BoxInfo);
		}
	}
}

void ULagCompensationComponent::MoveBoxes(AShooterCharacter* HitCharacter, const FFramePackage& Package) {
	if (HitCharacter == nullptr) return;
	for (TTuple<FName, UBoxComponent*>& HitBoxPair : HitCharacter->HitCollisionBoxes) {
		if (HitBoxPair.Value != nullptr) {
			const FBoxInformation* BoxValue = Package.HitBoxInfo.Find(HitBoxPair.Key);
			HitBoxPair.Value->SetWorldLocation(BoxValue->Location);
			HitBoxPair.Value->SetWorldRotation(BoxValue->Rotation);
			HitBoxPair.Value->SetBoxExtent(BoxValue->BoxExtent);
		}
	}
}


void ULagCompensationComponent::ResetHitBoxes(AShooterCharacter* HitCharacter, const FFramePackage& Package) {
	if (HitCharacter == nullptr) return;
	for (TTuple<FName, UBoxComponent*>& HitBoxPair : HitCharacter->HitCollisionBoxes) {
		if (HitBoxPair.Value != nullptr) {
			const FBoxInformation* BoxValue = Package.HitBoxInfo.Find(HitBoxPair.Key);
			HitBoxPair.Value->SetWorldLocation(BoxValue->Location);
			HitBoxPair.Value->SetWorldRotation(BoxValue->Rotation);
			HitBoxPair.Value->SetBoxExtent(BoxValue->BoxExtent);
			HitBoxPair.Value->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
}


void ULagCompensationComponent::EnableCharacterMeshCollision(AShooterCharacter* HitCharacter, ECollisionEnabled::Type CollisionEnabled) {
	if (HitCharacter && HitCharacter->GetMesh()) {
		HitCharacter->GetMesh()->SetCollisionEnabled(CollisionEnabled);
	}
}


