#pragma once
// v10.5.1 class layouts (offset-accessor style). Every offset is tagged:
//   [SDK] = Dumper-7 dump    [RT] = runtime-verified in external build
#include "engine.h"

namespace off
{
	// UGameViewportClient
	constexpr uint64_t ViewportWorld        = 0x0078;  // [SDK]

	// UGameInstance / UPlayer / ULocalPlayer
	constexpr uint64_t LocalPlayers         = 0x0038;  // [SDK]
	constexpr uint64_t PlayerController     = 0x0030;  // [SDK] UPlayer
	constexpr uint64_t LPViewportClient     = 0x0078;  // [SDK] ULocalPlayer

	// UWorld / ULevel / GameState
	constexpr uint64_t PersistentLevel      = 0x0030;  // [SDK]
	constexpr uint64_t WorldGameState       = 0x01B0;  // [SDK]
	constexpr uint64_t WorldLevels          = 0x01C8;  // [SDK]
	constexpr uint64_t OwningGameInstance   = 0x0230;  // [SDK]
	constexpr uint64_t LevelActors          = 0x00A0;  // [SDK] dumper-flagged
	constexpr uint64_t PlayerArray          = 0x02D0;  // [SDK] AGameStateBase

	// AActor family
	constexpr uint64_t RootComponent        = 0x01C0;  // [SDK]
	constexpr uint64_t ActorOwner           = 0x0158;  // [SDK]
	constexpr uint64_t PawnPlayerState      = 0x02D8;  // [SDK] APawn
	constexpr uint64_t PawnController       = 0x02E8;  // [SDK] APawn
	constexpr uint64_t CharacterMesh        = 0x0338;  // [SDK] ACharacter
	constexpr uint64_t CharacterMovement    = 0x0340;  // [SDK] ACharacter

	// AController / APlayerController
	constexpr uint64_t CtrlPlayerState      = 0x02C0;  // [SDK]
	constexpr uint64_t CtrlPawn             = 0x0308;  // [SDK]
	constexpr uint64_t CtrlCharacter        = 0x0318;  // [SDK]
	constexpr uint64_t ControlRotation      = 0x0348;  // [SDK]
	constexpr uint64_t AcknowledgedPawn     = 0x0378;  // [SDK]
	constexpr uint64_t PlayerCameraManager  = 0x0388;  // [SDK]

	// APlayerCameraManager: CameraCachePrivate(0x15B0) + POV(0x10)
	constexpr uint64_t CamLocation          = 0x15C0;  // [RT]
	constexpr uint64_t CamRotation          = 0x15D8;  // [RT]
	constexpr uint64_t CamFOV               = 0x15F0;  // [RT]

	// APlayerState
	constexpr uint64_t PSScore              = 0x02B8;  // [SDK]
	constexpr uint64_t PSFlags              = 0x02C2;  // [SDK] bIsABot = bit 0x08
	constexpr uint64_t PSPawnPrivate        = 0x0338;  // [SDK]
	constexpr uint64_t PSPlayerName         = 0x0358;  // [SDK] FString

	// USceneComponent
	constexpr uint64_t AttachParent         = 0x00D0;  // [SDK]
	constexpr uint64_t AttachChildren       = 0x00E8;  // [SDK]
	constexpr uint64_t RelativeLocation     = 0x0148;  // [SDK]
	constexpr uint64_t RelativeRotation     = 0x0160;  // [SDK]
	constexpr uint64_t ComponentToWorld     = 0x0200;  // [RT]

	// USkinnedMeshComponent / USkeletalMeshComponent
	constexpr uint64_t SkeletalMeshAsset    = 0x05A8;  // [SDK]
	constexpr uint64_t BoneArray            = 0x0620;  // [RT] TArray<FTransform>
	constexpr uint64_t CachedWorldBounds    = 0x0858;  // [SDK] FBoxSphereBounds
	constexpr uint64_t CachedCompSpaceXf    = 0x0A00;  // [SDK] TArray<FTransform> alt

	// UCharacterMovementComponent
	constexpr uint64_t MoveVelocity         = 0x00D8;  // [SDK]
	constexpr uint64_t MovementMode         = 0x02E1;  // [SDK]
	constexpr uint64_t MaxWalkSpeed         = 0x0328;  // [SDK]
	constexpr uint64_t MaxAcceleration      = 0x033C;  // [SDK]

