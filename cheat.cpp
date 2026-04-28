#include "cheat.h"
#include "offsets.h"
#include <vector>
#include <cmath>
#include <string>

namespace UE {
    constexpr uintptr_t APPEND_STRING = 0x010BC1C0;
    constexpr uintptr_t GOBJECTS      = 0x07D12B30;
    constexpr uintptr_t OFF_CLASS     = 0x10;
    constexpr uintptr_t OFF_NAME      = 0x18;

    struct FString { wchar_t* Data; int32_t Count; int32_t Max; };
    using AppendString_t = void(__fastcall*)(const void*, FString*);
    static AppendString_t g_AppendString = nullptr;

    inline void Init(uintptr_t base) {
        g_AppendString = (AppendString_t)(base + APPEND_STRING);
    }

    static int CallAppendStringSafe(const void* fnamePtr, wchar_t* buf, int maxChars) {
        FString out{ buf, 0, maxChars };
        __try {
            g_AppendString(fnamePtr, &out);
            return out.Count;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return 0;
        }
    }

    static std::string NameToString(const void* obj) {
        if (!obj || !g_AppendString) return {};
        wchar_t buf[1024] = {};
        int n = CallAppendStringSafe((const uint8_t*)obj + OFF_NAME, buf, 1024);
        if (n <= 0 || n > 1024) return {};
        std::string s;
        s.reserve(n);
        for (int i = 0; i < n && buf[i]; ++i) s.push_back((char)buf[i]);
        return s;
    }

    template<typename Fn>
    void ForEachUObject(uintptr_t base, Fn&& fn) {
        uintptr_t array   = base + GOBJECTS;
        uintptr_t* chunks = *(uintptr_t**)(array + 0x00);
        int32_t numElems  = *(int32_t* )(array + 0x14);
        int32_t numChunks = *(int32_t* )(array + 0x1C);
        if (!chunks || numElems <= 0 || numChunks <= 0) return;
        if (numElems > 10'000'000) return;

        constexpr int32_t PER_CHUNK = 0x10000;
        constexpr int32_t ITEM_SIZE = 0x18;
        for (int32_t i = 0; i < numElems; ++i) {
            int32_t ci = i / PER_CHUNK;
            int32_t ii = i % PER_CHUNK;
            if (ci >= numChunks) break;
            uintptr_t chunk = chunks[ci];
            if (!chunk) continue;
            uintptr_t obj = *(uintptr_t*)(chunk + (uintptr_t)ii * ITEM_SIZE);
            if (!obj) continue;
            if (!fn((void*)obj)) break;
        }
    }
}

namespace Cheat {
    uintptr_t base_address = 0;
    uintptr_t gworld_ptr   = 0;

    bool no_kick_cooldown  = false;
    bool no_touch_slowdown = false;
    bool custom_cam        = false;

    static void BallVelocityTick(uintptr_t pawn);
    static uintptr_t FindBallActor(const Vector3& playerPos);
    static const std::vector<uintptr_t>& FindAllBalls();
    static void AutoGKTick(uintptr_t pawn);
    static void AimBallTick();

    bool   zoom_hold        = false;
    float  zoom_fov         = 45.0f;
    float  fov_value        = 90.0f;
    double cam_dist_value   = 600.0;
    double cam_height_value = 150.0;

    bool  esp_players      = false;
    bool  esp_ball         = false;
    bool  esp_names        = true;
    bool  esp_distance     = true;
    bool  esp_lines        = false;
    float esp_max_distance = 10000.0f;
    bool  crosshair        = false;
    int   crosshair_style  = 0;
    float crosshair_size   = 6.0f;
    bool  radar            = false;
    float radar_size       = 180.0f;
    float radar_range      = 5000.0f;
    bool  ball_trail       = false;

    bool  ball_prediction = false;
    float ball_pred_time  = 1.5f;
    bool  move_guide      = false;
    bool  aim_view        = false;
    float aim_view_length = 700.0f;

