/*
 * sst-voicemanager - a header only library providing synth
 * voice management in response to midi and clap event streams
 * with support for a variety of play, trigger, and midi nodes
 *
 * Copyright 2023-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * sst-voicemanager is released under the MIT license, available
 * as LICENSE.md in the root of this repository.
 *
 * All source in sst-voicemanager available at
 * https://github.com/surge-synthesizer/sst-voicemanager
 */

#include "catch2.hpp"

#include "sst/voicemanager/voicemanager.h"
#include "test_player.h"

/*
 * These tests verify that MonoPriorityMode controls whether a new note takes over
 * from the currently playing voice in MONO_NOTES and MONO_LEGATO modes.
 *
 * LATEST (default): any new note always takes over.
 * HIGHEST: a new note only takes over if its key is strictly higher than the current voice.
 * LOWEST:  a new note only takes over if its key is strictly lower than the current voice.
 *
 * When a new note does not win, the existing voice continues and the new key is still tracked
 * in held state so that release-priority logic (ON_RELEASE_TO_*) works correctly.
 */

// ---------------------------------------------------------------------------
// MONO_LEGATO + HIGHEST
// ---------------------------------------------------------------------------

TEST_CASE("Mono Legato - Stealing Priority HIGHEST: higher note wins")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_LEGATO);
    vm.setMonoPriorityMode(0, vm_t::MonoPriorityMode::HIGHEST);

    // Press 60 — voice starts at 60
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);

    // Press 65 — 65 > 60, voice moves to 65
    vm.processNoteOnEvent(0, 0, 65, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 65);

    // Still only one voice
    REQUIRE_VOICE_COUNTS(1, 1);
}

TEST_CASE("Mono Legato - Stealing Priority HIGHEST: lower note does not move voice")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_LEGATO);
    vm.setMonoPriorityMode(0, vm_t::MonoPriorityMode::HIGHEST);

    // Press 60 — voice starts at 60
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);

    // Press 65 — 65 > 60, voice moves to 65
    vm.processNoteOnEvent(0, 0, 65, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 65);

    // Press 63 — 63 < 65, voice stays at 65; no new voice created
    vm.processNoteOnEvent(0, 0, 63, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 65);
}

TEST_CASE(
    "Mono Legato - Stealing Priority HIGHEST: release highest, voice moves to next-highest held")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::MONO_LEGATO |
                       (uint64_t)vm_t::MonoPlayModeFeatures::ON_RELEASE_TO_HIGHEST);
    vm.setMonoPriorityMode(0, vm_t::MonoPriorityMode::HIGHEST);

    // Press 60, 65, 63  (63 doesn't take over since 63 < 65)
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
    vm.processNoteOnEvent(0, 0, 65, -1, 0.8, 0);
    vm.processNoteOnEvent(0, 0, 63, -1, 0.8, 0);

    // Voice should be at 65 (highest winner)
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 65);

    // Release 65 — voice moves to highest remaining held key (63 > 60)
    vm.processNoteOffEvent(0, 0, 65, -1, 0.8);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 63);

    // Release 63 — voice moves to 60
    vm.processNoteOffEvent(0, 0, 63, -1, 0.8);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);

    // Release 60 — voice releases (no other held keys)
    vm.processNoteOffEvent(0, 0, 60, -1, 0.8);
    REQUIRE_VOICE_COUNTS(1, 0);
    tp.processFor(10);
    REQUIRE_VOICE_COUNTS(0, 0);
}

