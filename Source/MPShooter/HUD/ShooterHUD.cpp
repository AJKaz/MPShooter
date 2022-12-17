// Fill out your copyright notice in the Description page of Project Settings.


#include "ShooterHUD.h"
#include "GameFramework/PlayerController.h"
#include "CharacterOverlay.h"
#include "Announcement.h"
#include "ElimAnnouncement.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/HorizontalBox.h"
#include "Components/CanvasPanelSlot.h"

void AShooterHUD::BeginPlay() {
	Super::BeginPlay();
}

void AShooterHUD::DrawHUD() {
	Super::DrawHUD();

	// Draw crosshairs on HUD
	FVector2D ViewportSize;
	if (GEngine) {
		GEngine->GameViewport->GetViewportSize(ViewportSize);
		const FVector2D ViewportCenter(ViewportSize.X / 2.f, ViewportSize.Y / 2.f);

		float SpreadScaled = CrosshairSpreadMax * HUDPackage.CrosshairSpread;

		// If crosshair texture is valid, draw it:
		if (HUDPackage.CrosshairsCenter) {
			DrawCrosshair(HUDPackage.CrosshairsCenter, ViewportCenter, FVector2D(0.f, 0.f), HUDPackage.CrosshairsColor);
		}
		if (HUDPackage.CrosshairsLeft) {
			FVector2D Spread(-SpreadScaled, 0.f);
			DrawCrosshair(HUDPackage.CrosshairsLeft, ViewportCenter, Spread, HUDPackage.CrosshairsColor);
		}
		if (HUDPackage.CrosshairsRight) {
			FVector2D Spread(SpreadScaled, 0.f);
			DrawCrosshair(HUDPackage.CrosshairsRight, ViewportCenter, Spread, HUDPackage.CrosshairsColor);
		}
		if (HUDPackage.CrosshairsTop) {
			FVector2D Spread(0.f, -SpreadScaled);
			DrawCrosshair(HUDPackage.CrosshairsTop, ViewportCenter, Spread, HUDPackage.CrosshairsColor);
		}
		if (HUDPackage.CrosshairsBottom) {
			FVector2D Spread(0.f, SpreadScaled);
			DrawCrosshair(HUDPackage.CrosshairsBottom, ViewportCenter, Spread, HUDPackage.CrosshairsColor);
		}

	}

}

void AShooterHUD::AddCharacterOverlay() {
	APlayerController* PlayerController = GetOwningPlayerController();
	if (PlayerController && CharacterOverlayClass) {
		CharacterOverlay = CreateWidget<UCharacterOverlay>(PlayerController, CharacterOverlayClass);
		CharacterOverlay->AddToViewport();
	}
}

void AShooterHUD::AddAnnouncement() {
	APlayerController* PlayerController = GetOwningPlayerController();
	if (PlayerController && AnnouncementClass && Announcement == nullptr) {
		Announcement = CreateWidget<UAnnouncement>(PlayerController, AnnouncementClass);
		Announcement->AddToViewport();
	}
}

void AShooterHUD::AddElimAnnouncement(FString Attacker, FString Victim) {
	OwningPlayer = OwningPlayer == nullptr ? GetOwningPlayerController() : OwningPlayer;
	if (OwningPlayer && ElimAnnouncementClass) {
		// Create & add elim announcement
		UElimAnnouncement* ElimAnnouncementWidget = CreateWidget<UElimAnnouncement>(OwningPlayer, ElimAnnouncementClass);
		if (ElimAnnouncementWidget) {
			ElimAnnouncementWidget->SetElimAnnouncementText(Attacker, Victim);
			ElimAnnouncementWidget->AddToViewport();

			// move old kill messages up
			for (UElimAnnouncement* Msg : ElimMessages) {
				if (Msg && Msg->AnnouncementBox) {
					UCanvasPanelSlot* CanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(Msg->AnnouncementBox);
					if (CanvasSlot) {						
						FVector2D NewPosition(CanvasSlot->GetPosition().X, CanvasSlot->GetPosition().Y - CanvasSlot->GetSize().Y);
						CanvasSlot->SetPosition(NewPosition);
					}
				}
			}
			ElimMessages.Add(ElimAnnouncementWidget);
			//UE_LOG(LogTemp, Warning, TEXT("%d"), ElimMessages.Num());
			
			// setup timer
			FTimerHandle ElimMsgTimer;
			FTimerDelegate ElimMsgDelegate;
			ElimMsgDelegate.BindUFunction(this, FName("ElimAnnouncementTimerFinished"), ElimAnnouncementWidget);
			GetWorldTimerManager().SetTimer(ElimMsgTimer, ElimMsgDelegate, ElimAnnouncementTime, false);
		}
	}
}

void AShooterHUD::ElimAnnouncementTimerFinished(UElimAnnouncement* MsgToRemove) {
	if (MsgToRemove) {
		ElimMessages.RemoveAt(ElimMessages.Num() - 1);
		MsgToRemove->RemoveFromParent();
	}
}

void AShooterHUD::DrawCrosshair(UTexture2D* Texture, FVector2D Center, FVector2D Spread, FLinearColor CrosshairColor) {
	const float TextureWidth = Texture->GetSizeX();
	const float TextureHeight = Texture->GetSizeY();
	const FVector2D TextureDrawPoint(Center.X - (TextureWidth / 2.f) + Spread.X, Center.Y - (TextureHeight / 2.f) + Spread.Y);
	DrawTexture(
		Texture,
		TextureDrawPoint.X,
		TextureDrawPoint.Y,
		TextureWidth,
		TextureHeight,
		0.f,
		0.f,
		1.f,
		1.f,
		CrosshairColor
	);

}


