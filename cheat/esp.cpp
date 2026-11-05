// language: C++, file: cheat/esp.cpp, target: Valorant x64, MSVC
// Entity reader — walks UWorld actor list, reads player data each frame

#include "esp.hpp"
#include <algorithm>
#include <cmath>

namespace esp_reader
{

// ── Read one bone world position ─────────────────────────────────────────────
Vec3 ReadBoneWorld(uint64_t mesh, int bone_id, const FTransform& comp_to_world)
{
    if (!mesh) return {};

    // USkeletalMeshComponent bone array is a TArray<FTransform>
    uint64_t bone_array_ptr = mem::ReadPtrOffset(mesh, offsets::USkMesh_BoneArray);
    int bone_count          = mem::ReadOffset<int>(mesh, offsets::USkMesh_BoneCount);

    if (!bone_array_ptr || bone_id >= bone_count) return {};

    FTransform bone_xf = mem::Read<FTransform>(bone_array_ptr + bone_id * sizeof(FTransform));
    return BoneToWorld(bone_xf, comp_to_world);
}

// ── Read local player controller + team ─────────────────────────────────────
bool ReadLocalPlayer(uint64_t world, uint64_t& out_pc, int& out_team)
{
    // UWorld -> UGameInstance -> LocalPlayers[0] -> PlayerController
    uint64_t game_inst = mem::ReadPtrOffset(world, offsets::UWorld_OwningGameInstance);
    if (!game_inst) return false;

    // TArray<ULocalPlayer*>
    uint64_t lp_array = mem::ReadPtrOffset(game_inst, offsets::UGameInstance_LocalPlayers);
    if (!lp_array) return false;

    uint64_t lp0 = mem::Read<uint64_t>(lp_array); // first element
    if (!lp0) return false;

    uint64_t pc = mem::ReadPtrOffset(lp0, offsets::ULocalPlayer_PlayerController);
    if (!pc) return false;

    out_pc = pc;

    // Get local player team
    uint64_t pawn = mem::ReadPtr(pc + 0x2A8); // AController::Pawn
    if (!pawn) { out_team = -1; return true; }

    uint64_t ps = mem::ReadPtrOffset(pawn, offsets::APawn_PlayerState);
    out_team = ps ? mem::ReadOffset<int>(ps, offsets::APlayerState_TeamID) : -1;

    return true;
}

// ── Camera from APlayerCameraManager ────────────────────────────────────────
static FCameraInfo ReadCamera(uint64_t pc)
{
    FCameraInfo cam = {};
    uint64_t cam_mgr = mem::ReadPtrOffset(pc, offsets::APlayerController_CameraManager);
    if (!cam_mgr) return cam;

    // FCameraCache: 8-byte pad + FMinimalViewInfo
    uint64_t view_base = cam_mgr + offsets::ACameraManager_CameraCache + offsets::FCameraCache_View;
    cam.location = mem::Read<Vec3>(view_base + offsets::FMinViewInfo_Location);
    cam.rotation = mem::Read<Vec3>(view_base + offsets::FMinViewInfo_Rotation);
    cam.fov      = mem::Read<float>(view_base + offsets::FMinViewInfo_FOV);
    if (cam.fov < 1.f || cam.fov > 180.f) cam.fov = 103.f; // Valorant default
    return cam;
}

// ── Main frame update ────────────────────────────────────────────────────────
void UpdateFrame(float sw, float sh)
{
    g_frame.entities.clear();
    g_frame.screen_w = sw;
    g_frame.screen_h = sh;

    // ── 1. Read UWorld ───────────────────────────────────────────────────────
    uint64_t world = mem::ReadGame<uint64_t>((uint32_t)offsets::GWorld);
    if (!world) return;

    // ── 2. Local player ──────────────────────────────────────────────────────
    uint64_t local_pc = 0;
    int      local_team = -1;
    ReadLocalPlayer(world, local_pc, local_team);

    // ── 3. Camera ────────────────────────────────────────────────────────────
    if (local_pc) {
        g_frame.camera = ReadCamera(local_pc);
        g_frame.vp = BuildViewMatrix(g_frame.camera, sw, sh);
    }

    // ── 4. Actor list ────────────────────────────────────────────────────────
    uint64_t level = mem::ReadPtrOffset(world, offsets::UWorld_PersistentLevel);
    if (!level) return;

    uint64_t actor_data  = mem::ReadPtrOffset(level, offsets::ULevel_Actors + offsets::TArray_Data);
    int      actor_count = mem::ReadOffset<int>(level, offsets::ULevel_Actors + offsets::TArray_Count);

    if (!actor_data || actor_count <= 0 || actor_count > 1024) return;

    // Read all actor pointers in one shot (up to 1024)
    int clamped = std::min(actor_count, 1024);
    std::vector<uint64_t> actors(clamped);
    mem::ReadRaw(actor_data, actors.data(), clamped * sizeof(uint64_t));

    for (int i = 0; i < clamped; i++)
    {
        uint64_t actor = actors[i];
        if (!actor) continue;

        // ── Class name check: "ShooterCharacter" or "BaseCharacter" ─────────
        // UObject.Class -> UClass.Name. We check by FName comparison.
        // Faster path: check if the actor has a valid mesh + health component
        // (avoids string reads every frame; fallback if needed)

        // ── Mesh ──────────────────────────────────────────────────────────
        uint64_t mesh = mem::ReadPtrOffset(actor, offsets::ACharacter_Mesh);
        if (!mesh) continue;

        // ── Health ────────────────────────────────────────────────────────
        uint64_t health_comp = mem::ReadPtrOffset(actor, offsets::VCharacter_HealthComp);
        if (!health_comp) continue;

        float hp     = mem::ReadOffset<float>(health_comp, offsets::UHealthComp_Health);
        float max_hp = mem::ReadOffset<float>(health_comp, offsets::UHealthComp_MaxHealth);
        float armor  = mem::ReadOffset<float>(health_comp, offsets::UHealthComp_Armor);

        if (hp <= 0.f || hp > 150.f || max_hp <= 0.f) continue; // sanity

        // ── Team ─────────────────────────────────────────────────────────
        uint64_t ps   = mem::ReadPtrOffset(actor, offsets::APawn_PlayerState);
        int team      = ps ? mem::ReadOffset<int>(ps, offsets::APlayerState_TeamID) : -1;
        bool is_enemy = (team != local_team) || (local_team == -1);

        // ── Root position ────────────────────────────────────────────────
        uint64_t root_comp = mem::ReadPtrOffset(actor, offsets::AActor_RootComponent);
        Vec3 root_world = root_comp
            ? mem::ReadOffset<Vec3>(root_comp,
                offsets::USceneComponent_ComponentToWorld + offsets::FTransform_Translation)
            : Vec3{};

        // ── Mesh ComponentToWorld ────────────────────────────────────────
        FTransform c2w = mem::ReadOffset<FTransform>(mesh, offsets::USkMesh_ComponentToWorld);

        // ── Head bone world position ─────────────────────────────────────
        Vec3 head_world = ReadBoneWorld(mesh, offsets::bone::Head, c2w);
        if (head_world.x == 0.f && head_world.y == 0.f) continue;

        // ── Distance ────────────────────────────────────────────────────
        float dist = DistMeters(g_frame.camera.location, head_world);
        if (dist > 300.f) continue; // 300 m cutoff (Valorant maps are small)

        // ── World → Screen ───────────────────────────────────────────────
        EntityData ent;
        ent.alive      = true;
        ent.is_enemy   = is_enemy;
        ent.health     = hp;
        ent.max_health = max_hp;
        ent.armor      = armor;
        ent.distance   = dist;
        ent.head_world = head_world;
        ent.root_world = root_world;

        bool head_vis = WorldToScreen(g_frame.vp, head_world,  sw, sh, ent.head_screen);
        bool foot_vis = WorldToScreen(g_frame.vp, root_world,  sw, sh, ent.foot_screen);

        if (!head_vis && !foot_vis) continue;

        // ── All bones ───────────────────────────────────────────────────
        static const int all_bones[] = {
            offsets::bone::Head,  offsets::bone::Neck,
            offsets::bone::Chest, offsets::bone::Stomach, offsets::bone::Pelvis,
            offsets::bone::LShoulder, offsets::bone::LElbow, offsets::bone::LWrist,
            offsets::bone::RShoulder, offsets::bone::RElbow, offsets::bone::RWrist,
            offsets::bone::LHip, offsets::bone::LKnee, offsets::bone::LAnkle,
            offsets::bone::RHip, offsets::bone::RKnee, offsets::bone::RAnkle,
        };
        static const int bone_slot_map[] = { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 };

        for (int b = 0; b < 17; b++) {
            Vec3 bw = ReadBoneWorld(mesh, all_bones[b], c2w);
            ent.bone_valid[b] = WorldToScreen(g_frame.vp, bw, sw, sh, ent.bones_screen[b]);
        }

        // ── Bounding box from head + foot ────────────────────────────────
        if (head_vis && foot_vis) {
            float height     = ent.foot_screen.y - ent.head_screen.y;
            float box_width  = fabsf(height) * 0.5f;
            float cx         = (ent.head_screen.x + ent.foot_screen.x) * 0.5f;
            ent.box_tl = { cx - box_width * 0.5f, ent.head_screen.y };
            ent.box_br = { cx + box_width * 0.5f, ent.foot_screen.y };
        }

        g_frame.entities.push_back(ent);
    }
}

} // namespace esp_reader