	// ---- Squad package ----
	constexpr uint64_t SQTeamId             = 0x0508;  // [SDK] ASQPlayerState
	constexpr uint64_t SQTeamState          = 0x07E0;  // [SDK] ASQPlayerState
	constexpr uint64_t SQSoldier            = 0x07F0;  // [SDK] ASQPlayerState
	constexpr uint64_t SQCurrentRole        = 0x0810;  // [SDK] ASQPlayerState
	constexpr uint64_t SQIndexedTeamStates  = 0x03F8;  // [SDK] ASQGameState
	constexpr uint64_t SQTeamTickets        = 0x02B8;  // [SDK] ASQTeamState
	constexpr uint64_t SQTeamStateId        = 0x02E8;  // [SDK] ASQTeamState
	constexpr uint64_t SQPawnTeam           = 0x0346;  // [SDK] ASQPawn (vehicles)
	constexpr uint64_t SQSoldierMovement    = 0x07F0;  // [SDK] ASQSoldier
	constexpr uint64_t SQSeat               = 0x2098;  // [SDK] ASQSoldier::CurrentSeat
	constexpr uint64_t SQHealthFlags        = 0x273C;  // [SDK] bit0 dying bit2 wounded
	constexpr uint64_t SQHealth             = 0x2740;  // [SDK] ASQSoldier
	constexpr uint64_t SQBotComponent       = 0x2AF8;  // [SDK] non-null => bot
	constexpr uint64_t SQCurrentPS          = 0x310C;  // [SDK] ASQSoldier weak
	constexpr uint64_t SQStamina            = 0x10D0;  // [SDK] USQSoldierMovement

	// ASQWeapon / ASQEquipableItem
	constexpr uint64_t SQItemDisplayName    = 0x0348;  // [SDK] FText (ASQEquipableItem)
	constexpr uint64_t SQWeaponMagazines    = 0x0858;  // [SDK] TArray<FSQMagazineData> stride 8
	constexpr uint64_t SQWeaponADS          = 0x0804;  // [SDK] bool bAimingDownSights
	constexpr uint64_t SQCurrentHeldWeapon  = 0x311C;  // [SDK] ASQSoldier weak
	constexpr uint64_t SQWeaponMuzzleVel    = 0x0780;  // [SDK] WeaponConfig(0x720)+MuzzleVelocity(0x60)

	// UMaterial
	constexpr uint64_t MatDisableDepthTest  = 0x01D8;  // [SDK] uint8 bit0

	// ASQGrenadeProjectile
	constexpr uint64_t SQGrenadeExploding   = 0x05C4;  // [SDK] bool
	constexpr uint64_t SQGrenadeFuseTime    = 0x05C8;  // [SDK] float

	// ASQPlayerState extras
	constexpr uint64_t SQCurrentRoleId      = 0x0800;  // [SDK] FName
	constexpr uint64_t SQPlayerStateData    = 0x0730;  // [SDK] inline FPlayerStateDataObject (0x80)
	constexpr uint64_t PSD_Kills            = 0x04;    // [SDK]
	constexpr uint64_t PSD_Deaths           = 0x0C;    // [SDK]
	constexpr uint64_t PSD_bAdmin           = 0x30;    // [SDK] bool
	constexpr uint64_t PSD_bCommander       = 0x34;    // [SDK] bool

	// stance
	constexpr uint64_t SQProne              = 0x1F4C;  // [SDK] uint8 bit0
	constexpr uint64_t CharCrouched         = 0x0460;  // [SDK] uint8 bit1 (ACharacter)
	constexpr uint64_t SQCachedSprinting    = 0x2C51;  // [SDK] bool
	constexpr uint64_t SQMoveADS            = 0x123C;  // [SDK] USQSoldierMovement uint8 bit2 (via 0x7F0)

	// AActor extra
	constexpr uint64_t InstanceComponents   = 0x0290;  // [SDK] TArray<UActorComponent*>
	constexpr uint64_t ChildActorActor      = 0x0268;  // [SDK] UChildActorComponent::ChildActor

