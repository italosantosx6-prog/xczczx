#pragma once
// language: C++, file: cheat/math.hpp, target: Windows 11 x64
// Vector math, FTransform, World-to-Screen for UE4

#include <cmath>
#include <algorithm>

static constexpr float M_PI_F = 3.14159265358979323846f;

struct Vec2 { float x, y; };
struct Vec3 { float x, y, z; };
struct Vec4 { float x, y, z, w; };

// UE4 FQuat
struct FQuat { float x, y, z, w; };

// UE4 FTransform (96 bytes: Rotation, Translation, Scale3D)
struct FTransform {
    FQuat  rotation;        // 0x00  16 bytes
    Vec3   translation;     // 0x10  12 bytes
    float  trans_pad;       // 0x1C
    Vec3   scale;           // 0x20
    float  scale_pad;       // 0x2C
};

// UE4 FMinimalViewInfo camera
struct FCameraInfo {
    Vec3  location;   // world location
    float pad0;
    Vec3  rotation;   // pitch, yaw, roll (degrees)
    float fov;        // horizontal FOV
};

// ── Quaternion → rotation matrix ────────────────────────────────────────────
struct FMatrix {
    float m[4][4];

    static FMatrix FromQuat(const FQuat& q)
    {
        FMatrix out = {};
        float x2 = q.x*q.x, y2 = q.y*q.y, z2 = q.z*q.z;
        out.m[0][0] = 1 - 2*(y2+z2);
        out.m[0][1] = 2*(q.x*q.y + q.z*q.w);
        out.m[0][2] = 2*(q.x*q.z - q.y*q.w);
        out.m[1][0] = 2*(q.x*q.y - q.z*q.w);
        out.m[1][1] = 1 - 2*(x2+z2);
        out.m[1][2] = 2*(q.y*q.z + q.x*q.w);
        out.m[2][0] = 2*(q.x*q.z + q.y*q.w);
        out.m[2][1] = 2*(q.y*q.z - q.x*q.w);
        out.m[2][2] = 1 - 2*(x2+y2);
        out.m[3][3] = 1.0f;
        return out;
    }

    // Multiply two 4x4 matrices
    FMatrix operator*(const FMatrix& o) const
    {
        FMatrix r = {};
        for (int i = 0; i < 4; i++)
            for (int j = 0; j < 4; j++)
                for (int k = 0; k < 4; k++)
                    r.m[i][j] += m[i][k] * o.m[k][j];
        return r;
    }
};

// ── World space bone to world position via FTransform ───────────────────────
// UE4: component-space bone * mesh ComponentToWorld = world position
inline Vec3 BoneToWorld(const FTransform& bone, const FTransform& comp_to_world)
{
    // Build rotation matrix from component-to-world quaternion
    FMatrix cm = FMatrix::FromQuat(comp_to_world.rotation);
    // Apply scale
    cm.m[0][0] *= comp_to_world.scale.x;
    cm.m[1][1] *= comp_to_world.scale.y;
    cm.m[2][2] *= comp_to_world.scale.z;
    // Add translation column
    cm.m[3][0] = comp_to_world.translation.x;
    cm.m[3][1] = comp_to_world.translation.y;
    cm.m[3][2] = comp_to_world.translation.z;
    cm.m[3][3] = 1.0f;

    // Bone local position
    Vec3 lp = bone.translation;

    // Transform: world = comp_to_world * local
    Vec3 wp;
    wp.x = lp.x*cm.m[0][0] + lp.y*cm.m[1][0] + lp.z*cm.m[2][0] + cm.m[3][0];
    wp.y = lp.x*cm.m[0][1] + lp.y*cm.m[1][1] + lp.z*cm.m[2][1] + cm.m[3][1];
    wp.z = lp.x*cm.m[0][2] + lp.y*cm.m[1][2] + lp.z*cm.m[2][2] + cm.m[3][2];
    return wp;
}