    bool  auto_gk          = false;
    int   gk_key_short     = 0xA2;
    int   gk_key_big       = 'W';
    int   gk_key_left      = 'A';
    int   gk_key_right     = 'D';
    float gk_big_threshold = 80.0f;
    float gk_trigger_time  = 0.30f;
    float gk_max_height    = 250.0f;

    bool  aim_ball        = false;
    float aim_speed       = 0.30f;
    float aim_sensitivity = 10.0f;
    bool  aim_ball_pitch  = true;

    static Vector3  g_ball_last_pos     = {0,0,0};
    static uint64_t g_ball_last_time_ms = 0;
    static Vector3  g_ball_velocity     = {0,0,0};
    static bool     g_ball_has_history  = false;

    uintptr_t FindPattern(const char* signature) {
        static auto patternToByte = [](const char* pattern) {
            auto bytes = std::vector<int>{};
            auto start = const_cast<char*>(pattern);
            auto end   = const_cast<char*>(pattern) + strlen(pattern);
            for (auto current = start; current < end; ++current) {
                if (*current == '?') {
                    ++current;
                    if (*current == '?') ++current;
                    bytes.push_back(-1);
                } else {
                    bytes.push_back(strtoul(current, &current, 16));
                }
            }
            return bytes;
        };

        auto module    = GetModuleHandle(NULL);
        auto dosHeader = (PIMAGE_DOS_HEADER)module;
        auto ntHeaders = (PIMAGE_NT_HEADERS)((std::uint8_t*)module + dosHeader->e_lfanew);
        auto sizeOfImage = ntHeaders->OptionalHeader.SizeOfImage;
        auto patternBytes = patternToByte(signature);
        auto scanBytes = reinterpret_cast<std::uint8_t*>(module);

        auto s = patternBytes.size();
        auto d = patternBytes.data();

        for (auto i = 0ul; i < sizeOfImage - s; ++i) {
            bool found = true;
            for (auto j = 0ul; j < s; ++j) {
                if (scanBytes[i + j] != d[j] && d[j] != -1) {
                    found = false;
                    break;
                }
            }
            if (found) return reinterpret_cast<uintptr_t>(&scanBytes[i]);
        }
        return 0;
    }

    bool Init() {
        base_address = (uintptr_t)GetModuleHandle(NULL);

        uintptr_t addr = FindPattern("48 8B 05 ? ? ? ? 48 8B 88 ? ? ? ? 48 85 C9 74 06");
        if (addr) {
            int32_t offset = *(int32_t*)(addr + 3);
            gworld_ptr = addr + 7 + offset;
        } else {
            gworld_ptr = base_address + Offsets::GWorld;
        }

        UE::Init(base_address);

        return (gworld_ptr != 0);
    }

    bool GetCamera(Vector3& outLoc, Rotator& outRot, float& outFov) {
        if (!gworld_ptr) return false;
        uintptr_t world = *(uintptr_t*)gworld_ptr; if (!world) return false;
        uintptr_t gi  = Read<uintptr_t>(world + Offsets::OwningGameInstance); if (!gi)  return false;
        uintptr_t lpa = Read<uintptr_t>(gi + Offsets::LocalPlayers);          if (!lpa) return false;
        uintptr_t lp  = Read<uintptr_t>(lpa);                                  if (!lp)  return false;
        uintptr_t pc  = Read<uintptr_t>(lp + Offsets::PlayerController);       if (!pc)  return false;
        uintptr_t pcm = Read<uintptr_t>(pc + Offsets::PlayerCameraManager);    if (!pcm) return false;

        uintptr_t povBase = pcm + Offsets::ViewTarget + 0x10;
        outLoc = Read<Vector3>(povBase + 0x00);
        outRot = Read<Rotator>(povBase + 0x18);
        outFov = Read<float>(povBase + 0x30);
        return outFov > 1.0f;
    }