	// ASQVehicle (chain: ASQPawn->ASQVehicleSeat->ASQVehicle)
	constexpr uint64_t SQVehicleComponents  = 0x04A0;  // [SDK] TArray<USQVehicleComponent*> (ASQVehicleSeat)
	constexpr uint64_t SQVehicleSeats       = 0x08C8;  // [SDK] TArray<USQVehicleSeatComponent*>
	constexpr uint64_t SQVehicleMesh        = 0x08D8;  // [SDK] USkeletalMeshComponent*
	constexpr uint64_t SQVehicleHealth      = 0x09B8;  // [SDK] float
	constexpr uint64_t SQVehicleMaxHealth   = 0x09BC;  // [SDK] float
	constexpr uint64_t SQVehicleDestroyed   = 0x0A10;  // [SDK] bool
	constexpr uint64_t SQVCompHealth        = 0x0788;  // [SDK] USQVehicleComponent
	constexpr uint64_t SQSeatSeatedPlayer   = 0x02F0;  // [SDK] USQVehicleSeatComponent
	constexpr uint64_t SQSeatSeatedSoldier  = 0x02F8;  // [SDK] USQVehicleSeatComponent

	// ASQDeployable / ASQForwardBase
	constexpr uint64_t SQDeployTeam         = 0x02E0;  // [SDK] int32
	constexpr uint64_t SQDeployIsFob        = 0x02E4;  // [SDK] bool
	constexpr uint64_t SQDeployExplosive    = 0x0350;  // [SDK] uint8 ESQExplosiveType
	constexpr uint64_t SQDeployBuildState   = 0x0408;  // [SDK] uint8 (0=Completed)
	constexpr uint64_t SQDeployHealth       = 0x0444;  // [SDK] float
	constexpr uint64_t SQDeployMaxHealth    = 0x043C;  // [SDK] float
	constexpr uint64_t SQFobName            = 0x0568;  // [SDK] FString
	constexpr uint64_t SQFobSieged          = 0x05C0;  // [SDK] bool

	// USQCaptureZoneComponent
	constexpr uint64_t SQCapFlagName        = 0x0140;  // [SDK] FText
	constexpr uint64_t SQCapCapturingTeam   = 0x0150;  // [SDK] uint8
	constexpr uint64_t SQCapOwningTeam      = 0x0151;  // [SDK] uint8
	constexpr uint64_t SQCapPercent         = 0x01CC;  // [SDK] float

	// Squad bone indices (Bip01, 141 bones) [RT verified v10.5.1]
	namespace bones
	{
		constexpr int Root = 0, Bip01 = 1, Pelvis = 2, Spine = 3, Spine1 = 4,
			Spine2 = 5, Neck = 6, Head = 7, HeadNub = 8;
		constexpr int R_Clavicle = 65, R_UpperArm = 66, R_Forearm = 67, R_Hand = 68;
		constexpr int L_Clavicle = 92, L_UpperArm = 93, L_Forearm = 94, L_Hand = 95;
		constexpr int L_Thigh = 125, L_Calf = 126, L_Foot = 127, L_Toe0 = 128;
		constexpr int R_Thigh = 130, R_Calf = 131, R_Foot = 132, R_Toe0 = 133;
		constexpr int CameraBone = 121;
	}
}

// helper: read typed field at offset (direct deref, caller wraps __try)
template<typename T> __forceinline T fld(uint64_t base, uint64_t offset) {
	return *reinterpret_cast<T*>(base + offset);
}
template<typename T> __forceinline T& fld_ref(uint64_t base, uint64_t offset) {
	return *reinterpret_cast<T*>(base + offset);
}

// ==================== Engine classes ====================
class UGameViewportClient;
class UCanvas;

class UGameInstance : public UObject
{
public:
	TArray<class ULocalPlayer*> LocalPlayers() { return fld<TArray<ULocalPlayer*>>((uint64_t)this, off::LocalPlayers); }
};

class UPlayer : public UObject
{
public:
	class APlayerController* PlayerController() { return fld<class APlayerController*>((uint64_t)this, off::PlayerController); }
};

class ULocalPlayer : public UPlayer
{
public:
	UGameViewportClient* ViewportClient() { return fld<UGameViewportClient*>((uint64_t)this, off::LPViewportClient); }
};

