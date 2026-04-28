#pragma once
#include <cstdint>

namespace Offsets {
    inline uintptr_t GWorld              = 0x7E93198;
    inline uintptr_t GameState           = 0x160;
    inline uintptr_t OwningGameInstance  = 0x1D8;
    inline uintptr_t LocalPlayers        = 0x38;
    inline uintptr_t PlayerController    = 0x30;
    inline uintptr_t PlayerCameraManager = 0x348;
    inline uintptr_t AcknowledgedPawn    = 0x338;
    inline uintptr_t ViewTarget          = 0x320;

    inline uintptr_t RootComponent    = 0x1A0;
    inline uintptr_t RelativeLocation = 0x128;

    inline uintptr_t OnestPersonCamera       = 0x748;
    inline uintptr_t ThreerdPersonCamera     = 0x758;
    inline uintptr_t FieldOfView             = 0x230;
    inline uintptr_t OnestPersonCameraRoot   = 0x740;
    inline uintptr_t ThreerdPersonCameraRoot = 0x750;
    inline uintptr_t SA_TargetArmLength      = 0x230;
    inline uintptr_t SA_SocketOffset         = 0x238;

    inline uintptr_t KickCoolDown           = 0x1B70;
    inline uintptr_t InTouchSlowdown        = 0x1DF8;
    inline uintptr_t PersonalBall           = 0x1748;
    inline uintptr_t Pawn_Team              = 0xD48;

    inline uintptr_t GS_PlayerArray = 0x2A8;
    inline uintptr_t PS_PawnPrivate = 0x308;

    inline uintptr_t MatchControllerFromGS = 0x308;
    inline uintptr_t MatchBall             = 0x2D8;
}
