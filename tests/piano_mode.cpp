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

TEST_CASE("Poly Multi Key Piano Mode")
{
    SECTION("Single Voice per Key")
    {
        auto tp = TestPlayer<32>();
        auto &vm = tp.voiceManager;
        vm.repeatedKeyMode = TestPlayer<32>::voiceManager_t::RepeatedKeyMode::PIANO;

        REQUIRE_NO_VOICES;

        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(1, 1);
        tp.processFor(3);
        vm.processNoteOffEvent(0, 0, 60, -1, 0.2);
        REQUIRE_VOICE_COUNTS(1, 0);
        tp.processFor(2);
        REQUIRE_VOICE_COUNTS(1, 0);
        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(1, 1);
    }

    SECTION("Single Voice per Key")
    {
        auto tp = TestPlayer<32>();
        auto &vm = tp.voiceManager;
        vm.repeatedKeyMode = TestPlayer<32>::voiceManager_t::RepeatedKeyMode::PIANO;

        REQUIRE_NO_VOICES;

        vm.processNoteOnEvent(0, 0, 90, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(3, 3);
        tp.processFor(3);
        vm.processNoteOffEvent(0, 0, 90, -1, 0.2);
        REQUIRE_VOICE_COUNTS(3, 0);
        tp.processFor(2);
        REQUIRE_VOICE_COUNTS(3, 0);
        vm.processNoteOnEvent(0, 0, 90, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(3, 3);
    }
}

TEST_CASE("Poly Multi Key Non Piano Mode")
{
    SECTION("Single Voice per Key")
    {
        auto tp = TestPlayer<32>();
        auto &vm = tp.voiceManager;
        REQUIRE(vm.repeatedKeyMode == TestPlayer<32>::voiceManager_t::RepeatedKeyMode::MULTI_VOICE);

        REQUIRE_NO_VOICES;

        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(1, 1);
        tp.processFor(3);
        vm.processNoteOffEvent(0, 0, 60, -1, 0.2);
        REQUIRE_VOICE_COUNTS(1, 0);
        tp.processFor(2);
        REQUIRE_VOICE_COUNTS(1, 0);

        INFO("This note on event creates a new gated voice while the old one rings out");
        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(2, 1);

        INFO("And then the ungated one release away after time");
        tp.processFor(10);
        REQUIRE_VOICE_COUNTS(1, 1);
    }

    SECTION("Triple Voice per Key")
    {
        auto tp = TestPlayer<32>();
        auto &vm = tp.voiceManager;
        REQUIRE(vm.repeatedKeyMode == TestPlayer<32>::voiceManager_t::RepeatedKeyMode::MULTI_VOICE);

        REQUIRE_NO_VOICES;

        vm.processNoteOnEvent(0, 0, 90, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(3, 3);
        tp.processFor(3);
        vm.processNoteOffEvent(0, 0, 90, -1, 0.2);
        REQUIRE_VOICE_COUNTS(3, 0);
        tp.processFor(2);
        REQUIRE_VOICE_COUNTS(3, 0);

        INFO("This note on event creates a new gated voice while the old one rings out");
        vm.processNoteOnEvent(0, 0, 90, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(6, 3);

        INFO("And then the ungated one release away after time");
        tp.processFor(10);
        REQUIRE_VOICE_COUNTS(3, 3);
    }
}

TEST_CASE("Piano Mode Sustain Pedal")
{
    SECTION("Single notes, no retrig, sustain")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;
        REQUIRE_NO_VOICES;

        vm.dialect = TestPlayer<32>::voiceManager_t::MIDI1Dialect::MIDI1;
        vm.repeatedKeyMode = TestPlayer<32>::voiceManager_t::RepeatedKeyMode::PIANO;

        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(1, 1);
        tp.processFor(10);
        REQUIRE_VOICE_COUNTS(1, 1);
        vm.updateSustainPedal(0, 0, 120);
        tp.processFor(10);
        REQUIRE_VOICE_COUNTS(1, 1);
        vm.processNoteOffEvent(0, 0, 60, -1, 0.4);
        REQUIRE_VOICE_COUNTS(1, 1);
        tp.processFor(40);
        REQUIRE_VOICE_COUNTS(1, 1);

        vm.updateSustainPedal(0, 0, 0);
        REQUIRE_VOICE_COUNTS(1, 0);
        tp.processFor(20);

        REQUIRE_NO_VOICES;
    }

    SECTION("Multiple notes, no retrig, sustain")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;
        REQUIRE_NO_VOICES;

        vm.dialect = TestPlayer<32>::voiceManager_t::MIDI1Dialect::MIDI1;
        vm.repeatedKeyMode = TestPlayer<32>::voiceManager_t::RepeatedKeyMode::PIANO;

        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
        vm.processNoteOnEvent(0, 0, 64, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(2, 2);
        tp.processFor(10);
        REQUIRE_VOICE_COUNTS(2, 2);
        vm.updateSustainPedal(0, 0, 120);
        tp.processFor(10);
        REQUIRE_VOICE_COUNTS(2, 2);
        vm.processNoteOffEvent(0, 0, 60, -1, 0.4);
        REQUIRE_VOICE_COUNTS(2, 2);
        tp.processFor(40);
        REQUIRE_VOICE_COUNTS(2, 2);

        vm.updateSustainPedal(0, 0, 0);
        REQUIRE_VOICE_COUNTS(2, 1);
        tp.processFor(20);
        REQUIRE_VOICE_COUNTS(1, 1);

        vm.processNoteOffEvent(0, 0, 64, -1, 0.4);
        REQUIRE_VOICE_COUNTS(1, 0);

        tp.processFor(20);
        REQUIRE_NO_VOICES;
    }

    SECTION("Retrigger a note under sustain and release during sustain")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;
        REQUIRE_NO_VOICES;

        vm.dialect = TestPlayer<32>::voiceManager_t::MIDI1Dialect::MIDI1;
        vm.repeatedKeyMode = TestPlayer<32>::voiceManager_t::RepeatedKeyMode::PIANO;

        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(1, 1);
        tp.processFor(10);
        REQUIRE_VOICE_COUNTS(1, 1);
        vm.updateSustainPedal(0, 0, 120);
        tp.processFor(10);
        REQUIRE_VOICE_COUNTS(1, 1);
        vm.processNoteOffEvent(0, 0, 60, -1, 0.4);
        REQUIRE_VOICE_COUNTS(1, 1);
        tp.processFor(40);
        REQUIRE_VOICE_COUNTS(1, 1);

        INFO("About to retrigger");
        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(1, 1);
        tp.processFor(40);

        vm.processNoteOffEvent(0, 0, 60, -1, 0.8);
        REQUIRE_VOICE_COUNTS(1, 1);
        tp.processFor(40);

        vm.updateSustainPedal(0, 0, 0);
        REQUIRE_VOICE_COUNTS(1, 0);
        tp.processFor(20);
        REQUIRE_VOICE_COUNTS(0, 0);

        tp.processFor(20);
        REQUIRE_NO_VOICES;
    }

    SECTION("Retrigger a note under sustain and release outside sustain")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;
        REQUIRE_NO_VOICES;

        vm.dialect = TestPlayer<32>::voiceManager_t::MIDI1Dialect::MIDI1;
        vm.repeatedKeyMode = TestPlayer<32>::voiceManager_t::RepeatedKeyMode::PIANO;

        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(1, 1);
        tp.processFor(10);
        REQUIRE_VOICE_COUNTS(1, 1);
        vm.updateSustainPedal(0, 0, 120);
        tp.processFor(10);
        REQUIRE_VOICE_COUNTS(1, 1);
        vm.processNoteOffEvent(0, 0, 60, -1, 0.4);
        REQUIRE_VOICE_COUNTS(1, 1);
        tp.processFor(40);
        REQUIRE_VOICE_COUNTS(1, 1);

        INFO("About to retrigger");
        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(1, 1);
        tp.processFor(40);

        vm.updateSustainPedal(0, 0, 0);
        REQUIRE_VOICE_COUNTS(1, 1);
        tp.processFor(10);
        vm.processNoteOffEvent(0, 0, 60, -1, 0.8);
        REQUIRE_VOICE_COUNTS(1, 0);

        tp.processFor(20);
        REQUIRE_NO_VOICES;
    }
}