class ULevel : public UObject
{
public:
	TArray<class AActor*> Actors() { return fld<TArray<AActor*>>((uint64_t)this, off::LevelActors); }
};

class UWorld : public UObject
{
public:
	class AGameStateBase* GameState() { return fld<class AGameStateBase*>((uint64_t)this, off::WorldGameState); }
	TArray<ULevel*> Levels() { return fld<TArray<ULevel*>>((uint64_t)this, off::WorldLevels); }
	UGameInstance* OwningGameInstance() { return fld<UGameInstance*>((uint64_t)this, off::OwningGameInstance); }
};

class UGameViewportClient : public UObject
{
public:
	UWorld* World() { return fld<UWorld*>((uint64_t)this, off::ViewportWorld); }
};

class USceneComponent : public UObject
{
public:
	FTransform& ComponentToWorld() { return fld_ref<FTransform>((uint64_t)this, off::ComponentToWorld); }
	FVector& RelativeLocation() { return fld_ref<FVector>((uint64_t)this, off::RelativeLocation); }
	FVector WorldLocation() { return ComponentToWorld().translation; }
};

class USkinnedMeshComponent : public USceneComponent {};
class UMeshComponent : public USceneComponent {};

class USkeletalMeshComponent : public USceneComponent
{
public:
	// [RT] BoneArray header @0x620: {FTransform* data; int32 count; int32 max}
	bool GetBoneArray(FTransform*& data, int32_t& count) {
		try {
			data = fld<FTransform*>((uint64_t)this, off::BoneArray);
			count = fld<int32_t>((uint64_t)this, off::BoneArray + 0x8);
			return data != nullptr && count >= 100 && count <= 300;
		} catch (...) { return false; }
	}
	TArray<UObject*> GetMaterials();
	void SetMaterial(int32_t slot, UObject* material);
};

class UMovementComponent : public UObject {};
class UPawnMovementComponent : public UMovementComponent {};
class UCharacterMovementComponent : public UPawnMovementComponent
{
public:
	FVector& Velocity() { return fld_ref<FVector>((uint64_t)this, off::MoveVelocity); }
	uint8_t& MovementMode() { return fld_ref<uint8_t>((uint64_t)this, off::MovementMode); }
	float& MaxWalkSpeed() { return fld_ref<float>((uint64_t)this, off::MaxWalkSpeed); }
	float& MaxAcceleration() { return fld_ref<float>((uint64_t)this, off::MaxAcceleration); }
};

class AActor : public UObject
{
public:
	USceneComponent* RootComponent() { return fld<USceneComponent*>((uint64_t)this, off::RootComponent); }
	FVector GetActorLocation() {
		auto root = RootComponent();
		if (root && ptr_sane(root)) return root->WorldLocation();
		return {};
	}
};

class AInfo : public AActor {};

class AGameStateBase : public AInfo
{
public:
	TArray<class APlayerState*> PlayerArray() { return fld<TArray<class APlayerState*>>((uint64_t)this, off::PlayerArray); }
};

class APlayerState : public AInfo
{
public:
	float Score() { return fld<float>((uint64_t)this, off::PSScore); }
	bool IsBot() { return (fld<uint8_t>((uint64_t)this, off::PSFlags) & 0x08) != 0; }
	class APawn* PawnPrivate() { return fld<class APawn*>((uint64_t)this, off::PSPawnPrivate); }
	FString PlayerName() { return fld<FString>((uint64_t)this, off::PSPlayerName); }
	std::string Name() { try { return PlayerName().to_string(); } catch (...) { return std::string(); } }
};

class ASQPlayerState : public APlayerState
{
public:
	int32_t TeamId() { return fld<int32_t>((uint64_t)this, off::SQTeamId); }
	class ASQTeamState* TeamState() { return fld<class ASQTeamState*>((uint64_t)this, off::SQTeamState); }
	UObject* CurrentRole() { return fld<UObject*>((uint64_t)this, off::SQCurrentRole); }
	std::string RoleId() { try { return fld<FName>((uint64_t)this, off::SQCurrentRoleId).to_string(); } catch (...) { return std::string(); } }
	// inline FPlayerStateDataObject @0x730
	int32_t Kills() { return fld<int32_t>((uint64_t)this, off::SQPlayerStateData + off::PSD_Kills); }
	int32_t Deaths() { return fld<int32_t>((uint64_t)this, off::SQPlayerStateData + off::PSD_Deaths); }
	bool IsAdmin() { return fld<bool>((uint64_t)this, off::SQPlayerStateData + off::PSD_bAdmin); }
	bool IsCommander() { return fld<bool>((uint64_t)this, off::SQPlayerStateData + off::PSD_bCommander); }
};

