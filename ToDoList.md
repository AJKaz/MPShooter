==============================
    MPShooter ToDo List
==============================
    
    
    - [ ] Move AddToDeaths() inside of the MulticastRPC for DeathMessage
    - [x] Slightly decrease jump velocity
    - [x] Hide local mesh for player when ADS with sniper

    - [ ] Roll while ADS
    - [ ] Infinite MAG ammo toggle
    - [x] Infinite ammo toggle    
    - [ ] Add Player name above players on ADS
        -> FString PlayerName = Pawn->GetPlayerState()->GetPlayerName();    

    - [ ] Add option for static crosshair
    - [ ] Portals
    - [ ] Golden gun from easter egg? (2 teammates stand on buttons in middle of map for 15 seconds)
    
    - [x] Powerups on buttons (+health / ammo / speed / jumpboost)
    - [x] Create death barrier for falling off map

 ==============================
    Sound Tweaking
==============================

    - [x] Tweak Rocket Launcher Sounds
    - [x] Increase Shotgun Shoot Sound
        Sound Cue Volume: 1.0 -> 1.5

==============================
    MPShooter Bug List
==============================

    - [x] Killed By Text only displays on server (not replicating for some reason?)
        -> Fix: Used MulticastRPC instead of Rep Notify for killer's name (Rep notify only gets called if variable is set to different value)
    - [x] When holding a weapon and looking right (hip fire), left hand comes off of the left hand socket
        -> Fix: Changed Turning In Place threshold from +- 90 to += 55
    - [x] Being shot while reloading stops reload anim, never exits reload state
        -> Fix: Add notify on hit react montage, set up same as reload end notify
    
