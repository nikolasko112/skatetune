#pragma once
#include <array>

namespace skatetune
{

/** Which simplified silhouette to draw for a trick. Several tricks share a
    pose (e.g. all the flip tricks are "tucked in mid-air") - the name and
    score are what make each one feel distinct in the popup banner. */
enum class TrickPose
{
    AirTuck,
    Grind,
    Manual,
    Boneless,
    Slide
};

struct Trick
{
    const char* name;
    TrickPose pose;
    int score;
};

inline const std::array<Trick, 14>& allTricks()
{
    static const std::array<Trick, 14> tricks { {
        { "Ollie",        TrickPose::AirTuck,  100 },
        { "Pop Shove-It",  TrickPose::AirTuck, 150 },
        { "Kickflip",      TrickPose::AirTuck, 200 },
        { "Heelflip",      TrickPose::AirTuck, 200 },
        { "Varial Flip",   TrickPose::AirTuck, 250 },
        { "Hardflip",      TrickPose::AirTuck, 350 },
        { "360 Flip",      TrickPose::AirTuck, 400 },
        { "Impossible",    TrickPose::AirTuck, 450 },
        { "Boneless",      TrickPose::Boneless, 150 },
        { "Manual",        TrickPose::Manual,  100 },
        { "50-50 Grind",   TrickPose::Grind,   200 },
        { "5-0 Grind",     TrickPose::Grind,   250 },
        { "Nosegrind",     TrickPose::Grind,   300 },
        { "Darkslide",     TrickPose::Slide,   500 },
    } };
    return tricks;
}

} // namespace skatetune