    bool WorldToScreen(const Vector3& world, const Vector3& camLoc, const Rotator& camRot,
                       float fov, float screenW, float screenH, float& sx, float& sy) {
        const double DEG = 3.14159265358979323846 / 180.0;
        double sp = sin(camRot.pitch * DEG), cp = cos(camRot.pitch * DEG);
        double sy_= sin(camRot.yaw   * DEG), cy = cos(camRot.yaw   * DEG);

        double dx = world.x - camLoc.x;
        double dy = world.y - camLoc.y;
        double dz = world.z - camLoc.z;

        double fwdDot   = dx*(cp*cy)   + dy*(cp*sy_)  + dz*(sp);
        double rightDot = dx*(-sy_)    + dy*(cy);
        double upDot    = dx*(-sp*cy)  + dy*(-sp*sy_) + dz*(cp);

        if (fwdDot < 1.0) return false;

        double tanHalf = tan(fov * 0.5 * DEG);
        double cxd = screenW * 0.5;
        double cyd = screenH * 0.5;

        sx = (float)(cxd + (rightDot / fwdDot) / tanHalf * cxd);
        sy = (float)(cyd - (upDot    / fwdDot) / tanHalf * cxd);
        return true;
    }

    void GatherEsp(std::vector<EspEntry>& out) {
        out.clear();
        if (!gworld_ptr) return;
        uintptr_t world = *(uintptr_t*)gworld_ptr;                            if (!world)     return;
        uintptr_t gameState = Read<uintptr_t>(world + Offsets::GameState);    if (!gameState) return;

        uintptr_t gi     = Read<uintptr_t>(world + Offsets::OwningGameInstance);
        uintptr_t lpa    = gi  ? Read<uintptr_t>(gi + Offsets::LocalPlayers)          : 0;
        uintptr_t lp     = lpa ? Read<uintptr_t>(lpa)                                  : 0;
        uintptr_t pc     = lp  ? Read<uintptr_t>(lp + Offsets::PlayerController)       : 0;
        uintptr_t myPawn = pc  ? Read<uintptr_t>(pc + Offsets::AcknowledgedPawn)       : 0;

        Vector3 refLoc{0, 0, 0};
        if (myPawn) {
            uintptr_t root = Read<uintptr_t>(myPawn + Offsets::RootComponent);
            if (root) refLoc = Read<Vector3>(root + Offsets::RelativeLocation);
        }

        if (esp_players) {
            uintptr_t arrData = Read<uintptr_t>(gameState + Offsets::GS_PlayerArray);
            int32_t   arrNum  = Read<int32_t>  (gameState + Offsets::GS_PlayerArray + 8);
            if (arrData && arrNum > 0 && arrNum < 64) {
                for (int i = 0; i < arrNum; i++) {
                    uintptr_t ps   = Read<uintptr_t>(arrData + (uintptr_t)i * 8);             if (!ps)   continue;
                    uintptr_t pawn = Read<uintptr_t>(ps + Offsets::PS_PawnPrivate);           if (!pawn) continue;
                    uintptr_t root = Read<uintptr_t>(pawn + Offsets::RootComponent);          if (!root) continue;
                    if (pawn == myPawn) continue;

                    Vector3 loc = Read<Vector3>(root + Offsets::RelativeLocation);
                    double dx = loc.x - refLoc.x, dy = loc.y - refLoc.y, dz = loc.z - refLoc.z;
                    float dist = (float)sqrt(dx*dx + dy*dy + dz*dz);
                    if (dist > esp_max_distance) continue;

                    EspEntry e{};
                    e.worldPos = loc;
                    e.rotation = Read<Rotator>(root + Offsets::RelativeLocation + 0x18);
                    e.distance = dist;
                    e.team     = Read<int32_t>(pawn + Offsets::Pawn_Team);
                    sprintf_s(e.label, "P%d", e.team);
                    out.push_back(e);
                }
            }
        }

        if (esp_ball) {
            struct BallCand { uintptr_t ptr; Vector3 loc; float dist; bool privileged; };
            BallCand cands[32];
            int       nCands = 0;
            auto pushUnique = [&](uintptr_t b, bool priv) {
                if (!b || nCands >= 32) return;
                for (int i = 0; i < nCands; ++i) if (cands[i].ptr == b) return;
                uintptr_t r = Read<uintptr_t>(b + Offsets::RootComponent);
                if (!r) return;
                Vector3 loc = Read<Vector3>(r + Offsets::RelativeLocation);
                double dx = loc.x - refLoc.x, dy = loc.y - refLoc.y, dz = loc.z - refLoc.z;
                float dist = (float)sqrt(dx*dx + dy*dy + dz*dz);
                cands[nCands++] = { b, loc, dist, priv };
            };

            uintptr_t mc = Read<uintptr_t>(gameState + Offsets::MatchControllerFromGS);
            if (mc) pushUnique(Read<uintptr_t>(mc + Offsets::MatchBall), true);
            if (myPawn) pushUnique(Read<uintptr_t>(myPawn + Offsets::PersonalBall), true);
            for (uintptr_t b : FindAllBalls()) pushUnique(b, false);

            const double Z_TOLERANCE = 300.0;
            int kept[8]; int nKept = 0;
            for (int i = 0; i < nCands && nKept < 8; ++i) {
                const auto& c = cands[i];
                if (!c.privileged) {
                    if (c.dist > esp_max_distance) continue;
                    if (fabs(c.loc.z - refLoc.z) > Z_TOLERANCE) continue;
                }
                kept[nKept++] = i;
            }
            for (int i = 1; i < nKept; ++i) {
                int v = kept[i];
                int j = i - 1;
                while (j >= 0 && cands[kept[j]].dist > cands[v].dist) {
                    kept[j + 1] = kept[j]; --j;
                }
                kept[j + 1] = v;
            }
            if (nKept > 3) nKept = 3;

            for (int i = 0; i < nKept; ++i) {
                const auto& c = cands[kept[i]];
                EspEntry e{};
                e.worldPos = c.loc;
                e.distance = c.dist;
                e.team     = -1;
                if (nKept > 1) sprintf_s(e.label, "Ball %d", i + 1);
                else            sprintf_s(e.label, "Ball");
                out.push_back(e);
            }
        }
    }