TEST_CASE(
    "Mono Legato - Stealing Priority HIGHEST: releasing non-winning note clears its held state")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::MONO_LEGATO |
                       (uint64_t)vm_t::MonoPlayModeFeatures::ON_RELEASE_TO_HIGHEST);
    vm.setMonoPriorityMode(0, vm_t::MonoPriorityMode::HIGHEST);

    // Press 60, 65, 63
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
    vm.processNoteOnEvent(0, 0, 65, -1, 0.8, 0);
    vm.processNoteOnEvent(0, 0, 63, -1, 0.8, 0);
    REQUIRE_VOICE_MATCH(1, v.key() == 65);

    // Release 63 (the non-winning note) — voice should remain at 65
    vm.processNoteOffEvent(0, 0, 63, -1, 0.8);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 65);

    // Release 65 — only 60 remains; voice moves to 60
    vm.processNoteOffEvent(0, 0, 65, -1, 0.8);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);

    // Release 60 — voice releases
    vm.processNoteOffEvent(0, 0, 60, -1, 0.8);
    REQUIRE_VOICE_COUNTS(1, 0);
    tp.processFor(10);
    REQUIRE_VOICE_COUNTS(0, 0);
}

// ---------------------------------------------------------------------------
// MONO_LEGATO + LOWEST
// ---------------------------------------------------------------------------

TEST_CASE("Mono Legato - Stealing Priority LOWEST: lower note wins")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_LEGATO);
    vm.setMonoPriorityMode(0, vm_t::MonoPriorityMode::LOWEST);

    // Press 65 — voice starts at 65
    vm.processNoteOnEvent(0, 0, 65, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 65);

    // Press 60 — 60 < 65, voice moves to 60
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);

    // Press 63 — 63 > 60, voice stays at 60; no new voice created
    vm.processNoteOnEvent(0, 0, 63, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);
}

TEST_CASE("Mono Legato - Stealing Priority LOWEST: release lowest, voice moves to next-lowest held")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::MONO_LEGATO |
                       (uint64_t)vm_t::MonoPlayModeFeatures::ON_RELEASE_TO_LOWEST);
    vm.setMonoPriorityMode(0, vm_t::MonoPriorityMode::LOWEST);

    // Press 65, 60, 63  (63 doesn't take over since 63 > 60)
    vm.processNoteOnEvent(0, 0, 65, -1, 0.8, 0);
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
    vm.processNoteOnEvent(0, 0, 63, -1, 0.8, 0);

    // Voice should be at 60 (lowest winner)
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);

    // Release 60 — voice moves to lowest remaining held key (63 < 65)
    vm.processNoteOffEvent(0, 0, 60, -1, 0.8);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 63);

    // Release 63 — voice moves to 65
    vm.processNoteOffEvent(0, 0, 63, -1, 0.8);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 65);

    // Release 65 — voice releases
    vm.processNoteOffEvent(0, 0, 65, -1, 0.8);
    REQUIRE_VOICE_COUNTS(1, 0);
    tp.processFor(10);
    REQUIRE_VOICE_COUNTS(0, 0);
}

// ---------------------------------------------------------------------------
// MONO_NOTES (retrigger) + HIGHEST
// ---------------------------------------------------------------------------

TEST_CASE("Mono Retrigger - Stealing Priority HIGHEST: higher note wins")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_MONO);
    vm.setMonoPriorityMode(0, vm_t::MonoPriorityMode::HIGHEST);

    // Press 60 — voice at 60
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);

    // Press 65 — 65 > 60, old voice terminated, new voice at 65
    vm.processNoteOnEvent(0, 0, 65, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 65);
}

TEST_CASE("Mono Retrigger - Stealing Priority HIGHEST: lower note does not retrigger")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_MONO);
    vm.setMonoPriorityMode(0, vm_t::MonoPriorityMode::HIGHEST);

    // Press 60, 65 — voice ends up at 65
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
    vm.processNoteOnEvent(0, 0, 65, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 65);

    // Press 63 — 63 < 65, voice stays at 65; no retrigger
    vm.processNoteOnEvent(0, 0, 63, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 65);
}

