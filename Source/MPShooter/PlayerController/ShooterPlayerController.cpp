// Fill out your copyright notice in the Description page of Project Settings.


#include "ShooterPlayerController.h"
#include "MPShooter/HUD/ShooterHUD.h"
#include "MPShooter/HUD/CharacterOverlay.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "MPShooter/Character/ShooterCharacter.h"
#include "Net/UnrealNetwork.h"
#include "MPShooter/GameMode/ShooterGameMode.h"
#include "MPShooter/HUD/Announcement.h"
#include "Kismet/GameplayStatics.h"
#include "MPShooter/ShooterComponents/CombatComponent.h"
#include "MPShooter/GameState/ShooterGameState.h"
#include "MPShooter/PlayerState/ShooterPlayerState.h"
#include "Components/Image.h"
#include "MPShooter/HUD/ReturnToMenu.h"

void AShooterPlayerController::BeginPlay() {
	Super::BeginPlay();

	ShooterHUD = Cast<AShooterHUD>(GetHUD());
	ServerCheckMatchState();
}

void AShooterPlayerController::SetupInputComponent() {
	Super::SetupInputComponent();
	if (InputComponent == nullptr) return;
	InputComponent->BindAction("Quit", IE_Pressed, this, &AShooterPlayerController::ShowReturnToMenu);
}


void AShooterPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AShooterPlayerController, MatchState);
	DOREPLIFETIME(AShooterPlayerController, bShowTeamScores);
}


void AShooterPlayerController::Tick(float DeltaTime) {
	Super::Tick(DeltaTime);

	SetHUDTime();
	CheckTimeSync(DeltaTime);
	PollInit();
	CheckPing(DeltaTime);
}

void AShooterPlayerController::ShowReturnToMenu() {
	if (ReturnToMenuWidget == nullptr) return;
	if (ReturnToMenu == nullptr) {
		ReturnToMenu = CreateWidget<UReturnToMenu>(this, ReturnToMenuWidget);
	}
	if (ReturnToMenu) {
		bReturnToMenuOpen = !bReturnToMenuOpen;
		if (bReturnToMenuOpen) {
			ReturnToMenu->MenuSetup();
		}
		else {
			ReturnToMenu->MenuTearDown();
		}
	}
}

void AShooterPlayerController::OnRep_ShowTeamScores() {
	if (bShowTeamScores) {
		InitTeamScores();
	}
	else {
		HideTeamScores();
	}
}

void AShooterPlayerController::HideTeamScores() {
	ShooterHUD = ShooterHUD == nullptr ? Cast<AShooterHUD>(GetHUD()) : ShooterHUD;
	if (ShooterHUD && ShooterHUD->CharacterOverlay && ShooterHUD->CharacterOverlay->RedTeamScore && ShooterHUD->CharacterOverlay->BlueTeamScore && ShooterHUD->CharacterOverlay->ScoreSpacer) {
		ShooterHUD->CharacterOverlay->RedTeamScore->SetText(FText());
		ShooterHUD->CharacterOverlay->BlueTeamScore->SetText(FText());
		ShooterHUD->CharacterOverlay->ScoreSpacer->SetText(FText());
	}
}

void AShooterPlayerController::InitTeamScores() {
	ShooterHUD = ShooterHUD == nullptr ? Cast<AShooterHUD>(GetHUD()) : ShooterHUD;
	if (ShooterHUD && ShooterHUD->CharacterOverlay && ShooterHUD->CharacterOverlay->RedTeamScore && ShooterHUD->CharacterOverlay->BlueTeamScore && ShooterHUD->CharacterOverlay->ScoreSpacer) {
		FString Zero("0");
		FString Spacer("|");
		ShooterHUD->CharacterOverlay->RedTeamScore->SetText(FText::FromString(Zero));
		ShooterHUD->CharacterOverlay->BlueTeamScore->SetText(FText::FromString(Zero));
		ShooterHUD->CharacterOverlay->ScoreSpacer->SetText(FText::FromString(Spacer));
	}
}