    void Tick() {
        if (!gworld_ptr) return;
        uintptr_t world = *(uintptr_t*)gworld_ptr;
        if (!world) return;

        uintptr_t gameInstance = Read<uintptr_t>(world + Offsets::OwningGameInstance);
        if (!gameInstance) return;

        uintptr_t localPlayerArray = Read<uintptr_t>(gameInstance + Offsets::LocalPlayers);
        if (!localPlayerArray) return;

        uintptr_t localPlayer = Read<uintptr_t>(localPlayerArray);
        if (!localPlayer) return;

        uintptr_t playerController = Read<uintptr_t>(localPlayer + Offsets::PlayerController);
        if (!playerController) return;

        uintptr_t pawn = Read<uintptr_t>(playerController + Offsets::AcknowledgedPawn);
        if (!pawn) return;

        if (no_kick_cooldown)  Write<bool>(pawn + Offsets::KickCoolDown,    false);
        if (no_touch_slowdown) Write<bool>(pawn + Offsets::InTouchSlowdown, false);

        uintptr_t firstCam = Read<uintptr_t>(pawn + Offsets::OnestPersonCamera);
        uintptr_t thirdCam = Read<uintptr_t>(pawn + Offsets::ThreerdPersonCamera);
        float activeFov = fov_value;
        if (zoom_hold && (GetAsyncKeyState('C') & 0x8000)) activeFov = zoom_fov;
        if (firstCam) Write<float>(firstCam + Offsets::FieldOfView, activeFov);
        if (thirdCam) Write<float>(thirdCam + Offsets::FieldOfView, activeFov);

        if (custom_cam) {
            uintptr_t springArm = Read<uintptr_t>(pawn + Offsets::ThreerdPersonCameraRoot);
            if (springArm) {
                Write<float> (springArm + Offsets::SA_TargetArmLength,   (float)cam_dist_value);
                Write<double>(springArm + Offsets::SA_SocketOffset + 16, cam_height_value);
            }
        }

        BallVelocityTick(pawn);
        AimBallTick();
        AutoGKTick(pawn);
    }

