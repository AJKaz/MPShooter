==============================
    MPShooter ToDo List
==============================
    
    - [ ] Roll while ADS
    - [ ] Infinite ammo toggle
    - [ ] Slightly decrease jump velocity
    - [ ] 

    - [ ] Add option for static crosshair, just draw a singular image to screen
    - [ ] Portals
    - [ ] Powerups on buttons (+health / ammo)
    - [ ] Golden gun from easter egg? (2 teammates stand on buttons in middle of map for 15 seconds)
    


==============================
    MPShooter Bug List
==============================

    - [x] Killed By Text only displays on server (not replicating for some reason?)
        -> Fix: Used MulticastRPC instead of Rep Notify for killer's name (Rep notify only gets called if variable is set to different value)
    - [x] When holding a weapon and looking right (hip fire), left hand comes off of the left hand socket
        -> Fix: Changed Turning In Place threshold from +- 90 to += 55
    - [x] Being shot while reloading stops reload anim, never exits reload state
        -> Fix: Add notify on hit react montage, set up same as reload end notify
    