void AShooterPlayerController::SetHUDRedTeamScore(int32 RedScore) {
	ShooterHUD = ShooterHUD == nullptr ? Cast<AShooterHUD>(GetHUD()) : ShooterHUD;
	if (ShooterHUD && ShooterHUD->CharacterOverlay && ShooterHUD->CharacterOverlay->RedTeamScore) {
		FString ScoreText = FString::Printf(TEXT("%d"), RedScore);
		ShooterHUD->CharacterOverlay->RedTeamScore->SetText(FText::FromString(ScoreText));
	}
}

void AShooterPlayerController::SetHUDBlueTeamScore(int32 BlueScore) {
	ShooterHUD = ShooterHUD == nullptr ? Cast<AShooterHUD>(GetHUD()) : ShooterHUD;
	if (ShooterHUD && ShooterHUD->CharacterOverlay && ShooterHUD->CharacterOverlay->BlueTeamScore) {
		FString ScoreText = FString::Printf(TEXT("%d"), BlueScore);
		ShooterHUD->CharacterOverlay->BlueTeamScore->SetText(FText::FromString(ScoreText));
	}
}

void AShooterPlayerController::SetHUDPing(uint8 Ping) {
	ShooterHUD = ShooterHUD == nullptr ? Cast<AShooterHUD>(GetHUD()) : ShooterHUD;
	if (ShooterHUD && ShooterHUD->CharacterOverlay && ShooterHUD->CharacterOverlay->PingText) {
		FString PingText = FString::Printf(TEXT("%d ms"), Ping);
		ShooterHUD->CharacterOverlay->PingText->SetText(FText::FromString(PingText));
	}
}

void AShooterPlayerController::CheckPing(float DeltaTime) {
	HighPingRunningTime += DeltaTime;
	if (HighPingRunningTime > CheckPingFrequency) {
		// Check player's ping
		PlayerState = PlayerState == nullptr ? GetPlayerState<APlayerState>() : PlayerState;
		if (PlayerState) {
			//UE_LOG(LogTemp, Warning, TEXT("Ping: %d"), PlayerState->GetPing() * 4);
			// Ping is compressed and divided by 4
			SetHUDPing(PlayerState->GetPing() * 4);
			if (PlayerState->GetPing() * 4 > HighPingThreshold) {
				HighPingWarning();
				PingAnimationRunningTime = 0.f;
				ServerReportPingStatus(true);
			}
			else {
				ServerReportPingStatus(false);
			}
		}
		HighPingRunningTime = 0.f;
	}
	// Stops High Ping Warning Animation after (HighPingDuration) seconds
	if (ShooterHUD && ShooterHUD->CharacterOverlay && ShooterHUD->CharacterOverlay->HighPingAnimation && ShooterHUD->CharacterOverlay->IsAnimationPlaying(ShooterHUD->CharacterOverlay->HighPingAnimation)) {
		PingAnimationRunningTime += DeltaTime;
		if (PingAnimationRunningTime > HighPingDuration) {
			StopHighPingWarning();
		}
	}
}


void AShooterPlayerController::ServerReportPingStatus_Implementation(bool bHighPing) {
	// Reports if ping is too high
	HighPingDelegate.Broadcast(bHighPing);
}

void AShooterPlayerController::CheckTimeSync(float DeltaTime) {
	TimeSyncRunningTime += DeltaTime;
	if (IsLocalController() && TimeSyncRunningTime > TimeSyncFrequency) {
		ServerRequestServerTime(GetWorld()->GetTimeSeconds());
		TimeSyncRunningTime = 0.f;
	}
}

void AShooterPlayerController::ServerCheckMatchState_Implementation() {
	AShooterGameMode* GameMode = Cast<AShooterGameMode>(UGameplayStatics::GetGameMode(this));
	if (GameMode) {
		// Set MatchState and Game Times
		WarmupTime = GameMode->WarmupTime;
		MatchTime = GameMode->MatchTime;
		LevelStartingTime = GameMode->LevelStartingTime;
		CooldownTime = GameMode->CooldownTime;
		MatchState = GameMode->GetMatchState();

		ClientJoinMidgame(MatchState, WarmupTime, MatchTime, LevelStartingTime, CooldownTime);

		if (ShooterHUD && MatchState == MatchState::WaitingToStart) {
			ShooterHUD->AddAnnouncement();
		}
	}
}

