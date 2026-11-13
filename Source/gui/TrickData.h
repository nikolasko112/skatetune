#pragma once
#include <array>

namespace skatetune
{

/** Which simplified silhouette + rotation style to draw for a trick.
    AirTuck tricks are told apart from EACH OTHER by boardFlipTurns /
    boardShoveTurns below (that's the whole point - a kickflip and a
    heelflip must not look identical just because both are "airborne"),
    Grind tricks by grindTiltDeg, and the rest get a dedicated pose. */
enum class TrickPose
{
    AirTuck,   // airborne - board spin comes from boardFlipTurns/boardShoveTurns below
    Grind,     // grinding a rail/ledge - board held at a fixed grindTiltDeg, rail+sparks drawn underneath
    Manual,    // balancing on two wheels, held (not a quick snap) with a small wobble
    Boneless,  // one foot plants on the ground mid-trick
    Slide      // sliding on the board's underside along the coping, sparks
};

struct Trick
{
    const char* name;
    TrickPose pose;
    int score;

    // AirTuck only: how many full rotations the board makes about each of
    // two different axes, so the simple 2D silhouette can fake each one
    // with a believable squash instead of one generic flat spin:
    //  - boardFlipTurns rotates about the board's long (nose-tail) axis -
    //    the squash that reads as "edge-on" happens to the board's HEIGHT.
    //    +ve = kickflip direction, -ve = heelflip direction.
    //  - boardShoveTurns rotates about the vertical axis (a "shove-it") -
    //    the squash that reads as "edge-on" happens to the board's WIDTH.
    // A trick can use either, both (a varial-style combo), or neither
    // (an ollie: the board never leaves level at all).
    float boardFlipTurns  = 0.0f;
    float boardShoveTurns = 0.0f;

    // AirTuck only, for the big vert-ramp tricks: how many full rotations
    // the whole skater+board silhouette makes together as one rigid unit
    // (a real aerial spin, not the board moving relative to the feet the
    // way boardFlipTurns/boardShoveTurns do). drawSkater() renders this by
    // rotating the whole drawing around the hip, which is the correct way
    // to fake a big-air spin like the 900 or McTwist in 2D: those are body
    // rotations with the feet staying planted on the board throughout, not
    // a board trick at all.
    float wholeBodySpinTurns = 0.0f;

    // Grind only: static board pitch held through the whole grind - 0 =
    // level (50-50), +ve = nose lifted (grinding the tail truck, "5-0"),
    // -ve = tail lifted (grinding the nose truck, "Nosegrind").
    float grindTiltDeg = 0.0f;
};

inline const std::array<Trick, 16>& allTricks()
{
    // Columns after score are, in struct order: boardFlipTurns, boardShoveTurns,
    // wholeBodySpinTurns, grindTiltDeg. All four are written out explicitly
    // below (even when 0) so inserting/reordering a field can never again
    // silently shift a later column into the wrong meaning.
    static const std::array<Trick, 16> tricks { {
        { "Ollie",         TrickPose::AirTuck,  100,  0.0f, 0.0f, 0.0f,  0.0f },
        { "Pop Shove-It",  TrickPose::AirTuck,  150,  0.0f, 0.5f, 0.0f,  0.0f },
        { "Kickflip",      TrickPose::AirTuck,  200,  1.0f, 0.0f, 0.0f,  0.0f },
        { "Heelflip",      TrickPose::AirTuck,  200, -1.0f, 0.0f, 0.0f,  0.0f },
        { "Varial Flip",   TrickPose::AirTuck,  250,  1.0f, 0.5f, 0.0f,  0.0f },
        { "Hardflip",      TrickPose::AirTuck,  350, -1.0f, 0.5f, 0.0f,  0.0f },
        { "360 Flip",      TrickPose::AirTuck,  400,  1.0f, 1.0f, 0.0f,  0.0f },
        { "Impossible",    TrickPose::AirTuck,  450,  2.0f, 0.0f, 0.0f,  0.0f },
        // The two biggest vert-ramp tricks in skateboarding, named for
        // their real rotation count - both landed on a rigid, grabbed
        // body+board unit, not a board-relative flip/shove, hence
        // wholeBodySpinTurns rather than the board-only fields above.
        // McTwist: Mike McGill's signature inverted 540 (1.5 turns).
        { "McTwist",       TrickPose::AirTuck,  540,  0.0f, 0.0f, 1.5f,  0.0f },
        // The 900: two and a half full aerial spins (900 degrees) - first
        // landed by Tony Hawk at the 1999 X Games, still skateboarding's
        // most legendary trick. The hardest thing this GUI draws, so it
        // also gets this file's largest wholeBodySpinTurns, to read as
        // clearly the biggest air of any trick here.
        { "The 900",       TrickPose::AirTuck,  900,  0.0f, 0.0f, 2.5f,  0.0f },
        { "Boneless",      TrickPose::Boneless, 150,  0.0f, 0.0f, 0.0f,  0.0f },
        { "Manual",        TrickPose::Manual,   100,  0.0f, 0.0f, 0.0f,  0.0f },
        { "50-50 Grind",   TrickPose::Grind,    200,  0.0f, 0.0f, 0.0f,   0.0f },
        { "5-0 Grind",     TrickPose::Grind,    250,  0.0f, 0.0f, 0.0f,  18.0f },
        { "Nosegrind",     TrickPose::Grind,    300,  0.0f, 0.0f, 0.0f, -18.0f },
        { "Darkslide",     TrickPose::Slide,    500,  0.0f, 0.0f, 0.0f,   0.0f },
    } };
    return tricks;
}

} // namespace skatetune