TEST_CASE("Mono Retrigger - Stealing Priority HIGHEST: release highest retrigs to held lower note")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::MONO_RETRIGGER |
                       (uint64_t)vm_t::MonoPlayModeFeatures::ON_RELEASE_TO_HIGHEST);
    vm.setMonoPriorityMode(0, vm_t::MonoPriorityMode::HIGHEST);

    // Press 60, 65, 63 — voice at 65 (63 doesn't win)
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
    vm.processNoteOnEvent(0, 0, 65, -1, 0.8, 0);
    vm.processNoteOnEvent(0, 0, 63, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 65);

    // Release 65 — retrigs to highest remaining held = 63
    vm.processNoteOffEvent(0, 0, 65, -1, 0.8);
    tp.processFor(1); // let terminated voice age out
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 63);

    // Release 63 — retrigs to 60
    vm.processNoteOffEvent(0, 0, 63, -1, 0.8);
    tp.processFor(1);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);

    // Release 60 — voice releases
    vm.processNoteOffEvent(0, 0, 60, -1, 0.8);
    REQUIRE_VOICE_COUNTS(1, 0);
    tp.processFor(10);
    REQUIRE_VOICE_COUNTS(0, 0);
}

// ---------------------------------------------------------------------------
// MONO_NOTES (retrigger) + LOWEST
// ---------------------------------------------------------------------------

TEST_CASE("Mono Retrigger - Stealing Priority LOWEST: lower note wins")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_MONO);
    vm.setMonoPriorityMode(0, vm_t::MonoPriorityMode::LOWEST);

    // Press 65 — voice at 65
    vm.processNoteOnEvent(0, 0, 65, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 65);

    // Press 60 — 60 < 65, old voice terminated, new voice at 60
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);

    // Press 63 — 63 > 60, voice stays at 60; no retrigger
    vm.processNoteOnEvent(0, 0, 63, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);
}

TEST_CASE("Mono Retrigger - Stealing Priority LOWEST: release lowest retrigs to held higher note")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::MONO_RETRIGGER |
                       (uint64_t)vm_t::MonoPlayModeFeatures::ON_RELEASE_TO_LOWEST);
    vm.setMonoPriorityMode(0, vm_t::MonoPriorityMode::LOWEST);

    // Press 65, 60, 63 — voice at 60 (63 doesn't win since 63 > 60)
    vm.processNoteOnEvent(0, 0, 65, -1, 0.8, 0);
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
    vm.processNoteOnEvent(0, 0, 63, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);

    // Release 60 — retrigs to lowest remaining held = 63
    vm.processNoteOffEvent(0, 0, 60, -1, 0.8);
    tp.processFor(1);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 63);

    // Release 63 — retrigs to 65
    vm.processNoteOffEvent(0, 0, 63, -1, 0.8);
    tp.processFor(1);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 65);

    // Release 65 — voice releases
    vm.processNoteOffEvent(0, 0, 65, -1, 0.8);
    REQUIRE_VOICE_COUNTS(1, 0);
    tp.processFor(10);
    REQUIRE_VOICE_COUNTS(0, 0);
}

// ---------------------------------------------------------------------------
// Sustain pedal interaction with MonoPriorityMode
//
// Scenario: sustain on, 60 on, 63 on, 60 off, 63 off
//   → voice is now sustained (gated by pedal only) at 63 in all three modes.
//     (LATEST/HIGHEST: voice moved to 63 on the 63-on event.
//      LOWEST: voice stayed at 60 on 63-on, then moved to 63 on 60-off via
//      release-to-latest retrigger, then became sustained on 63-off.)
//
// Then press 62:
//   LATEST  → 62 always wins, voice moves/retrigs to 62.
//   HIGHEST → 62 < 63, doesn't win, voice stays sustained at 63.
//   LOWEST  → 62 < 63, wins, voice moves/retrigs to 62.
// ---------------------------------------------------------------------------

