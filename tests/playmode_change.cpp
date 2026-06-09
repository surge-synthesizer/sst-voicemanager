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

TEST_CASE("Playmode Change POLY to MONO Terminates Group")
{
    INFO("Switching a group from poly to mono while voices sound terminates the sounding "
         "voices, and subsequent play follows the new mono mode.");

    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    REQUIRE_NO_VOICES;

    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
    vm.processNoteOnEvent(0, 0, 64, -1, 0.8, 0.0);
    vm.processNoteOnEvent(0, 0, 67, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(3, 3);

    // Flip to mono: all three poly voices are terminated at once
    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_MONO);
    REQUIRE_NO_VOICES;

    // And the group is now mono: a second note-on does not stack a voice
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(1, 1);
    vm.processNoteOnEvent(0, 0, 64, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 64; }) == 1);
}

TEST_CASE("Playmode Change MONO to POLY Terminates Group")
{
    INFO("Switching a group from mono to poly while a voice sounds terminates it, and "
         "subsequent play is polyphonic.");

    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    // Establishing mono with no active voices terminates nothing
    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_MONO);
    REQUIRE_NO_VOICES;

    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
    vm.processNoteOnEvent(0, 0, 64, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(1, 1);

    // Flip back to poly: the sounding mono voice is terminated
    vm.setPlaymode(0, vm_t::PlayMode::POLY_VOICES);
    REQUIRE_NO_VOICES;

    // And the group is now poly: two note-ons stack two voices
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
    vm.processNoteOnEvent(0, 0, 64, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(2, 2);
}

TEST_CASE("Playmode Change Clears Held Key State")
{
    INFO("Changing mode clears the group's held-key state so a key that was held before "
         "the change is not later picked as a mono-retrigger target for a new voice.");

    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_LEGATO);

    // Key 48 is physically held and recorded in the group's held-key state
    vm.processNoteOnEvent(0, 0, 48, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(1, 1);

    // Mode change (LEGATO -> RETRIGGER): voice terminates and held-key state is cleared,
    // so the still-held key 48 is forgotten
    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_MONO);
    REQUIRE_NO_VOICES;

    // A fresh note, then its release. With held-key state cleared there is no other held
    // key to fall back to, so releasing 60 ends the voice. If 48's stale state survived,
    // the retrigger would resurrect a voice on key 48.
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(1, 1);
    vm.processNoteOffEvent(0, 0, 60, -1, 0.8);
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 48; }) == 0);
    tp.processFor(10);
    REQUIRE_NO_VOICES;
}

TEST_CASE("Playmode Change Is A No-op When Mode Is Unchanged")
{
    INFO("Re-asserting the current mode and features must not disturb sounding voices.");

    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
    vm.processNoteOnEvent(0, 0, 64, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(2, 2);

    // Default group is already POLY_VOICES with no features; setting the same is a no-op
    vm.setPlaymode(0, vm_t::PlayMode::POLY_VOICES);
    REQUIRE_VOICE_COUNTS(2, 2);
}

TEST_CASE("Playmode Change Delayed Termination")
{
    INFO("Under delayed termination the mode-change voices are slated for terminate rather "
         "than reaped instantly.");

    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    tp.terminateInstantly = false;
    auto &vm = tp.voiceManager;

    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
    vm.processNoteOnEvent(0, 0, 64, -1, 0.8, 0.0);
    vm.processNoteOnEvent(0, 0, 67, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(3, 3);

    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_MONO);

    // Delayed model: the callback has not fired, so the slots are still counted but all
    // three are marked for termination
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.slatedForTerminate; }) == 3);
}
