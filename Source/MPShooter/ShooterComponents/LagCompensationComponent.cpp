// Fill out your copyright notice in the Description page of Project Settings.


#include "LagCompensationComponent.h"
#include "MPShooter/Character/ShooterCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "MPShooter/Weapons/Weapon.h"

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
		// Store official server time
		Package.Time = GetWorld()->GetTimeSeconds();
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
	if (Character && Character->Controller && HitCharacter && DamageCauser && DamageCauser->GetDamage() && Confirm.bHitConfirmed) {
		UGameplayStatics::ApplyDamage(
			HitCharacter,
			DamageCauser->GetDamage(),
			Character->Controller,
			DamageCauser,
			UDamageType::StaticClass()
		);
	}
}


FServerSideRewindResult ULagCompensationComponent::ServerSideRewind(AShooterCharacter* HitCharacter, const FVector_NetQuantize& TraceStart, const FVector_NetQuantize& HitLocation, float HitTime) {
	bool bReturn = HitCharacter == nullptr ||
		HitCharacter->GetLagCompensation() == nullptr ||
		HitCharacter->GetLagCompensation()->FrameHistory.GetHead() == nullptr ||
		HitCharacter->GetLagCompensation()->FrameHistory.GetTail() == nullptr;
	if (bReturn) return FServerSideRewindResult();

	// Frame Package to check to verify a hit
	FFramePackage FrameToCheck;
	bool bShouldInterp = true;

	// Hit Character's Frame History
	const TDoubleLinkedList<FFramePackage>& History = HitCharacter->GetLagCompensation()->FrameHistory;
	const float OldestHistoryTime = History.GetTail()->GetValue().Time;
	const float NewestHistoryTime = History.GetHead()->GetValue().Time;
	if (OldestHistoryTime > HitTime) {
		// Too far back, character is too laggy to do server side rewind
		return FServerSideRewindResult();
	}
	if (OldestHistoryTime == HitTime) {
		// Last frame of frame package, this is the frame to save
		FrameToCheck = History.GetTail()->GetValue();
		bShouldInterp = false;
	}
	if (NewestHistoryTime <= HitTime) {
		// Check head's frame package (probably on server, very low ping)
		// Found frame to check
		FrameToCheck = History.GetHead()->GetValue();
		bShouldInterp = false;
	}

	// Don't have frame yet, loop to check frame timing
	TDoubleLinkedList<FFramePackage>::TDoubleLinkedListNode* Younger = History.GetHead();
	TDoubleLinkedList<FFramePackage>::TDoubleLinkedListNode* Older = Younger;
	// Only loop IF we haven't found a frame to check from 2 conditions above
	// Will loop most of the time
	if (bShouldInterp) {
		// Loops as long as older is still younger than the hit time
		while (Older->GetValue().Time > HitTime) {
			// Loop back in time until OlderTime < HitTime < YoungerTime
			if (Older->GetNextNode() == nullptr) break;
			Older = Older->GetNextNode();
			if (Older->GetValue().Time > HitTime) {
				Younger = Older;
			}
		}
	}
	// Highly unlikely, but frame to check is exactly last frame
	if (Older->GetValue().Time == HitTime) {
		FrameToCheck = Older->GetValue();
		bShouldInterp = false;
	}

	// At this point, either we have frame to check or have 2 nodes 1 before and 1 after the frame to check
	if (bShouldInterp) {
		// Interpolate between Younger and Older
		InterpBetweenFrames(Older->GetValue(), Younger->GetValue(), HitTime);
	}

	return ConfirmHit(FrameToCheck, HitCharacter, TraceStart, HitLocation);
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
	if(HitCharacter == nullptr) return FServerSideRewindResult();

	FFramePackage CurrentFrame;
	CacheBoxPositions(HitCharacter, CurrentFrame);
	MoveBoxes(HitCharacter, Package);

	// Disable mesh collision
	EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::NoCollision);

	// Perform line trace & see if we hit head box
	// Enable collision for head first
	UBoxComponent* HeadBox = HitCharacter->HitCollisionBoxes[FName("head")];
	HeadBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	HeadBox->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);

	FHitResult ConfirmHitResult;
	// Make sure to trace thru object to gurantee it hits
	const FVector TraceEnd = TraceStart + (HitLocation - TraceStart) * 1.25f;
	UWorld* World = GetWorld();
	if (World) {
		World->LineTraceSingleByChannel(ConfirmHitResult, TraceStart, TraceEnd, ECollisionChannel::ECC_Visibility);
		if (ConfirmHitResult.bBlockingHit) {
			// Hit head, return early
			ResetHitBoxes(HitCharacter, CurrentFrame);
			// Re-enable mesh collision
			EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics);
			return FServerSideRewindResult{ true, true };
		}
		else {
			// Didn't hit head, check the rest of the body (boxes)
			for (auto& HitBoxPair : HitCharacter->HitCollisionBoxes) {
				if (HitBoxPair.Value != nullptr) {
					// Enable collision
					HitBoxPair.Value->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
					HitBoxPair.Value->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
				}
			}
			World->LineTraceSingleByChannel(ConfirmHitResult, TraceStart, TraceEnd, ECollisionChannel::ECC_Visibility);
			if (ConfirmHitResult.bBlockingHit) {
				// Hit character, but not a headshot
				ResetHitBoxes(HitCharacter, CurrentFrame);
				// Re-enable mesh collision
				EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics);
				return FServerSideRewindResult{ true, false };
			}
		}
	}

	// No confirmed hit, reset boxes and confirm that
	ResetHitBoxes(HitCharacter, CurrentFrame);
	// Re-enable mesh collision
	EnableCharacterMeshCollision(HitCharacter, ECollisionEnabled::QueryAndPhysics);
	return FServerSideRewindResult{ false, false };
}

void ULagCompensationComponent::CacheBoxPositions(AShooterCharacter* HitCharacter, FFramePackage& OutFramePackage) {
	if (HitCharacter == nullptr) return;
	// loop over hit character's collision boxes TMap
	for (auto& HitBoxPair : HitCharacter->HitCollisionBoxes) {
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
	for (auto& HitBoxPair : HitCharacter->HitCollisionBoxes) {
		if (HitBoxPair.Value != nullptr) {
			HitBoxPair.Value->SetWorldLocation(Package.HitBoxInfo[HitBoxPair.Key].Location);
			HitBoxPair.Value->SetWorldRotation(Package.HitBoxInfo[HitBoxPair.Key].Rotation);
			HitBoxPair.Value->SetBoxExtent(Package.HitBoxInfo[HitBoxPair.Key].BoxExtent);
		}
	}
}

void ULagCompensationComponent::ResetHitBoxes(AShooterCharacter* HitCharacter, const FFramePackage& Package) {
	if (HitCharacter == nullptr) return;
	for (auto& HitBoxPair : HitCharacter->HitCollisionBoxes) {
		if (HitBoxPair.Value != nullptr) {
			HitBoxPair.Value->SetWorldLocation(Package.HitBoxInfo[HitBoxPair.Key].Location);
			HitBoxPair.Value->SetWorldRotation(Package.HitBoxInfo[HitBoxPair.Key].Rotation);
			HitBoxPair.Value->SetBoxExtent(Package.HitBoxInfo[HitBoxPair.Key].BoxExtent);
			HitBoxPair.Value->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
}

void ULagCompensationComponent::EnableCharacterMeshCollision(AShooterCharacter* HitCharacter, ECollisionEnabled::Type CollisionEnabled) {
	if (HitCharacter && HitCharacter->GetMesh()) {
		HitCharacter->GetMesh()->SetCollisionEnabled(CollisionEnabled);
	}
}

