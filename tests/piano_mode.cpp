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

TEST_CASE("Piano Mode Voice Stealing")
{
    SECTION("Retrigger Of Releasing Voice Does Not Steal")
    {
        INFO("When a key is re-pressed in PIANO mode and its voice is still releasing, the voice "
             "manager retriggeres the existing slot in-place without stealing any other voice. "
             "The total voice count must not change.");

        auto tp = TestPlayer<4>(); // tight pool so any accidental steal is visible
        auto &vm = tp.voiceManager;
        vm.repeatedKeyMode = TestPlayer<4>::voiceManager_t::RepeatedKeyMode::PIANO;

        // Fill all four slots with keys 60-63
        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
        vm.processNoteOnEvent(0, 0, 61, -1, 0.8, 0.0);
        vm.processNoteOnEvent(0, 0, 62, -1, 0.8, 0.0);
        vm.processNoteOnEvent(0, 0, 63, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(4, 4);

        // Release key 60 — its voice enters the release tail (releaseCountdown=5)
        vm.processNoteOffEvent(0, 0, 60, -1, 0.4);
        REQUIRE_VOICE_COUNTS(4, 3);

        // Re-press key 60 in piano mode: must retrigger the releasing voice, not steal
        vm.processNoteOnEvent(0, 0, 60, -1, 0.9, 0.0);
        REQUIRE_VOICE_COUNTS(4, 4); // still 4 total, all gated again

        REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 60 && v.isGated; }) == 1);
        REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 61; }) == 1);
        REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 62; }) == 1);
        REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 63; }) == 1);
    }

    SECTION("New Key In Full Pool Steals Oldest Voice")
    {
        INFO("When a genuinely new key is pressed in PIANO mode and the physical pool is full, "
             "the normal oldest-first stealing logic runs and the new key takes the freed slot.");

        auto tp = TestPlayer<4>();
        auto &vm = tp.voiceManager;
        vm.repeatedKeyMode = TestPlayer<4>::voiceManager_t::RepeatedKeyMode::PIANO;

        // Fill pool with keys 60-63; key 60 is oldest (lowest voiceCounter)
        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
        tp.process(); // advance time so voiceCounters are clearly ordered
        vm.processNoteOnEvent(0, 0, 61, -1, 0.8, 0.0);
        tp.process();
        vm.processNoteOnEvent(0, 0, 62, -1, 0.8, 0.0);
        tp.process();
        vm.processNoteOnEvent(0, 0, 63, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(4, 4);

        // Key 64 is new — no existing voice to retrigger, so stealing must occur
        vm.processNoteOnEvent(0, 0, 64, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(4, 4); // pool stays at 4 after steal + create

        REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 60; }) == 0); // stolen
        REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 64; }) == 1); // new
        REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 61; }) == 1);
        REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 62; }) == 1);
        REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 63; }) == 1);
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
