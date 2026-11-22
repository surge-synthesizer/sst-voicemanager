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

TEST_CASE("Wildcard Note Off - Poly")
{
    auto tp = TestPlayer<32, false>();
    auto &vm = tp.voiceManager;

    SECTION("Any channel releases the key on every channel")
    {
        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
        vm.processNoteOnEvent(0, 1, 60, -1, 0.8, 0);
        vm.processNoteOnEvent(0, 0, 62, -1, 0.8, 0);
        REQUIRE_VOICE_COUNTS(3, 3);

        vm.processNoteOffEvent(0, -1, 60, -1, 0.8);
        REQUIRE_VOICE_COUNTS(3, 1);
        REQUIRE_VOICE_MATCH(1, v.isGated && v.key() == 62);
    }

    SECTION("Any key releases every key on the channel")
    {
        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
        vm.processNoteOnEvent(0, 0, 62, -1, 0.8, 0);
        vm.processNoteOnEvent(0, 1, 64, -1, 0.8, 0);
        REQUIRE_VOICE_COUNTS(3, 3);

        vm.processNoteOffEvent(0, 0, -1, -1, 0.8);
        REQUIRE_VOICE_COUNTS(3, 1);
        REQUIRE_VOICE_MATCH(1, v.isGated && v.key() == 64);
    }

    SECTION("Any port releases the key")
    {
        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
        REQUIRE_VOICE_COUNTS(1, 1);

        vm.processNoteOffEvent(-1, 0, 60, -1, 0.8);
        REQUIRE_VOICE_COUNTS(1, 0);
    }

    SECTION("A note id alone releases its note")
    {
        vm.processNoteOnEvent(0, 0, 60, 17, 0.8, 0);
        vm.processNoteOnEvent(0, 0, 62, 18, 0.8, 0);
        REQUIRE_VOICE_COUNTS(2, 2);

        vm.processNoteOffEvent(-1, -1, -1, 17, 0.8);
        REQUIRE_VOICE_COUNTS(2, 1);
        REQUIRE_VOICE_MATCH(1, v.isGated && v.key() == 62);
    }
}

TEST_CASE("Wildcard Note Off - Mono Legato")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_LEGATO);

    SECTION("Any channel releases a single key")
    {
        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
        REQUIRE_VOICE_COUNTS(1, 1);

        vm.processNoteOffEvent(0, -1, 60, -1, 0.8);
        REQUIRE_VOICE_COUNTS(1, 0);
        tp.processFor(10);
        REQUIRE_NO_VOICES;
    }

    SECTION("Any key releases a single key")
    {
        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
        REQUIRE_VOICE_COUNTS(1, 1);

        vm.processNoteOffEvent(0, 0, -1, -1, 0.8);
        REQUIRE_VOICE_COUNTS(1, 0);
        tp.processFor(10);
        REQUIRE_NO_VOICES;
    }

    SECTION("A released key does not return when a later note ends")
    {
        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
        vm.processNoteOffEvent(0, -1, 60, -1, 0.8);
        tp.processFor(10);
        REQUIRE_NO_VOICES;

        vm.processNoteOnEvent(0, 0, 62, -1, 0.8, 0);
        REQUIRE_VOICE_COUNTS(1, 1);
        vm.processNoteOffEvent(0, 0, 62, -1, 0.8);
        REQUIRE_VOICE_COUNTS(1, 0);
        tp.processFor(10);
        REQUIRE_NO_VOICES;
    }
}