    static void BallVelocityTick(uintptr_t pawn) {
        if (!ball_prediction && !auto_gk) {
            g_ball_has_history = false;
            return;
        }

        uintptr_t ball = 0;
        uintptr_t world = *(uintptr_t*)gworld_ptr;
        if (world) {
            uintptr_t gs = Read<uintptr_t>(world + Offsets::GameState);
            uintptr_t mc = gs ? Read<uintptr_t>(gs + Offsets::MatchControllerFromGS) : 0;
            if (mc) ball = Read<uintptr_t>(mc + Offsets::MatchBall);
        }
        if (!ball) ball = Read<uintptr_t>(pawn + Offsets::PersonalBall);
        if (!ball) {
            Vector3 pp;
            if (GetPlayerPosition(pp)) ball = FindBallActor(pp);
        }
        if (!ball) { g_ball_has_history = false; return; }

        uintptr_t ballRoot = Read<uintptr_t>(ball + Offsets::RootComponent);
        if (!ballRoot) { g_ball_has_history = false; return; }

        Vector3  pos = Read<Vector3>(ballRoot + Offsets::RelativeLocation);
        uint64_t now = GetTickCount64();

        if (g_ball_has_history) {
            double dt = (double)(now - g_ball_last_time_ms) / 1000.0;
            if (dt > 0.001 && dt < 0.2) {
                Vector3 v;
                v.x = (pos.x - g_ball_last_pos.x) / dt;
                v.y = (pos.y - g_ball_last_pos.y) / dt;
                v.z = (pos.z - g_ball_last_pos.z) / dt;

                const double alpha = auto_gk ? 0.85 : 0.55;
                g_ball_velocity.x = alpha * v.x + (1.0 - alpha) * g_ball_velocity.x;
                g_ball_velocity.y = alpha * v.y + (1.0 - alpha) * g_ball_velocity.y;
                g_ball_velocity.z = alpha * v.z + (1.0 - alpha) * g_ball_velocity.z;
            }
        }

        g_ball_last_pos     = pos;
        g_ball_last_time_ms = now;
        g_ball_has_history  = true;
    }

    static bool IsGameForeground() {
        HWND fg = GetForegroundWindow();
        if (!fg) return false;
        DWORD fgPid = 0;
        GetWindowThreadProcessId(fg, &fgPid);
        return fgPid == GetCurrentProcessId();
    }

    static void AimBallTick() {
        if (!aim_ball) return;
        if (!IsGameForeground()) return;

        bool m4 = (GetAsyncKeyState(VK_XBUTTON1) & 0x8000) != 0;
        bool m5 = (GetAsyncKeyState(VK_XBUTTON2) & 0x8000) != 0;
        if (!m4 && !m5) return;

        Vector3 ballPos;
        if (!GetBallPosition(ballPos)) return;

        Vector3 camLoc; Rotator camRot; float fov = 0.f;
        if (!GetCamera(camLoc, camRot, fov)) return;

        double dx = ballPos.x - camLoc.x;
        double dy = ballPos.y - camLoc.y;
        double dz = ballPos.z - camLoc.z;
        double horiz = sqrt(dx*dx + dy*dy);
        if (horiz < 5.0) return;

        const double DEG = 180.0 / 3.14159265358979323846;
        double desiredYaw   = atan2(dy, dx) * DEG;
        double desiredPitch = atan2(dz, horiz) * DEG;

        double dyaw = desiredYaw - camRot.yaw;
        while (dyaw >  180.0) dyaw -= 360.0;
        while (dyaw < -180.0) dyaw += 360.0;
        double dpitch = aim_ball_pitch ? (desiredPitch - camRot.pitch) : 0.0;

        double alpha = aim_speed;
        if (alpha < 0.02) alpha = 0.02;
        if (alpha > 1.0)  alpha = 1.0;
        double moveYaw   = dyaw   * alpha;
        double movePitch = dpitch * alpha;

        LONG mdx = (LONG)(moveYaw    * aim_sensitivity);
        LONG mdy = (LONG)(-movePitch * aim_sensitivity);

        if (mdx == 0 && mdy == 0) return;
        const LONG CAP = 200;
        if (mdx >  CAP) mdx =  CAP;
        if (mdx < -CAP) mdx = -CAP;
        if (mdy >  CAP) mdy =  CAP;
        if (mdy < -CAP) mdy = -CAP;

        INPUT in = {};
        in.type = INPUT_MOUSE;
        in.mi.dx = mdx;
        in.mi.dy = mdy;
        in.mi.dwFlags = MOUSEEVENTF_MOVE;
        SendInput(1, &in, sizeof(in));
    }

