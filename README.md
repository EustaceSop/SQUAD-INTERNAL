# Squad Internal (UE 5.7 / v10.5.1)

Internal DLL cheat for **Squad v10.5.1 (UE 5.7.4)**, built on a rewritten UE5 internal base with offsets from a live Dumper-7 SDK dump. Features a full ESP suite, aimbot, per-category material chams, and a clean one-key unhook.

## Features

**Player ESP**
- Bone-projected boxes (22-bone minmax, head gap + padding), skeleton, health bar
- Name + K/D, **[ADMIN] / [CMD] / (DOWN) markers**, role/kit, stance (prone/crouch/sprint/ADS)
- Enemy weapon name + ammo count, tickets HUD
- Bots supported (structural probe, anti-flicker bone snapshots)

**Aimbot**
- Direct `ControlRotation` write with UE-correct angle normalization (pole-flip safe)
- Target stickiness, per-frame turn cap, dead zone, FOV circle
- Bone select (head/neck/chest), velocity + muzzle-velocity prediction

**World ESP**
- Vehicles: oriented 3D wire box (near-plane clipped), health, seat occupancy, weakpoints (engine/track/ammo/turret)
- Deployables / FOBs (unbuilt, sieged), enemy mine/IED proximity warnings
- Capture point HUD, grenade fuse warnings, rotating radar
- Distance-based declutter + label de-overlap

**Chams (through-wall materials)**
- Separate material selection for **Players / Vehicles / Deployables**
- Curated safe whitelist + full-scan list for testing
- **Crash auto-blacklist**: last-tested material is persisted; if the game dies, it's excluded next session
- Game-thread-safe restore on toggle/unhook

**Misc**
- ZeroGUI canvas menu (INSERT), FOV circle, config auto-save (`%TEMP%\squad_int_cfg.txt`)
- Debug log at `%TEMP%\squad_int_log.txt`, F9 = dump entity mesh coverage
- **END = real unhook**: restores hooks & materials, unloads the DLL cleanly

## Build

- VS2022 (v143), C++17, **/EHa required**, Release|x64 only
- MinHook (static lib, included)
- Offsets target **Squad v10.5.1 / UE 5.7.4 build 627303** — regenerate/reverify against your own dump for other versions

## Usage

1. Inject into `SquadGame-Win64-Shipping.exe` (bring your own EAC bypass)
2. Watch the console/log for `[init] PostRender hooked ... ready`
3. INSERT = menu, END = unhook & eject

## Credits

- **[Dumper-7](https://github.com/Encryqed/Dumper-7)** — the UE5 SDK dumper that produced the class layouts, offsets and parameter structs this project is built on
- **sbaggy** — the original *"ue5 internal base"* (ZeroGUI canvas menu, hooking scaffold) this project started from, since heavily rewritten for UE 5.7 LWC

## Disclaimer

For authorized security research and game-security study only. Use at your own risk.