// FText helper: FText{ FTextData* @0x0 } -> FTextData::TextSource FString @+0x20 [SDK Basic.hpp]
inline std::string read_ftext(uint64_t ftext_addr) {
	try {
		uint64_t td = fld<uint64_t>(ftext_addr, 0);
		if (!ptr_sane((void*)td)) return std::string();
		return fld<FString>(td, 0x20).to_string();
	} catch (...) { return std::string(); }
}

// ASQWeapon (via ASQSoldier::CurrentHeldWeapon weak @0x311C)
class ASQWeapon : public AActor
{
public:
	std::string DisplayName() {
		std::string s = read_ftext((uint64_t)this + off::SQItemDisplayName);
		if (s.empty()) { try { if (ClassPrivate) s = ClassPrivate->GetName(); } catch (...) {} }
		return s;
	}
	// Magazines @0x858: TArray<FSQMagazineData{DefaultRoundsPerMag;RemainingRounds}, stride 8>
	void Ammo(int32_t& mag, int32_t& reserve) {
		mag = reserve = 0;
		try {
			uint64_t data = fld<uint64_t>((uint64_t)this, off::SQWeaponMagazines);
			int32_t count = fld<int32_t>((uint64_t)this, off::SQWeaponMagazines + 0x8);
			if (!ptr_sane((void*)data) || count <= 0 || count > 64) return;
			mag = fld<int32_t>(data, 0x4);   // loaded mag remaining
			for (int32_t i = 1; i < count; i++)
				reserve += fld<int32_t>(data + (uint64_t)i * 8, 0x4);
		} catch (...) {}
	}
	bool AimingDownSights() { return fld<bool>((uint64_t)this, off::SQWeaponADS); }
};

class ASQTeamState : public AInfo
{
public:
	int32_t Tickets() { return fld<int32_t>((uint64_t)this, off::SQTeamTickets); }
	int32_t Id() { return fld<int32_t>((uint64_t)this, off::SQTeamStateId); }
};

class ASQGameState : public AGameStateBase
{
public:
	TArray<ASQTeamState*> IndexedTeamStates() { return fld<TArray<ASQTeamState*>>((uint64_t)this, off::SQIndexedTeamStates); }
};

class APawn : public AActor
{
public:
	APlayerState* PlayerState() { return fld<APlayerState*>((uint64_t)this, off::PawnPlayerState); }
	class AController* Controller() { return fld<class AController*>((uint64_t)this, off::PawnController); }
};

class ACharacter : public APawn
{
public:
	USkeletalMeshComponent* Mesh() { return fld<USkeletalMeshComponent*>((uint64_t)this, off::CharacterMesh); }
	UCharacterMovementComponent* CharacterMovementComp() { return fld<UCharacterMovementComponent*>((uint64_t)this, off::CharacterMovement); }
};

// ASQSoldier : ACharacter (size 0x3140)
class ASQSoldier : public ACharacter
{
public:
	float Health() { return fld<float>((uint64_t)this, off::SQHealth); }
	bool IsDying() { return (fld<uint8_t>((uint64_t)this, off::SQHealthFlags) & 0x01) != 0; }
	bool IsWounded() { return (fld<uint8_t>((uint64_t)this, off::SQHealthFlags) & 0x04) != 0; }
	bool IsBot() { return fld<uint64_t>((uint64_t)this, off::SQBotComponent) != 0; }
	bool InVehicle() { return fld<uint64_t>((uint64_t)this, off::SQSeat) != 0; }
	bool IsProne() { return (fld<uint8_t>((uint64_t)this, off::SQProne) & 0x01) != 0; }
	bool IsCrouched() { return (fld<uint8_t>((uint64_t)this, off::CharCrouched) & 0x02) != 0; }
	bool IsSprinting() { return fld<bool>((uint64_t)this, off::SQCachedSprinting); }
	bool IsADS() {
		try {
			uint64_t mv = fld<uint64_t>((uint64_t)this, off::SQSoldierMovement);
			if (!ptr_sane((void*)mv)) return false;
			return (fld<uint8_t>(mv, off::SQMoveADS) & 0x04) != 0;
		} catch (...) { return false; }
	}
	class ASQWeapon* HeldWeapon() {
		return (class ASQWeapon*)WeakObjResolve(fld<FWeakObjectPtr>((uint64_t)this, off::SQCurrentHeldWeapon));
	}
	ASQPlayerState* SQPlayerState() {
		return (ASQPlayerState*)WeakObjResolve(fld<FWeakObjectPtr>((uint64_t)this, off::SQCurrentPS));
	}
};