TEST_CASE("Mono Legato - Sustain interaction with MonoPriorityMode")
{
    auto setup = [](auto &tp, auto &vm, auto mpm)
    {
        typedef TestPlayer<32, false>::voiceManager_t vm_t;
        vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                       (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_LEGATO);
        vm.setMonoPriorityMode(0, mpm);
        vm.updateSustainPedal(0, 0, 127);
        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
        vm.processNoteOnEvent(0, 0, 63, -1, 0.8, 0);
        vm.processNoteOffEvent(0, 0, 60, -1, 0.8);
        vm.processNoteOffEvent(0, 0, 63, -1, 0.8);
        // Voice is held by sustain — gated stays true while the pedal is down
        REQUIRE_VOICE_COUNTS(1, 1);
        REQUIRE_VOICE_MATCH(1, v.key() == 63);
    };

    SECTION("LATEST: new lower note takes over sustained voice")
    {
        auto tp = TestPlayer<32, false>();
        typedef TestPlayer<32, false>::voiceManager_t vm_t;
        auto &vm = tp.voiceManager;
        setup(tp, vm, vm_t::MonoPriorityMode::LATEST);

        vm.processNoteOnEvent(0, 0, 62, -1, 0.8, 0);
        REQUIRE_VOICE_COUNTS(1, 1);
        REQUIRE_VOICE_MATCH(1, v.key() == 62);
    }

    SECTION("HIGHEST: lower note does not take over sustained voice")
    {
        auto tp = TestPlayer<32, false>();
        typedef TestPlayer<32, false>::voiceManager_t vm_t;
        auto &vm = tp.voiceManager;
        setup(tp, vm, vm_t::MonoPriorityMode::HIGHEST);

        vm.processNoteOnEvent(0, 0, 62, -1, 0.8, 0);
        // 62 < 63, so HIGHEST priority means the sustained voice at 63 keeps playing
        REQUIRE_VOICE_COUNTS(1, 1);
        REQUIRE_VOICE_MATCH(1, v.key() == 63);
    }

    SECTION("LOWEST: new lower note takes over sustained voice")
    {
        auto tp = TestPlayer<32, false>();
        typedef TestPlayer<32, false>::voiceManager_t vm_t;
        auto &vm = tp.voiceManager;
        setup(tp, vm, vm_t::MonoPriorityMode::LOWEST);

        vm.processNoteOnEvent(0, 0, 62, -1, 0.8, 0);
        REQUIRE_VOICE_COUNTS(1, 1);
        REQUIRE_VOICE_MATCH(1, v.key() == 62);
    }
}

TEST_CASE("Mono Retrigger - Sustain interaction with MonoPriorityMode")
{
    auto setup = [](auto &tp, auto &vm, auto mpm)
    {
        typedef TestPlayer<32, false>::voiceManager_t vm_t;
        vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                       (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_MONO);
        vm.setMonoPriorityMode(0, mpm);
        vm.updateSustainPedal(0, 0, 127);
        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
        vm.processNoteOnEvent(0, 0, 63, -1, 0.8, 0);
        vm.processNoteOffEvent(0, 0, 60, -1, 0.8);
        vm.processNoteOffEvent(0, 0, 63, -1, 0.8);
        // Voice is held by sustain — gated stays true while the pedal is down
        REQUIRE_VOICE_COUNTS(1, 1);
        REQUIRE_VOICE_MATCH(1, v.key() == 63);
    };

    SECTION("LATEST: new lower note retrigs over sustained voice")
    {
        auto tp = TestPlayer<32, false>();
        typedef TestPlayer<32, false>::voiceManager_t vm_t;
        auto &vm = tp.voiceManager;
        setup(tp, vm, vm_t::MonoPriorityMode::LATEST);

        vm.processNoteOnEvent(0, 0, 62, -1, 0.8, 0);
        REQUIRE_VOICE_COUNTS(1, 1);
        REQUIRE_VOICE_MATCH(1, v.key() == 62);
    }

    SECTION("HIGHEST: lower note does not retrig over sustained voice")
    {
        auto tp = TestPlayer<32, false>();
        typedef TestPlayer<32, false>::voiceManager_t vm_t;
        auto &vm = tp.voiceManager;
        setup(tp, vm, vm_t::MonoPriorityMode::HIGHEST);

        vm.processNoteOnEvent(0, 0, 62, -1, 0.8, 0);
        // 62 < 63, so HIGHEST priority means the sustained voice at 63 keeps playing
        REQUIRE_VOICE_COUNTS(1, 1);
        REQUIRE_VOICE_MATCH(1, v.key() == 63);
    }

    SECTION("LOWEST: new lower note retrigs over sustained voice")
    {
        auto tp = TestPlayer<32, false>();
        typedef TestPlayer<32, false>::voiceManager_t vm_t;
        auto &vm = tp.voiceManager;
        setup(tp, vm, vm_t::MonoPriorityMode::LOWEST);

        vm.processNoteOnEvent(0, 0, 62, -1, 0.8, 0);
        REQUIRE_VOICE_COUNTS(1, 1);
        REQUIRE_VOICE_MATCH(1, v.key() == 62);
    }
}

