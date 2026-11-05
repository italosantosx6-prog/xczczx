#pragma once
// language: C++, file: cheat/esp.hpp, target: Valorant x64
// Entity reading + frame data for the ESP renderer

#include <vector>
#include <string>
#include "math.hpp"
#include "memory.hpp"
#include "offsets.hpp"

// ── Per-entity frame data ────────────────────────────────────────────────────
struct EntityData {
    bool    alive       = false;
    bool    is_enemy    = false;
    float   health      = 0.f;
    float   max_health  = 0.f;
    float   armor       = 0.f;
    float   distance    = 0.f;      // meters
    Vec3    head_world  = {};
    Vec3    root_world  = {};
    Vec2    head_screen = {};
    Vec2    foot_screen = {};
    Vec2    bones_screen[18] = {};  // screen-space bone positions
    bool    bone_valid  [18] = {};
    Vec2    box_tl      = {};       // top-left of bounding box
    Vec2    box_br      = {};       // bottom-right
};

// ── Bone index mapping (for skeleton drawing) ────────────────────────────────
static const int g_bone_pairs[][2] = {
    // spine
    { offsets::bone::Head,     offsets::bone::Neck      },
    { offsets::bone::Neck,     offsets::bone::Chest     },
    { offsets::bone::Chest,    offsets::bone::Stomach   },
    { offsets::bone::Stomach,  offsets::bone::Pelvis    },
    // left arm
    { offsets::bone::Chest,    offsets::bone::LShoulder },
    { offsets::bone::LShoulder,offsets::bone::LElbow    },
    { offsets::bone::LElbow,   offsets::bone::LWrist    },
    // right arm
    { offsets::bone::Chest,    offsets::bone::RShoulder },
    { offsets::bone::RShoulder,offsets::bone::RElbow    },
    { offsets::bone::RElbow,   offsets::bone::RWrist    },
    // left leg
    { offsets::bone::Pelvis,   offsets::bone::LHip      },
    { offsets::bone::LHip,     offsets::bone::LKnee     },
    { offsets::bone::LKnee,    offsets::bone::LAnkle    },
    // right leg
    { offsets::bone::Pelvis,   offsets::bone::RHip      },
    { offsets::bone::RHip,     offsets::bone::RKnee     },
    { offsets::bone::RKnee,    offsets::bone::RAnkle    },
};
static const int g_bone_pair_count = (int)(sizeof(g_bone_pairs) / sizeof(g_bone_pairs[0]));

// ── Frame state ──────────────────────────────────────────────────────────────
struct FrameState {
    std::vector<EntityData> entities;
    FCameraInfo             camera;
    FMatrix                 vp;
    float                   screen_w = 1920.f;
    float                   screen_h = 1080.f;
};

static FrameState g_frame;

// ── Forward declarations ─────────────────────────────────────────────────────
namespace esp_reader
{

// Read bone world position from mesh component
Vec3 ReadBoneWorld(uint64_t mesh, int bone_id, const FTransform& comp_to_world);
bool ReadLocalPlayer(uint64_t world, uint64_t& out_lp, int& out_team);
void UpdateFrame(float sw, float sh);

} // namespace esp_reader
