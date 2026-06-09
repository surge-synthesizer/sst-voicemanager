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
 * processNoteOnEvent returns a bool. The contract: it returns true only when the
 * note-on was handled by the PIANO fast-retrigger path (an existing, non-gated voice
 * for the same key was re-gated rather than a new voice being created). Every path
 * that runs the normal begin/init voice-creation transaction returns false - including
 * fresh poly notes and all mono note-ons (even when the mono voice is moved or skipped).
 */

TEST_CASE("processNoteOnEvent Return - Poly")
{
    TestPlayer<32> tp;
    auto &vm = tp.voiceManager;
    REQUIRE_NO_VOICES;

    // A fresh poly note creates a voice via the normal path -> false
    REQUIRE(vm.processNoteOnEvent(0, 0, 60, -1, 0.8f, 0.f) == false);
    REQUIRE_VOICE_COUNTS(1, 1);

    // A second, distinct poly note also goes through the normal path -> false
    REQUIRE(vm.processNoteOnEvent(0, 0, 64, -1, 0.8f, 0.f) == false);
    REQUIRE_VOICE_COUNTS(2, 2);

    // Re-striking a still-gated key in the default MULTI_VOICE mode just makes another
    // voice (no piano retrigger) -> false
    REQUIRE(vm.processNoteOnEvent(0, 0, 60, -1, 0.8f, 0.f) == false);
    REQUIRE_VOICE_COUNTS(3, 3);
}

TEST_CASE("processNoteOnEvent Return - Piano Mode Retrigger")
{
    using vm_t = TestPlayer<32>::voiceManager_t;

    SECTION("Retrigger Of A Releasing Key Returns True (no note id)")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;
        vm.repeatedKeyMode = vm_t::RepeatedKeyMode::PIANO;
        REQUIRE_NO_VOICES;

        // First strike: no voice exists yet, so this falls through to normal creation
        REQUIRE(vm.processNoteOnEvent(0, 0, 60, -1, 0.8f, 0.f) == false);
        REQUIRE_VOICE_COUNTS(1, 1);

        // Release it; the voice is now active but releasing (ungated)
        vm.processNoteOffEvent(0, 0, 60, -1, 0.2f);
        tp.processFor(1);
        REQUIRE_VOICE_COUNTS(1, 0);

        // Striking the same key while it is releasing retriggers in place -> true
        REQUIRE(vm.processNoteOnEvent(0, 0, 60, -1, 0.8f, 0.f) == true);
        REQUIRE_VOICE_COUNTS(1, 1);
    }

    SECTION("Retrigger Of A Releasing Key Returns True (with note ids)")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;
        vm.repeatedKeyMode = vm_t::RepeatedKeyMode::PIANO;
        REQUIRE_NO_VOICES;

        REQUIRE(vm.processNoteOnEvent(0, 0, 60, 1000, 0.8f, 0.f) == false);
        REQUIRE_VOICE_COUNTS(1, 1);

        vm.processNoteOffEvent(0, 0, 60, 1000, 0.2f);
        tp.processFor(1);
        REQUIRE_VOICE_COUNTS(1, 0);

        REQUIRE(vm.processNoteOnEvent(0, 0, 60, 1001, 0.8f, 0.f) == true);
        REQUIRE_VOICE_COUNTS(1, 1);
    }

    SECTION("Sustained Key Retrigger Returns True")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;
        vm.repeatedKeyMode = vm_t::RepeatedKeyMode::PIANO;
        REQUIRE_NO_VOICES;

        REQUIRE(vm.processNoteOnEvent(0, 0, 60, -1, 0.8f, 0.f) == false);
        vm.updateSustainPedal(0, 0, 127);
        vm.processNoteOffEvent(0, 0, 60, -1, 0.2f);
        // Still gated, but only by sustain
        REQUIRE_VOICE_COUNTS(1, 1);

        // Re-striking the sustain-held key retriggers it -> true
        REQUIRE(vm.processNoteOnEvent(0, 0, 60, -1, 0.8f, 0.f) == true);
        REQUIRE_VOICE_COUNTS(1, 1);
    }
}

TEST_CASE("processNoteOnEvent Return - Mono Modes")
{
    using vm_t = TestPlayer<32>::voiceManager_t;
    using MF = vm_t::MonoPlayModeFeatures;

    SECTION("NATURAL_MONO Always Returns False")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;
        vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES, (uint64_t)MF::NATURAL_MONO);
        REQUIRE_NO_VOICES;

        // First mono note runs the normal creation path -> false
        REQUIRE(vm.processNoteOnEvent(0, 0, 60, -1, 0.8f, 0.f) == false);
        REQUIRE_VOICE_COUNTS(1, 1);

        // A second mono note steals/recreates through the transaction -> false
        REQUIRE(vm.processNoteOnEvent(0, 0, 64, -1, 0.8f, 0.f) == false);
        REQUIRE_VOICE_COUNTS(1, 1);
    }

    SECTION("NATURAL_LEGATO Always Returns False")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;
        vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES, (uint64_t)MF::NATURAL_LEGATO);
        REQUIRE_NO_VOICES;

        REQUIRE(vm.processNoteOnEvent(0, 0, 60, -1, 0.8f, 0.f) == false);
        REQUIRE_VOICE_COUNTS(1, 1);

        // Legato move of the existing voice still reports false (the begin/init
        // transaction ran even though the voice was moved rather than created)
        REQUIRE(vm.processNoteOnEvent(0, 0, 64, -1, 0.8f, 0.f) == false);
        REQUIRE_VOICE_COUNTS(1, 1);
    }
}