    enum class GKState { Idle, Holding, Cooldown };
    static GKState  g_gk_state        = GKState::Idle;
    static uint64_t g_gk_state_time   = 0;
    static int      g_gk_held_mod_key = 0;
    static int      g_gk_held_dir_key = 0;

    static void GKSendKey(int vk, bool down) {
        INPUT in = {};
        in.type = INPUT_KEYBOARD;
        in.ki.wVk = (WORD)vk;
        in.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
        SendInput(1, &in, sizeof(in));
    }

    static void GKReleaseAll() {
        if (g_gk_held_dir_key) {
            GKSendKey(g_gk_held_dir_key, false);
            g_gk_held_dir_key = 0;
        }
        if (g_gk_held_mod_key) {
            GKSendKey(g_gk_held_mod_key, false);
            g_gk_held_mod_key = 0;
        }
    }

    static bool GKComputeImpact(const Vector3& playerPos,
                                const Vector3& ballPos, const Vector3& ballVel,
                                double cameraYawDeg,
                                float& outTime, float& outSide,
                                float& outImpactZ, float& outClosest)
    {
        double v2 = ballVel.x*ballVel.x + ballVel.y*ballVel.y + ballVel.z*ballVel.z;
        if (v2 < 100.0 * 100.0) return false;

        double rx = ballPos.x - playerPos.x;
        double ry = ballPos.y - playerPos.y;
        double rz = ballPos.z - playerPos.z;
        double rdotv = rx*ballVel.x + ry*ballVel.y + rz*ballVel.z;
        double t = -rdotv / v2;
        if (t <= 0.0)        return false;
        if (t > 1.5)         return false;

        double bxt = ballPos.x + ballVel.x * t;
        double byt = ballPos.y + ballVel.y * t;
        double bzt = ballPos.z + ballVel.z * t + 0.5 * (-980.0) * t * t;

        double dxh = bxt - playerPos.x;
        double dyh = byt - playerPos.y;
        double closest = sqrt(dxh*dxh + dyh*dyh);

        if (closest > 600.0) return false;
        if (bzt < playerPos.z - 80.0) return false;
        if (bzt > gk_max_height)      return false;

        const double DEG = 3.14159265358979323846 / 180.0;
        double yaw   = cameraYawDeg * DEG;
        double fx    = cos(yaw),  fy = sin(yaw);
        double rxAx  = -fy,       ryAx = fx;
        double side  = dxh * rxAx + dyh * ryAx;

        outTime    = (float)t;
        outSide    = (float)side;
        outImpactZ = (float)bzt;
        outClosest = (float)closest;
        return true;
    }