void AShooterPlayerController::BroadcastElim(APlayerState* Attacker, APlayerState* Victim) {
	ClientElimAnnouncement(Attacker, Victim);
}

void AShooterPlayerController::ClientElimAnnouncement_Implementation(APlayerState* Attacker, APlayerState* Victim) {
	APlayerState* Self = GetPlayerState<APlayerState>();
	if (Attacker && Victim && Self) {
		ShooterHUD = ShooterHUD == nullptr ? Cast<AShooterHUD>(GetHUD()) : ShooterHUD;
		if (ShooterHUD) {
			if (Attacker == Self && Victim != Self) {
				// You killed someone and it's not a suicide
				ShooterHUD->AddElimAnnouncement("You", Victim->GetPlayerName());
				return;
			}
			if (Victim == Self && Attacker != Self) {
				// someone else killed you
				ShooterHUD->AddElimAnnouncement(Attacker->GetPlayerName(), "You");
				return;
			}
			if (Attacker == Victim && Attacker == Self) {
				// killed yourself
				ShooterHUD->AddElimAnnouncement("You", "Yourself");
				return;
			}
			if (Attacker == Victim && Attacker != Self) {
				// someone else killed themselves
				ShooterHUD->AddElimAnnouncement(Attacker->GetPlayerName(), "Themself");
				return;
			}
			// someone killed someone else 
			ShooterHUD->AddElimAnnouncement(Attacker->GetPlayerName(), Victim->GetPlayerName());
		}
	}
}

void AShooterPlayerController::ClientJoinMidgame_Implementation(FName StateOfMatch, float Warmup, float Match, float StartingTime, float Cooldown) {
	// Set MatchState and Game Times
	MatchState = StateOfMatch;
	WarmupTime = Warmup;
	MatchTime = Match;
	LevelStartingTime = StartingTime;
	CooldownTime = Cooldown;

	OnMatchStateSet(MatchState);
	if (ShooterHUD && MatchState == MatchState::WaitingToStart) {
		ShooterHUD->AddAnnouncement();
	}
}

void AShooterPlayerController::SetHUDHealth(float Health, float MaxHealth) {
	ShooterHUD = ShooterHUD == nullptr ? Cast<AShooterHUD>(GetHUD()) : ShooterHUD;
	if (ShooterHUD && ShooterHUD->CharacterOverlay && ShooterHUD->CharacterOverlay->HealthBar && ShooterHUD->CharacterOverlay->HealthText) {
		const float HealthPercent = Health / MaxHealth;
		ShooterHUD->CharacterOverlay->HealthBar->SetPercent(HealthPercent);
		// Round float health values up to int values and set text
		FString HealthText = FString::Printf(TEXT("%d/%d"), FMath::CeilToInt(Health), FMath::CeilToInt(MaxHealth));
		ShooterHUD->CharacterOverlay->HealthText->SetText(FText::FromString(HealthText));
	}
	else {
		bInitializeHealth = true;
		HUDHealth = Health;
		HUDMaxHealth = MaxHealth;
	}
}

void AShooterPlayerController::SetHUDShield(float Shield, float MaxShield) {
	ShooterHUD = ShooterHUD == nullptr ? Cast<AShooterHUD>(GetHUD()) : ShooterHUD;
	if (ShooterHUD && ShooterHUD->CharacterOverlay && ShooterHUD->CharacterOverlay->ShieldBar) {
		const float ShieldPercent = Shield / MaxShield;
		ShooterHUD->CharacterOverlay->ShieldBar->SetPercent(ShieldPercent);
		//// Round float shield values up to int values and set text
		//FString ShieldText = FString::Printf(TEXT("%d/%d"), FMath::CeilToInt(Shield), FMath::CeilToInt(MaxShield));
		//ShooterHUD->CharacterOverlay->ShieldText->SetText(FText::FromString(ShieldText));
	}
	else {
		bInitializeShield = true;
		HUDShield = Shield;
		HUDMaxShield = MaxShield;
	}
}