// ── Build UE4 view-projection matrix from camera FRotator + FOV ─────────────
// Returns combined VP matrix used for WorldToScreen
FMatrix BuildViewMatrix(const FCameraInfo& cam, float screen_w, float screen_h)
{
    float yaw   = cam.rotation.y * M_PI_F / 180.0f;
    float pitch = cam.rotation.x * M_PI_F / 180.0f;
    float roll  = cam.rotation.z * M_PI_F / 180.0f;

    // UE4 uses left-handed, Z-up coordinates
    // Build basis vectors from yaw/pitch
    float cy = cosf(yaw),   sy = sinf(yaw);
    float cp = cosf(pitch), sp = sinf(pitch);
    float cr = cosf(roll),  sr = sinf(roll);

    Vec3 fwd  = { cp*cy, cp*sy, sp };
    Vec3 right= { cr*sy - sr*sp*cy, -cr*cy - sr*sp*sy, sr*cp };
    Vec3 up   = { -sr*sy - cr*sp*cy, sr*cy - cr*sp*sy, cr*cp };

    FMatrix view = {};
    view.m[0][0] = right.x; view.m[0][1] = up.x; view.m[0][2] = fwd.x;
    view.m[1][0] = right.y; view.m[1][1] = up.y; view.m[1][2] = fwd.y;
    view.m[2][0] = right.z; view.m[2][1] = up.z; view.m[2][2] = fwd.z;
    view.m[3][0] = -(right.x*cam.location.x + right.y*cam.location.y + right.z*cam.location.z);
    view.m[3][1] = -(up.x   *cam.location.x + up.y   *cam.location.y + up.z   *cam.location.z);
    view.m[3][2] = -(fwd.x  *cam.location.x + fwd.y  *cam.location.y + fwd.z  *cam.location.z);
    view.m[3][3] = 1.0f;

    // Projection matrix (UE4 reverse-Z)
    float fov_rad = cam.fov * M_PI_F / 180.0f;
    float aspect  = screen_w / screen_h;
    float cot_fov = 1.0f / tanf(fov_rad * 0.5f);
    float near_z  = 10.0f;

    FMatrix proj = {};
    proj.m[0][0] = cot_fov / aspect;
    proj.m[1][1] = cot_fov;
    proj.m[2][2] = 0.0f;         // reverse-Z: maps [near,∞] → [1,0]
    proj.m[2][3] = 1.0f;
    proj.m[3][2] = near_z;

    return view * proj;
}

// ── World-to-Screen ──────────────────────────────────────────────────────────
// Returns false if behind camera
inline bool WorldToScreen(
    const FMatrix& vp,
    const Vec3&    world,
    float          screen_w,
    float          screen_h,
    Vec2&          out_screen)
{
    float x = world.x, y = world.y, z = world.z;

    // Clip space
    float cx = x*vp.m[0][0] + y*vp.m[1][0] + z*vp.m[2][0] + vp.m[3][0];
    float cy = x*vp.m[0][1] + y*vp.m[1][1] + z*vp.m[2][1] + vp.m[3][1];
    float cw = x*vp.m[0][3] + y*vp.m[1][3] + z*vp.m[2][3] + vp.m[3][3];

    if (cw < 0.001f) return false; // behind camera

    float inv_w = 1.0f / cw;
    float ndcx  = cx * inv_w;
    float ndcy  = cy * inv_w;

    out_screen.x = (screen_w / 2.0f) + ndcx * (screen_w / 2.0f);
    out_screen.y = (screen_h / 2.0f) - ndcy * (screen_h / 2.0f);
    return true;
}

// ── Distance between two 3D points ──────────────────────────────────────────
inline float Dist3D(const Vec3& a, const Vec3& b)
{
    float dx = a.x-b.x, dy = a.y-b.y, dz = a.z-b.z;
    return sqrtf(dx*dx + dy*dy + dz*dz);
}

// UE4 units are cm by default; convert to meters
inline float DistMeters(const Vec3& a, const Vec3& b)
{
    return Dist3D(a, b) / 100.0f;
}