// ---------------------------------------------------------------------------
// LATEST still works as before (regression guard)
// ---------------------------------------------------------------------------

TEST_CASE("Mono Legato - Priority LATEST: any new note always wins (default behaviour)")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_LEGATO);
    // LATEST is the default; set it explicitly for clarity
    vm.setMonoPriorityMode(0, vm_t::MonoPriorityMode::LATEST);

    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);

    // Lower note still takes over with LATEST
    vm.processNoteOnEvent(0, 0, 55, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 55);

    // Higher note takes over with LATEST
    vm.processNoteOnEvent(0, 0, 70, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 70);

    // Another lower note takes over
    vm.processNoteOnEvent(0, 0, 50, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 50);
}

TEST_CASE("Mono Retrigger - Priority LATEST: any new note always retrigs (default behaviour)")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_MONO);
    vm.setMonoPriorityMode(0, vm_t::MonoPriorityMode::LATEST);

    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);

    vm.processNoteOnEvent(0, 0, 55, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 55);

    vm.processNoteOnEvent(0, 0, 70, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 70);
}

// ---------------------------------------------------------------------------
// Releasing (ungated) voice does not block new notes in HIGHEST / LOWEST mode.
//
// Scenario: press 60, release 60 (voice still sounding but ungated), press 62.
// Even though 62 would not normally win over a gated 60 in LOWEST mode, the
// existing voice is no longer gated so the new note should always win.
// Same logic applies to HIGHEST mode (release 65, press 60 should win).
// ---------------------------------------------------------------------------

TEST_CASE("Mono Legato - LOWEST: releasing voice does not block higher new note")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_LEGATO);
    vm.setMonoPriorityMode(0, vm_t::MonoPriorityMode::LOWEST);

    // Press 60 — voice at 60, gated
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);

    // Release 60 — voice now releasing (ungated), still sounding
    vm.processNoteOffEvent(0, 0, 60, -1, 0.8);
    REQUIRE_VOICE_COUNTS(1, 0);

    // Press 62 — 62 > 60 would normally lose in LOWEST mode, but 60 is no longer
    // gated so the new note must win and the voice should move to 62
    vm.processNoteOnEvent(0, 0, 62, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 62);
}

TEST_CASE("Mono Legato - HIGHEST: releasing voice does not block lower new note")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_LEGATO);
    vm.setMonoPriorityMode(0, vm_t::MonoPriorityMode::HIGHEST);

    // Press 65 — voice at 65, gated
    vm.processNoteOnEvent(0, 0, 65, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 65);

    // Release 65 — voice now releasing (ungated), still sounding
    vm.processNoteOffEvent(0, 0, 65, -1, 0.8);
    REQUIRE_VOICE_COUNTS(1, 0);

    // Press 60 — 60 < 65 would normally lose in HIGHEST mode, but 65 is no longer
    // gated so the new note must win and the voice should move to 60
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);
}