class AController : public AActor
{
public:
	APlayerState* PlayerState() { return fld<APlayerState*>((uint64_t)this, off::CtrlPlayerState); }
	APawn* Pawn() { return fld<APawn*>((uint64_t)this, off::CtrlPawn); }
	ACharacter* Character() { return fld<ACharacter*>((uint64_t)this, off::CtrlCharacter); }
	FRotator& ControlRotation() { return fld_ref<FRotator>((uint64_t)this, off::ControlRotation); }
	bool LineOfSightTo(AActor* Other, FVector ViewPoint, bool bAlternateChecks);
};

class APlayerCameraManager : public AActor
{
public:
	FVector CameraLocation() { return fld<FVector>((uint64_t)this, off::CamLocation); }
	FRotator CameraRotation() { return fld<FRotator>((uint64_t)this, off::CamRotation); }
	float CameraFOV() { return fld<float>((uint64_t)this, off::CamFOV); }
};

class APlayerController : public AController
{
public:
	APawn* AcknowledgedPawn() { return fld<APawn*>((uint64_t)this, off::AcknowledgedPawn); }
	APlayerCameraManager* CameraManager() { return fld<APlayerCameraManager*>((uint64_t)this, off::PlayerCameraManager); }
};

class UMaterialInterface : public UObject {};
class UMaterial : public UMaterialInterface {};
class UFont : public UObject {};

// ==================== UCanvas (ProcessEvent wrappers, impl in sdk.cpp) ====================
// Parms layouts verbatim from Dumper-7 Engine_parameters.hpp
struct Canvas_K2_DrawLine_Parms
{
	FVector2D ScreenPositionA; // 0x00
	FVector2D ScreenPositionB; // 0x10
	float     Thickness;       // 0x20
	FLinearColor RenderColor;  // 0x24
};                             // 0x38
static_assert(sizeof(Canvas_K2_DrawLine_Parms) == 0x38, "DrawLine parms");

struct Canvas_K2_DrawText_Parms
{
	UObject*  RenderFont;      // 0x00
	FString   RenderText;      // 0x08
	FVector2D ScreenPosition;  // 0x18
	FVector2D Scale;           // 0x28
	FLinearColor RenderColor;  // 0x38
	float     Kerning;         // 0x48
	FLinearColor ShadowColor;  // 0x4C
	FVector2D ShadowOffset;    // 0x60
	bool      bCentreX;        // 0x70
	bool      bCentreY;        // 0x71
	bool      bOutlined;       // 0x72
	FLinearColor OutlineColor; // 0x74
};                             // 0x88
static_assert(sizeof(Canvas_K2_DrawText_Parms) == 0x88, "DrawText parms");

struct Controller_LineOfSightTo_Parms
{
	const AActor* Other;         // 0x00
	FVector       ViewPoint;     // 0x08
	bool          bAlternateChecks; // 0x20
	bool          ReturnValue;   // 0x21
};                               // 0x28
static_assert(sizeof(Controller_LineOfSightTo_Parms) == 0x28, "LOS parms");

class UCanvas : public UObject
{
public:
	void K2_DrawLine(FVector2D a, FVector2D b, float thickness, FLinearColor color);
	void K2_DrawText(UObject* font, const wchar_t* text, FVector2D pos, FVector2D scale,
	                 FLinearColor color, float kerning, FLinearColor shadow_color,
	                 FVector2D shadow_offset, bool center_x, bool center_y, bool outlined,
	                 FLinearColor outline_color);
};
