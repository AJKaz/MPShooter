// Fill out your copyright notice in the Description page of Project Settings.


#include "LagCompensationComponent.h"
#include "MPShooter/Character/ShooterCharacter.h"
#include "Components/BoxComponent.h"
#include "DrawDebugHelpers.h"

ULagCompensationComponent::ULagCompensationComponent() {
	PrimaryComponentTick.bCanEverTick = true;

}


void ULagCompensationComponent::BeginPlay() {
	Super::BeginPlay();
	FFramePackage Package;
	SaveFramePackage(Package);
	ShowFramePackage(Package, FColor::Orange);
}

void ULagCompensationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
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
		DrawDebugBox(GetWorld(), BoxInfo.Value.Location, BoxInfo.Value.BoxExtent, FQuat(BoxInfo.Value.Rotation), Color, true);
	}
}