void AShooterPlayerController::SetHUDScore(float Score) {
	ShooterHUD = ShooterHUD == nullptr ? Cast<AShooterHUD>(GetHUD()) : ShooterHUD;
	if (ShooterHUD && ShooterHUD->CharacterOverlay && ShooterHUD->CharacterOverlay->ScoreAmount) {
		FString ScoreText = FString::Printf(TEXT("%d"), FMath::FloorToInt(Score));
		ShooterHUD->CharacterOverlay->ScoreAmount->SetText(FText::FromString(ScoreText));
	}
	else {
		bInitializeScore = true;
		HUDScore = Score;
	}
}

void AShooterPlayerController::SetHUDDeaths(int32 Deaths) {
	ShooterHUD = ShooterHUD == nullptr ? Cast<AShooterHUD>(GetHUD()) : ShooterHUD;
	if (ShooterHUD && ShooterHUD->CharacterOverlay && ShooterHUD->CharacterOverlay->DeathsAmount) {
		FString DeathsText = FString::Printf(TEXT("%d"), Deaths);
		ShooterHUD->CharacterOverlay->DeathsAmount->SetText(FText::FromString(DeathsText));
	}
	else {
		bInitializeDeaths = true;
		HUDDeaths = Deaths;
	}
}

void AShooterPlayerController::DisplayDeathMessage(const FString KilledBy) {
	ShooterHUD = ShooterHUD == nullptr ? Cast<AShooterHUD>(GetHUD()) : ShooterHUD;
	if (ShooterHUD &&
		ShooterHUD->CharacterOverlay &&
		ShooterHUD->CharacterOverlay->DeathMessage &&
		ShooterHUD->CharacterOverlay->KilledBy) {
		ShooterHUD->CharacterOverlay->KilledBy->SetText(FText::FromString(KilledBy));
		ShooterHUD->CharacterOverlay->KilledBy->SetVisibility(ESlateVisibility::Visible);
		ShooterHUD->CharacterOverlay->DeathMessage->SetVisibility(ESlateVisibility::Visible);
	}
}