    static void AutoGKTick(uintptr_t pawn) {
        uint64_t now = GetTickCount64();

        if (!auto_gk || !IsGameForeground()) {
            if (g_gk_state != GKState::Idle) {
                GKReleaseAll();
                g_gk_state = GKState::Idle;
            }
            return;
        }

        switch (g_gk_state) {
        case GKState::Idle: {
            if (!g_ball_has_history) return;

            uintptr_t root = Read<uintptr_t>(pawn + Offsets::RootComponent);
            if (!root) return;
            Vector3 playerPos = Read<Vector3>(root + Offsets::RelativeLocation);

            Vector3 camLoc; Rotator camRot; float fov = 0.f;
            double yawDeg = 0.0;
            if (GetCamera(camLoc, camRot, fov)) yawDeg = camRot.yaw;

            float t, dy, zImpact, closest;
            if (!GKComputeImpact(playerPos, g_ball_last_pos, g_ball_velocity,
                                 yawDeg, t, dy, zImpact, closest))
                return;

            if (t > gk_trigger_time + 0.05f) return;

            int   dirKey = (dy >= 0.f) ? gk_key_right : gk_key_left;
            float adyAbs = fabsf(dy);

            int modKey;
            if (adyAbs < 60.f)                  modKey = gk_key_big;
            else if (adyAbs < gk_big_threshold) modKey = gk_key_short;
            else                                modKey = gk_key_big;

            GKSendKey(modKey, true);
            g_gk_held_mod_key = modKey;
            GKSendKey(dirKey, true);
            g_gk_held_dir_key = dirKey;

            g_gk_state      = GKState::Holding;
            g_gk_state_time = now;
            break;
        }

        case GKState::Holding:
            if (now - g_gk_state_time >= 120) {
                GKReleaseAll();
                g_gk_state      = GKState::Cooldown;
                g_gk_state_time = now;
            }
            break;

        case GKState::Cooldown:
            if (now - g_gk_state_time >= 400) {
                g_gk_state = GKState::Idle;
            }
            break;
        }
    }

    void GetBallPrediction(BallPrediction& out) {
        out.count = 0;
        out.speed = 0.f;
        if (!ball_prediction || !g_ball_has_history) return;

        Vector3 pos = g_ball_last_pos;
        Vector3 vel = g_ball_velocity;

        double speed = sqrt(vel.x*vel.x + vel.y*vel.y + vel.z*vel.z);
        out.speed = (float)speed;
        if (speed < 60.0) return;

        constexpr double GRAVITY = -980.0;
        constexpr double DRAG    = 0.99;

        const float duration = (ball_pred_time < 0.2f) ? 0.2f : ball_pred_time;
        const int   N        = 48;
        const double dt      = duration / N;

        const double groundZ = pos.z - 30.0;

        for (int i = 0; i < N && out.count < 64; ++i) {
            vel.z += GRAVITY * dt;
            vel.x *= DRAG;
            vel.y *= DRAG;

            pos.x += vel.x * dt;
            pos.y += vel.y * dt;
            pos.z += vel.z * dt;

            if (pos.z < groundZ) {
                pos.z = groundZ;
                vel.z = -vel.z * 0.4;
                if (fabs(vel.z) < 80.0) vel.z = 0.0;
            }

            out.points[out.count++] = pos;
        }
    }

    bool GetPlayerPosition(Vector3& out) {
        if (!gworld_ptr) return false;
        uintptr_t world = *(uintptr_t*)gworld_ptr;                            if (!world) return false;
        uintptr_t gi  = Read<uintptr_t>(world + Offsets::OwningGameInstance); if (!gi)  return false;
        uintptr_t lpa = Read<uintptr_t>(gi    + Offsets::LocalPlayers);       if (!lpa) return false;
        uintptr_t lp  = Read<uintptr_t>(lpa);                                 if (!lp)  return false;
        uintptr_t pc  = Read<uintptr_t>(lp    + Offsets::PlayerController);   if (!pc)  return false;
        uintptr_t pwn = Read<uintptr_t>(pc    + Offsets::AcknowledgedPawn);   if (!pwn) return false;
        uintptr_t root = Read<uintptr_t>(pwn  + Offsets::RootComponent);      if (!root) return false;
        out = Read<Vector3>(root + Offsets::RelativeLocation);
        return true;
    }

    static uintptr_t g_BallClass    = 0;
    static uintptr_t g_CachedBall   = 0;
    static uint64_t  g_BallScanTime = 0;

    static std::vector<uintptr_t> g_AllBalls;
    static uint64_t               g_AllBallsScanTime = 0;