TEST_CASE("Mono Retrigger - LOWEST: releasing voice does not block higher new note")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_MONO);
    vm.setMonoPriorityMode(0, vm_t::MonoPriorityMode::LOWEST);

    // Press 60 — voice at 60, gated
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);

    // Release 60 — voice now releasing (ungated), still sounding
    vm.processNoteOffEvent(0, 0, 60, -1, 0.8);
    REQUIRE_VOICE_COUNTS(1, 0);

    // Press 62 — 62 > 60 would normally lose in LOWEST mode, but 60 is no longer
    // gated so the new note must win and a new voice should appear at 62
    vm.processNoteOnEvent(0, 0, 62, -1, 0.8, 0);
    tp.processFor(1);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 62);
}

TEST_CASE("Mono Retrigger - HIGHEST: releasing voice does not block lower new note")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_MONO);
    vm.setMonoPriorityMode(0, vm_t::MonoPriorityMode::HIGHEST);

    // Press 65 — voice at 65, gated
    vm.processNoteOnEvent(0, 0, 65, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 65);

    // Release 65 — voice now releasing (ungated), still sounding
    vm.processNoteOffEvent(0, 0, 65, -1, 0.8);
    REQUIRE_VOICE_COUNTS(1, 0);

    // Press 60 — 60 < 65 would normally lose in HIGHEST mode, but 65 is no longer
    // gated so the new note must win and a new voice should appear at 60
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
    tp.processFor(1);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);
}

// ---------------------------------------------------------------------------
// Repeated note (same key re-pressed while still alive) with HIGHEST / LOWEST
//
// Scenario: play C, release C (voice still releasing), play C again.
// A repeated note is neither higher nor lower — it is equal — so it should
// always win and retrigger the voice regardless of priority mode.
// ---------------------------------------------------------------------------

TEST_CASE("Mono Legato - HIGHEST: repeated note (same key) always wins")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_LEGATO);
    vm.setMonoPriorityMode(0, vm_t::MonoPriorityMode::HIGHEST);

    // Press 60 — voice starts at 60
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);

    // Release 60 — voice begins releasing (still alive)
    vm.processNoteOffEvent(0, 0, 60, -1, 0.8);
    REQUIRE_VOICE_COUNTS(1, 0);

    // Press 60 again while it is still releasing — equal key should always win
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);
}

TEST_CASE("Mono Legato - LOWEST: repeated note (same key) always wins")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_LEGATO);
    vm.setMonoPriorityMode(0, vm_t::MonoPriorityMode::LOWEST);

    // Press 60 — voice starts at 60
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);

    // Release 60 — voice begins releasing (still alive)
    vm.processNoteOffEvent(0, 0, 60, -1, 0.8);
    REQUIRE_VOICE_COUNTS(1, 0);

    // Press 60 again while it is still releasing — equal key should always win
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);
}

TEST_CASE("Mono Retrigger - HIGHEST: repeated note (same key) always wins")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_MONO);
    vm.setMonoPriorityMode(0, vm_t::MonoPriorityMode::HIGHEST);

    // Press 60 — voice starts at 60
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);

    // Release 60 — voice begins releasing (still alive)
    vm.processNoteOffEvent(0, 0, 60, -1, 0.8);
    REQUIRE_VOICE_COUNTS(1, 0);

    // Press 60 again while it is still releasing — equal key should always win
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);
}

TEST_CASE("Mono Retrigger - LOWEST: repeated note (same key) always wins")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_MONO);
    vm.setMonoPriorityMode(0, vm_t::MonoPriorityMode::LOWEST);

    // Press 60 — voice starts at 60
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);

    // Release 60 — voice begins releasing (still alive)
    vm.processNoteOffEvent(0, 0, 60, -1, 0.8);
    REQUIRE_VOICE_COUNTS(1, 0);

    // Press 60 again while it is still releasing — equal key should always win
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);
}