void AShooterPlayerController::HideDeathMessage() {
	ShooterHUD = ShooterHUD == nullptr ? Cast<AShooterHUD>(GetHUD()) : ShooterHUD;
	if (ShooterHUD &&
		ShooterHUD->CharacterOverlay &&
		ShooterHUD->CharacterOverlay->DeathMessage &&
		ShooterHUD->CharacterOverlay->KilledBy) {
		ShooterHUD->CharacterOverlay->KilledBy->SetVisibility(ESlateVisibility::Collapsed);
		ShooterHUD->CharacterOverlay->DeathMessage->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void AShooterPlayerController::SetHUDWeaponAmmo(int32 Ammo) {
	ShooterHUD = ShooterHUD == nullptr ? Cast<AShooterHUD>(GetHUD()) : ShooterHUD;
	if (ShooterHUD && ShooterHUD->CharacterOverlay && ShooterHUD->CharacterOverlay->WeaponAmmoAmount) {
		FString AmmoText = FString::Printf(TEXT("%d"), Ammo);
		ShooterHUD->CharacterOverlay->WeaponAmmoAmount->SetText(FText::FromString(AmmoText));
	}
	else {
		bInitializeWeaponAmmo = true;
		HUDWeaponAmmo = Ammo;
	}
}

void AShooterPlayerController::SetHUDCarriedAmmo(int32 Ammo) {
	ShooterHUD = ShooterHUD == nullptr ? Cast<AShooterHUD>(GetHUD()) : ShooterHUD;
	if (ShooterHUD && ShooterHUD->CharacterOverlay && ShooterHUD->CharacterOverlay->CarriedAmmoAmount) {
		FString AmmoText = FString::Printf(TEXT("%d"), Ammo);
		ShooterHUD->CharacterOverlay->CarriedAmmoAmount->SetText(FText::FromString(AmmoText));
	}
	else {
		bInitializeCarriedAmmo = true;
		HUDCarriedAmmo = Ammo;
	}
}

void AShooterPlayerController::SetHUDMatchCountdown(float CountdownTime) {
	ShooterHUD = ShooterHUD == nullptr ? Cast<AShooterHUD>(GetHUD()) : ShooterHUD;
	if (ShooterHUD && ShooterHUD->CharacterOverlay && ShooterHUD->CharacterOverlay->MatchCountdownText) {
		if (CountdownTime < 0.f) {
			ShooterHUD->CharacterOverlay->MatchCountdownText->SetText(FText());
			return;
		}
		int32 Minutes = FMath::FloorToInt(CountdownTime / 60.f);
		int32 Seconds = CountdownTime - Minutes * 60;
		FString CountdownText = "";
		if (Minutes < 10) {
			CountdownText = FString::Printf(TEXT("%01d:%02d"), Minutes, Seconds);
		}
		else {
			CountdownText = FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
		}

		ShooterHUD->CharacterOverlay->MatchCountdownText->SetText(FText::FromString(CountdownText));
	}
}

void AShooterPlayerController::SetHUDAnnouncementCountdown(float CountdownTime) {
	ShooterHUD = ShooterHUD == nullptr ? Cast<AShooterHUD>(GetHUD()) : ShooterHUD;
	if (ShooterHUD && ShooterHUD->Announcement && ShooterHUD->Announcement->WarmupTime) {
		if (CountdownTime < 0.f) {
			ShooterHUD->Announcement->WarmupTime->SetText(FText());
			return;
		}
		int32 Minutes = FMath::FloorToInt(CountdownTime / 60.f);
		int32 Seconds = CountdownTime - Minutes * 60;
		FString CountdownText = "";
		if (Minutes < 10) {
			CountdownText = FString::Printf(TEXT("%01d:%02d"), Minutes, Seconds);
		}
		else {
			CountdownText = FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds);
		}

		ShooterHUD->Announcement->WarmupTime->SetText(FText::FromString(CountdownText));
	}
}

void AShooterPlayerController::SetHUDGrenades(int32 Grenades) {
	ShooterHUD = ShooterHUD == nullptr ? Cast<AShooterHUD>(GetHUD()) : ShooterHUD;
	if (ShooterHUD && ShooterHUD->CharacterOverlay && ShooterHUD->CharacterOverlay->GrenadesText) {
		FString GrenadesText = FString::Printf(TEXT("%d"), Grenades);
		ShooterHUD->CharacterOverlay->GrenadesText->SetText(FText::FromString(GrenadesText));
	}
	else {
		HUDGrenades = Grenades;
		bInitializeGrenades = true;
	}
}

void AShooterPlayerController::HighPingWarning() {
	ShooterHUD = ShooterHUD == nullptr ? Cast<AShooterHUD>(GetHUD()) : ShooterHUD;
	if (ShooterHUD && ShooterHUD->CharacterOverlay && ShooterHUD->CharacterOverlay->HighPingImage && ShooterHUD->CharacterOverlay->HighPingAnimation) {
		// Show high ping image
		ShooterHUD->CharacterOverlay->HighPingImage->SetOpacity(1.f);
		// Play animation
		ShooterHUD->CharacterOverlay->PlayAnimation(ShooterHUD->CharacterOverlay->HighPingAnimation, 0.f, 5);
	}
}

void AShooterPlayerController::StopHighPingWarning() {
	ShooterHUD = ShooterHUD == nullptr ? Cast<AShooterHUD>(GetHUD()) : ShooterHUD;
	if (ShooterHUD && ShooterHUD->CharacterOverlay && ShooterHUD->CharacterOverlay->HighPingImage && ShooterHUD->CharacterOverlay->HighPingAnimation) {
		// Hide high ping image
		ShooterHUD->CharacterOverlay->HighPingImage->SetOpacity(0.f);
		// Stop animation
		if (ShooterHUD->CharacterOverlay->IsAnimationPlaying(ShooterHUD->CharacterOverlay->HighPingAnimation)) {
			ShooterHUD->CharacterOverlay->StopAnimation(ShooterHUD->CharacterOverlay->HighPingAnimation);
		}
	}
}

void AShooterPlayerController::OnPossess(APawn* InPawn) {
	Super::OnPossess(InPawn);

	AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(InPawn);
	if (ShooterCharacter) {
		SetHUDHealth(ShooterCharacter->GetHealth(), ShooterCharacter->GetMaxHealth());
		SetHUDShield(ShooterCharacter->GetShield(), ShooterCharacter->GetMaxShield());
		ShooterCharacter->UpdateHUDAmmo();
	}
	HideDeathMessage();
}


void AShooterPlayerController::SetHUDTime() {
	float TimeLeft = 0.f;
	if (MatchState == MatchState::WaitingToStart) TimeLeft = WarmupTime - GetServerTime() + LevelStartingTime;
	else if (MatchState == MatchState::InProgress) TimeLeft = WarmupTime + MatchTime - GetServerTime() + LevelStartingTime;
	else if (MatchState == MatchState::Cooldown) TimeLeft = CooldownTime + WarmupTime + MatchTime - GetServerTime() + LevelStartingTime;

	uint32 SecondsLeft = FMath::CeilToInt(TimeLeft);

	if (HasAuthority()) {
		ShooterGameMode = ShooterGameMode == nullptr ? Cast<AShooterGameMode>(UGameplayStatics::GetGameMode(this)) : ShooterGameMode;
		if (ShooterGameMode) {
			SecondsLeft = FMath::CeilToInt(ShooterGameMode->GetCountdownTime() + LevelStartingTime);
		}
	}

	if (CountdownInt != SecondsLeft) {
		if (MatchState == MatchState::WaitingToStart || MatchState == MatchState::Cooldown) {
			SetHUDAnnouncementCountdown(TimeLeft);
		}
		else if (MatchState == MatchState::InProgress) {
			SetHUDMatchCountdown(TimeLeft);
		}
	}
	CountdownInt = SecondsLeft;
}

void AShooterPlayerController::ServerRequestServerTime_Implementation(float TimeOfClientRequest) {
	float ServerTimeOfReceipt = GetWorld()->GetTimeSeconds();
	ClientReportServerTime(TimeOfClientRequest, ServerTimeOfReceipt);
}

void AShooterPlayerController::ClientReportServerTime_Implementation(float TimeOfClientRequest, float TimeServerReceivedClientRequest) {
	float RoundTripTime = GetWorld()->GetTimeSeconds() - TimeOfClientRequest;
	SingleTripTime = 0.5f * RoundTripTime;
	float CurrentServerTime = TimeServerReceivedClientRequest + SingleTripTime;
	ClientServerDelta = CurrentServerTime - GetWorld()->GetTimeSeconds();
}

float AShooterPlayerController::GetServerTime() {
	if (HasAuthority()) return GetWorld()->GetTimeSeconds();
	else return GetWorld()->GetTimeSeconds() + ClientServerDelta;
}

void AShooterPlayerController::ReceivedPlayer() {
	Super::ReceivedPlayer();
	if (IsLocalController()) {
		ServerRequestServerTime(GetWorld()->GetTimeSeconds());
	}
}

void AShooterPlayerController::PollInit() {
	if (CharacterOverlay == nullptr) {
		if (ShooterHUD && ShooterHUD->CharacterOverlay) {
			CharacterOverlay = ShooterHUD->CharacterOverlay;
			if (CharacterOverlay) {
				if (bInitializeHealth) SetHUDHealth(HUDHealth, HUDMaxHealth);
				if (bInitializeShield) SetHUDShield(HUDShield, HUDMaxShield);
				if (bInitializeScore) SetHUDScore(HUDScore);
				if (bInitializeDeaths) SetHUDDeaths(HUDDeaths);
				if (bInitializeCarriedAmmo) SetHUDCarriedAmmo(HUDCarriedAmmo);
				if (bInitializeWeaponAmmo) SetHUDWeaponAmmo(HUDWeaponAmmo);
				AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(GetPawn());
				if (ShooterCharacter && ShooterCharacter->GetCombat()) {
					if (bInitializeGrenades) SetHUDGrenades(ShooterCharacter->GetCombat()->GetGrenades());
				}
			}
		}
	}
}

void AShooterPlayerController::OnMatchStateSet(FName State, bool bTeamsMatch) {
	MatchState = State;
	if (MatchState == MatchState::InProgress) {
		HandleMatchHasStarted(bTeamsMatch);
	}
	else if (MatchState == MatchState::Cooldown) {
		HandleCooldown();
	}
}

void AShooterPlayerController::OnRep_MatchState() {
	if (MatchState == MatchState::InProgress) {
		HandleMatchHasStarted();
	}
	else if (MatchState == MatchState::Cooldown) {
		HandleCooldown();
	}
}

void AShooterPlayerController::HandleMatchHasStarted(bool bTeamsMatch) {
	if (HasAuthority()) bShowTeamScores = bTeamsMatch;
	// Adds character overlay to hud
	ShooterHUD = ShooterHUD == nullptr ? Cast<AShooterHUD>(GetHUD()) : ShooterHUD;
	if (ShooterHUD) {
		if (ShooterHUD->CharacterOverlay == nullptr) ShooterHUD->AddCharacterOverlay();
		if (ShooterHUD->Announcement) {
			ShooterHUD->Announcement->SetVisibility(ESlateVisibility::Hidden);
		}
		if (!HasAuthority()) return;
		if (bTeamsMatch) {
			InitTeamScores();
		}
		else {
			HideTeamScores();
		}
	}
}

void AShooterPlayerController::HandleCooldown() {
	// Hide player overlay and show Announcement Widget
	ShooterHUD = ShooterHUD == nullptr ? Cast<AShooterHUD>(GetHUD()) : ShooterHUD;
	if (ShooterHUD) {
		ShooterHUD->CharacterOverlay->RemoveFromParent();
		if (ShooterHUD->Announcement && ShooterHUD->Announcement->AnnouncementText && ShooterHUD->Announcement->InfoText) {
			ShooterHUD->Announcement->SetVisibility(ESlateVisibility::Visible);
			FString AnnouncementText("New Match Starts In:");
			ShooterHUD->Announcement->AnnouncementText->SetText(FText::FromString(AnnouncementText));

			// Display Winner
			AShooterGameState* ShooterGameState = Cast<AShooterGameState>(UGameplayStatics::GetGameState(this));
			AShooterPlayerState* ShooterPlayerState = GetPlayerState<AShooterPlayerState>();
			if (ShooterGameState && ShooterPlayerState) {
				TArray<AShooterPlayerState*> TopPlayers = ShooterGameState->TopScoringPlayers;
				FString InfoTextString;
				if (TopPlayers.Num() == 0) {
					InfoTextString = FString("There is no winner. You suck.");
				}
				else if (TopPlayers.Num() == 1 && TopPlayers[0] == ShooterPlayerState) {
					InfoTextString = FString("You are the winner!");
				}
				else if (TopPlayers.Num() == 1) {
					InfoTextString = FString::Printf(TEXT("Winner: %s"), *TopPlayers[0]->GetPlayerName());
				}
				else if (TopPlayers.Num() > 1) {
					InfoTextString = FString("Players tied for win: \n");
					for (auto TiedPlayer : TopPlayers) {
						InfoTextString.Append(FString::Printf(TEXT("%s\n"), *TiedPlayer->GetPlayerName()));
					}
				}


				ShooterHUD->Announcement->InfoText->SetText(FText::FromString(InfoTextString));
			}


		}
	}
	// Disable input
	AShooterCharacter* ShooterCharacter = Cast<AShooterCharacter>(GetPawn());
	if (ShooterCharacter && ShooterCharacter->GetCombat()) {
		ShooterCharacter->bDisableGameplay = true;
		ShooterCharacter->GetCombat()->FireButtonPressed(false);
	}
}
