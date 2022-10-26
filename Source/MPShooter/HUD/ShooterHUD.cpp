// Fill out your copyright notice in the Description page of Project Settings.


#include "ShooterHUD.h"

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
			DrawCrosshair(HUDPackage.CrosshairsCenter, ViewportCenter, FVector2D(0.f, 0.f));
		}
		if (HUDPackage.CrosshairsLeft) {
			FVector2D Spread(-SpreadScaled, 0.f);
			DrawCrosshair(HUDPackage.CrosshairsLeft, ViewportCenter, Spread);
		}
		if (HUDPackage.CrosshairsRight) {
			FVector2D Spread(SpreadScaled, 0.f);
			DrawCrosshair(HUDPackage.CrosshairsRight, ViewportCenter, Spread);
		}
		if (HUDPackage.CrosshairsTop) {
			FVector2D Spread(0.f, -SpreadScaled);
			DrawCrosshair(HUDPackage.CrosshairsTop, ViewportCenter, Spread);
		}
		if (HUDPackage.CrosshairsBottom) {
			FVector2D Spread(0.f, SpreadScaled);
			DrawCrosshair(HUDPackage.CrosshairsBottom, ViewportCenter, Spread);
		}

	}

}

void AShooterHUD::DrawCrosshair(UTexture2D* Texture, FVector2D Center, FVector2D Spread) {
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
		FLinearColor::White
	);

}
