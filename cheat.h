#pragma once
#include <windows.h>
#include <iostream>
#include <vector>
#include "offsets.h"

namespace Cheat {
    struct Vector3 { double x, y, z; };
    struct Rotator { double pitch, yaw, roll; };

    struct EspEntry {
        Vector3 worldPos;
        Rotator rotation;
        float   distance;
        int     team;
        char    label[32];
    };

    extern uintptr_t base_address;
    extern uintptr_t gworld_ptr;

    extern bool no_kick_cooldown;
    extern bool no_touch_slowdown;

    extern bool   custom_cam;
    extern bool   zoom_hold;
    extern float  zoom_fov;
    extern float  fov_value;
    extern double cam_dist_value;
    extern double cam_height_value;

    extern bool  esp_players;
    extern bool  esp_ball;
    extern bool  esp_names;
    extern bool  esp_distance;
    extern bool  esp_lines;
    extern float esp_max_distance;
    extern bool  crosshair;
    extern int   crosshair_style;
    extern float crosshair_size;
    extern bool  radar;
    extern float radar_size;
    extern float radar_range;
    extern bool  ball_trail;

    extern bool  ball_prediction;
    extern float ball_pred_time;

    extern bool  move_guide;

    extern bool  aim_view;
    extern float aim_view_length;

    extern bool  auto_gk;
    extern int   gk_key_short;
    extern int   gk_key_big;
    extern int   gk_key_left;
    extern int   gk_key_right;
    extern float gk_big_threshold;
    extern float gk_trigger_time;
    extern float gk_max_height;

    extern bool  aim_ball;
    extern float aim_speed;
    extern float aim_sensitivity;
    extern bool  aim_ball_pitch;

    struct BallPrediction {
        Vector3 points[64];
        int     count;
        float   speed;
    };
    void GetBallPrediction(BallPrediction& out);

    bool GetPlayerPosition(Vector3& out);
    bool GetBallPosition(Vector3& out);

    bool Init();
    void Tick();

    bool GetCamera(Vector3& outLoc, Rotator& outRot, float& outFov);
    void GatherEsp(std::vector<EspEntry>& out);
    bool WorldToScreen(const Vector3& world, const Vector3& camLoc, const Rotator& camRot,
                       float fov, float screenW, float screenH, float& sx, float& sy);

    template <typename T>
    T Read(uintptr_t address) {
        if (address < 0x20000 || address > 0x7FFFFFF000000) return T();
        if (IsBadReadPtr((const void*)address, sizeof(T))) return T();
        return *(T*)address;
    }

    template <typename T>
    void Write(uintptr_t address, T value) {
        if (address < 0x20000 || address > 0x7FFFFFF000000) return;
        if (IsBadWritePtr((void*)address, sizeof(T))) return;
        *(T*)address = value;
    }
}
