#pragma once
#include <cstdint>

namespace xyzpan {

enum EyeType : int { kEyeNone = 0, kEyeNormal = 1, kEyeGoogly = 2, kEyeXEyes = 3, kEyeCyclops = 4 };
enum EarType : int { kEarDefault = 0, kEarPointy = 1, kEarRound = 2, kEarCat = 3 };
enum NoseType : int { kNoseCone = 0, kNoseButton = 1, kNoseSnout = 2, kNoseClown = 3,
                      kNosePointed = 4, kNoseNone = 5 };
enum HatType : int { kHatNone = 0, kHatParty = 1, kHatTopHat = 2, kHatHalo = 3, kHatBeanie = 4,
                     kHatDevilHorns = 5, kHatPonytail = 6 };

// POD struct for RuneScape-style avatar deformation of the listener head.
// All values are unitless multipliers (1.0 = default/undeformed).
struct AvatarParams {
    float headElongation = 1.0f;  // Y-scale of head sphere (>1 = tall, <1 = squished)
    float eyeSize        = 1.0f;  // scale of eye spheres
    float eyeSpacing     = 1.0f;  // horizontal distance between eyes
    float eyeHeight      = 0.5f;  // vertical position on face (0=center, 1=top)
    float earSize        = 1.0f;  // scale of ear ellipsoids
    float earOffset      = 1.0f;  // lateral offset of ears from head center
    float headSize       = 1.0f;  // uniform scale of entire head assembly
    float pupilSize      = 0.35f; // pupil scale 0..1 (0=no pupil, 1=fills entire eye socket)
    float earRotation    = 0.0f;  // Z-axis roll of ears, degrees (-180 to +180)
    float googlyGravity  = 0.0f;  // source tracking strength 0..1 (0=off, 1=max pull + range)
    float googlySpring   = 1.0f;  // return-to-center weight 0..1 (0=free spin, 1=full spring return)
    int   eyeType        = kEyeNone;    // EyeType (stored as int for POD trivial-copy)
    int   earType        = kEarDefault;  // EarType (stored as int for POD trivial-copy)
    float hatSize        = 1.0f;  // uniform scale of hat/hair (0.2..3.0)
    int   hatType        = kHatNone;    // HatType (stored as int for POD trivial-copy)
    float noseSize       = 1.0f;  // uniform scale of nose (0.2..3.0)
    int   noseType       = kNoseCone;   // NoseType (stored as int for POD trivial-copy)

    // Per-user color overrides (0xFFRRGGBB format). 0 = inherit from active theme.
    uint32_t headColor   = 0;   // also applies to ears
    uint32_t noseColor   = 0;
    uint32_t hatColor    = 0;
    uint32_t eyeColor    = 0;   // eye iris/sclera color (0 = default white)
};

} // namespace xyzpan