    static uintptr_t FindBallActor(const Vector3& playerPos) {
        if (!base_address) return 0;
        uint64_t now = GetTickCount64();

        if (g_CachedBall && (now - g_BallScanTime) < 500) {
            if (Read<uintptr_t>(g_CachedBall + UE::OFF_CLASS) != 0)
                return g_CachedBall;
        }

        uintptr_t bestObj  = 0;
        double    bestDist = 1e30;

        UE::ForEachUObject(base_address, [&](void* obj) -> bool {
            uintptr_t cls = Read<uintptr_t>((uintptr_t)obj + UE::OFF_CLASS);
            if (!cls) return true;

            if (g_BallClass) {
                if (cls != g_BallClass) return true;
            } else {
                std::string n = UE::NameToString((void*)cls);
                if (n != "SoccerBall_C") return true;
                g_BallClass = cls;
            }

            uintptr_t root = Read<uintptr_t>((uintptr_t)obj + Offsets::RootComponent);
            if (!root) return true;
            Vector3 loc = Read<Vector3>(root + Offsets::RelativeLocation);
            if (fabs(loc.z - playerPos.z) > 200.0) return true;

            double dx = loc.x - playerPos.x;
            double dy = loc.y - playerPos.y;
            double dz = loc.z - playerPos.z;
            double d2 = dx*dx + dy*dy + dz*dz;
            if (d2 < bestDist) {
                bestDist = d2;
                bestObj  = (uintptr_t)obj;
            }
            return true;
        });

        g_CachedBall   = bestObj;
        g_BallScanTime = now;
        return bestObj;
    }

    static const std::vector<uintptr_t>& FindAllBalls() {
        uint64_t now = GetTickCount64();
        if (!g_AllBalls.empty() && (now - g_AllBallsScanTime) < 250) {
            return g_AllBalls;
        }
        g_AllBalls.clear();
        if (!base_address) return g_AllBalls;

        UE::ForEachUObject(base_address, [&](void* obj) -> bool {
            uintptr_t cls = Read<uintptr_t>((uintptr_t)obj + UE::OFF_CLASS);
            if (!cls) return true;
            if (g_BallClass) {
                if (cls != g_BallClass) return true;
            } else {
                std::string n = UE::NameToString((void*)cls);
                if (n != "SoccerBall_C") return true;
                g_BallClass = cls;
            }
            g_AllBalls.push_back((uintptr_t)obj);
            return true;
        });

        g_AllBallsScanTime = now;
        return g_AllBalls;
    }

    bool GetBallPosition(Vector3& out) {
        if (!gworld_ptr) return false;
        uintptr_t world = *(uintptr_t*)gworld_ptr;                            if (!world) return false;
        uintptr_t gi  = Read<uintptr_t>(world + Offsets::OwningGameInstance);
        uintptr_t lpa = gi  ? Read<uintptr_t>(gi  + Offsets::LocalPlayers)        : 0;
        uintptr_t lp  = lpa ? Read<uintptr_t>(lpa)                                 : 0;
        uintptr_t pc  = lp  ? Read<uintptr_t>(lp  + Offsets::PlayerController)    : 0;
        uintptr_t pwn = pc  ? Read<uintptr_t>(pc  + Offsets::AcknowledgedPawn)    : 0;

        uintptr_t ball = 0;
        uintptr_t gs = Read<uintptr_t>(world + Offsets::GameState);
        uintptr_t mc = gs ? Read<uintptr_t>(gs + Offsets::MatchControllerFromGS) : 0;
        if (mc) ball = Read<uintptr_t>(mc + Offsets::MatchBall);
        if (!ball && pwn) ball = Read<uintptr_t>(pwn + Offsets::PersonalBall);
        if (!ball) {
            Vector3 pp;
            if (GetPlayerPosition(pp)) ball = FindBallActor(pp);
        }
        if (!ball) return false;

        uintptr_t bRoot = Read<uintptr_t>(ball + Offsets::RootComponent);
        if (!bRoot) return false;
        out = Read<Vector3>(bRoot + Offsets::RelativeLocation);
        return true;
    }
}
