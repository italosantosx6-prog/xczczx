#pragma once
// language: C++, file: cheat/offsets.hpp, target: Valorant x64
// UE4-based offsets — Valorant uses a modified UE4 Chaos engine
// These offsets are version-specific. Update after each game patch.
// Use CE or x64dbg to re-derive; patterns provided below each offset.
//
// Patch version these were verified against: ~patch 10.x (2025-era)
// Run offset_scanner.py to auto-update after a Valorant update.

namespace offsets
{

// ─── Engine Globals ─────────────────────────────────────────────────────────
// Pattern for GObjects: 48 8B 05 ? ? ? ? 48 8D 1C C8 → resolve RIP+rel32
constexpr uint64_t GObjects        = 0x10E6A040; // FUObjectArray
// Pattern for GWorld:  48 8B 1D ? ? ? ? 48 85 DB → resolve RIP+rel32
constexpr uint64_t GWorld          = 0x10E56B80; // UWorld*

// ─── UObject ────────────────────────────────────────────────────────────────
constexpr uint32_t UObject_Index         = 0x000; // int32_t  — object index in GObjects
constexpr uint32_t UObject_Class         = 0x010; // UClass*
constexpr uint32_t UObject_Name          = 0x018; // FName
constexpr uint32_t UObject_Outer         = 0x020; // UObject*

// ─── UWorld ─────────────────────────────────────────────────────────────────
constexpr uint32_t UWorld_PersistentLevel = 0x038; // ULevel*
constexpr uint32_t UWorld_GameState       = 0x110; // AGameStateBase*
constexpr uint32_t UWorld_OwningGameInstance = 0x180; // UGameInstance*

// ─── ULevel ─────────────────────────────────────────────────────────────────
constexpr uint32_t ULevel_Actors         = 0x098; // TArray<AActor*>
constexpr uint32_t TArray_Data           = 0x000; // void*  (ptr to data)
constexpr uint32_t TArray_Count          = 0x008; // int32_t

// ─── AActor ─────────────────────────────────────────────────────────────────
constexpr uint32_t AActor_RootComponent  = 0x198; // USceneComponent*

// ─── USceneComponent ────────────────────────────────────────────────────────
constexpr uint32_t USceneComponent_ComponentToWorld = 0x240; // FTransform
// FTransform: Rotation(16) + Translation(12) + Scale(12) = 40 bytes
constexpr uint32_t FTransform_Translation = 0x010; // FVector (12 bytes)

// ─── Pawn / PlayerState ──────────────────────────────────────────────────────
constexpr uint32_t APawn_PlayerState     = 0x2C0; // APlayerState*
constexpr uint32_t APlayerState_TeamID   = 0x4E0; // int32_t  (Valorant-specific)
constexpr uint32_t APlayerState_IsBot    = 0x4E4; // bool

// ─── Valorant-specific: Health / Shield ─────────────────────────────────────
// ACharacter -> UAbilitySystemComponent path in Valorant
// Easier path: scan for health in character properties
constexpr uint32_t VCharacter_HealthComp = 0x4D0; // UHealthComponent*
constexpr uint32_t UHealthComp_Health    = 0x1D0; // float
constexpr uint32_t UHealthComp_MaxHealth = 0x1D4; // float
constexpr uint32_t UHealthComp_Armor     = 0x1D8; // float

// ─── Mesh / Bones ────────────────────────────────────────────────────────────
constexpr uint32_t ACharacter_Mesh       = 0x320; // USkeletalMeshComponent*
// USkeletalMeshComponent -> bone array
// Offset varies — search for ComponentToWorld in the mesh component
constexpr uint32_t USkMesh_BoneArray     = 0x5B0; // TArray<FTransform>
constexpr uint32_t USkMesh_BoneCount     = 0x5B8; // int32_t (overlaps TArray.Count)
constexpr uint32_t USkMesh_ComponentToWorld = 0x240; // FTransform (same as USceneComponent)

// ─── Bone IDs (Valorant shared rig — same across all agents) ─────────────────
// Confirm in UE4 viewer or bone dump; these are typical UE4 humanoid IDs
namespace bone
{
    constexpr int Head     = 8;
    constexpr int Neck     = 7;
    constexpr int Chest    = 5;
    constexpr int Stomach  = 4;
    constexpr int Pelvis   = 3;
    constexpr int LShoulder= 20;
    constexpr int LElbow   = 21;
    constexpr int LWrist   = 22;
    constexpr int RShoulder= 10;
    constexpr int RElbow   = 11;
    constexpr int RWrist   = 12;
    constexpr int LHip     = 25;
    constexpr int LKnee    = 26;
    constexpr int LAnkle   = 27;
    constexpr int RHip     = 29;
    constexpr int RKnee    = 30;
    constexpr int RAnkle   = 31;
}

// ─── Camera / Projection ─────────────────────────────────────────────────────
// APlayerController -> APlayerCameraManager -> ViewInfo
constexpr uint32_t APlayerController_CameraManager = 0x348; // APlayerCameraManager*
constexpr uint32_t ACameraManager_CameraCache       = 0x1B10; // FCameraCache
// FCameraCache.LastFrameCachedView (FMinimalViewInfo):
constexpr uint32_t FCameraCache_View                = 0x010; // FMinimalViewInfo
constexpr uint32_t FMinViewInfo_Location            = 0x000; // FVector
constexpr uint32_t FMinViewInfo_Rotation            = 0x00C; // FRotator
constexpr uint32_t FMinViewInfo_FOV                 = 0x018; // float

// ─── UGameInstance -> LocalPlayers ──────────────────────────────────────────
constexpr uint32_t UGameInstance_LocalPlayers = 0x038; // TArray<ULocalPlayer*>
constexpr uint32_t ULocalPlayer_PlayerController = 0x030; // APlayerController*

// ─── FName ──────────────────────────────────────────────────────────────────
// FName -> GNames lookup
// Pattern: 48 8D 05 ? ? ? ? 48 8B 48 ? → GNames
constexpr uint64_t GNames            = 0x10D00000; // TNameEntryArray
constexpr uint32_t FName_Index       = 0x000; // int32_t
constexpr uint32_t FName_Number      = 0x004; // int32_t

} // namespace offsets

// ─── NOTE: Re-derive after each Valorant patch ───────────────────────────────
// Run: python3 offset_scanner.py --pid <valorant_pid> --patterns offsets.json
// The scanner uses the byte patterns in comments above to find new offsets.
// Vanguard blocks most external tools; run the scanner from a kernel context
// or use CE with Vanguard's driver manually bypassed.
