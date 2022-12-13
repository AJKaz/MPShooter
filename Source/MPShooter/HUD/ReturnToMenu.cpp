// Fill out your copyright notice in the Description page of Project Settings.


#include "ReturnToMenu.h"
#include "GameFramework/PlayerController.h"
#include "Components/Button.h"
#include "MultiplayerSessionsSubsystem.h"
#include "GameFramework/GameModeBase.h"

void UReturnToMenu::MenuSetup() {
	// Show Widget
	AddToViewport();
	SetVisibility(ESlateVisibility::Visible);
	bIsFocusable = true;

	// Set Input Data
	UWorld* World = GetWorld();
	if (World) {
		PlayerController = PlayerController == nullptr ? World->GetFirstPlayerController() : PlayerController;
		if (PlayerController) {
			FInputModeGameAndUI InputModeData;
			InputModeData.SetWidgetToFocus(TakeWidget());
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(true);
		}
	}

	if (MenuButton && !MenuButton->OnClicked.IsBound()) {
		MenuButton->OnClicked.AddDynamic(this, &UReturnToMenu::MenuButtonClicked);
	}
	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance) {
		MultiplayerSessionsSubsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>();
		if (MultiplayerSessionsSubsystem) {
			MultiplayerSessionsSubsystem->MultiplayerOnDestroySessionComplete.AddDynamic(this, &UReturnToMenu::OnDestroySession);
		}
	}
}

bool UReturnToMenu::Initialize() {
	if (!Super::Initialize()) {
		return false;
	}
	

	return true;
}

void UReturnToMenu::MenuTearDown() {
	// Hide Widget
	RemoveFromParent();

	// Set Input Data
	UWorld* World = GetWorld();
	if (World) {
		PlayerController = PlayerController == nullptr ? World->GetFirstPlayerController() : PlayerController;
		if (PlayerController) {
			FInputModeGameOnly InputModeData;
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(false);
		}
	}
	if (MenuButton && MenuButton->OnClicked.IsBound()) {
		MenuButton->OnClicked.RemoveDynamic(this, &UReturnToMenu::MenuButtonClicked);
	}
	if (MultiplayerSessionsSubsystem && MultiplayerSessionsSubsystem->MultiplayerOnDestroySessionComplete.IsBound()) {
		MultiplayerSessionsSubsystem->MultiplayerOnDestroySessionComplete.RemoveDynamic(this, &UReturnToMenu::OnDestroySession);
	}
}


void UReturnToMenu::MenuButtonClicked() {
	MenuButton->SetIsEnabled(false);
	if (MultiplayerSessionsSubsystem) {
		MultiplayerSessionsSubsystem->DestroySession();
	}
}

void UReturnToMenu::OnDestroySession(bool bWasSuccessful) {
	if (!bWasSuccessful) {
		MenuButton->SetIsEnabled(true);
		return;
	}
	
	UWorld* World = GetWorld();
	if (World) {
		AGameModeBase* GameMode = World->GetAuthGameMode<AGameModeBase>();
		if (GameMode) {
			// On server
			GameMode->ReturnToMainMenuHost();
		}
		else {
			// on client
			PlayerController = PlayerController == nullptr ? World->GetFirstPlayerController() : PlayerController;
			if (PlayerController) {
				PlayerController->ClientReturnToMainMenuWithTextReason(FText());
			}
		}
	}
}