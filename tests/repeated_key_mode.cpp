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

TEST_CASE("Repeated Key Mode Is Per Group")
{
    using vm_t = TestPlayer<32>::voiceManager_t;
    TestPlayer<32> tp;
    auto &vm = tp.voiceManager;

    REQUIRE(vm.getRepeatedKeyMode(0) == vm_t::RepeatedKeyMode::MULTI_VOICE);
    REQUIRE(vm.getRepeatedKeyMode(1234) == vm_t::RepeatedKeyMode::MULTI_VOICE);

    vm.setRepeatedKeyMode(1234, vm_t::RepeatedKeyMode::MONO_PER_KEY);
    REQUIRE(vm.getRepeatedKeyMode(0) == vm_t::RepeatedKeyMode::MULTI_VOICE);
    REQUIRE(vm.getRepeatedKeyMode(1234) == vm_t::RepeatedKeyMode::MONO_PER_KEY);
}

TEST_CASE("Mono Per Key")
{
    using vm_t = TestPlayer<32>::voiceManager_t;

    SECTION("Restrike while releasing starts a fresh voice")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;
        vm.setRepeatedKeyMode(0, vm_t::RepeatedKeyMode::MONO_PER_KEY);

        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(1, 1);
        auto firstCreation = tp.lastCreationCount;
        vm.processNoteOffEvent(0, 0, 60, -1, 0.4);
        tp.processFor(1);
        REQUIRE_VOICE_COUNTS(1, 0);

        REQUIRE(vm.processNoteOnEvent(0, 0, 60, -1, 0.9, 0.0) == false);
        REQUIRE_VOICE_COUNTS(1, 1);
        REQUIRE(tp.lastCreationCount == firstCreation + 1);
        REQUIRE_VOICE_MATCH(1, v.key() == 60 && v.velocity == 0.9f);
    }

    SECTION("Restrike while held ends the held voice")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;
        vm.setRepeatedKeyMode(0, vm_t::RepeatedKeyMode::MONO_PER_KEY);

        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(1, 1);
    }

    SECTION("Multi voice still stacks for comparison")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;

        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(2, 2);
    }

    SECTION("Other keys are untouched")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;
        vm.setRepeatedKeyMode(0, vm_t::RepeatedKeyMode::MONO_PER_KEY);

        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
        vm.processNoteOnEvent(0, 0, 62, -1, 0.8, 0.0);
        vm.processNoteOffEvent(0, 0, 62, -1, 0.4);
        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(2, 1);
        REQUIRE_KEY_COUNT(1, 60);
        REQUIRE_KEY_COUNT(1, 62);
    }

    SECTION("Other channels are untouched")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;
        vm.setRepeatedKeyMode(0, vm_t::RepeatedKeyMode::MONO_PER_KEY);

        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
        vm.processNoteOnEvent(0, 1, 60, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(2, 2);
    }

    SECTION("Multi voice notes are replaced as a whole")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;
        vm.setRepeatedKeyMode(0, vm_t::RepeatedKeyMode::MONO_PER_KEY);

        // keys over 72 make three voices in the test player
        vm.processNoteOnEvent(0, 0, 80, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(3, 3);
        vm.processNoteOffEvent(0, 0, 80, -1, 0.4);
        vm.processNoteOnEvent(0, 0, 80, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(3, 3);
    }

    SECTION("Sustain pedal does not pile up voices")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;
        vm.setRepeatedKeyMode(0, vm_t::RepeatedKeyMode::MONO_PER_KEY);

        vm.updateSustainPedal(0, 0, 120);
        for (int i = 0; i < 4; ++i)
        {
            vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
            vm.processNoteOffEvent(0, 0, 60, -1, 0.4);
            tp.processFor(2);
        }
        REQUIRE_VOICE_COUNTS(1, 1);

        vm.updateSustainPedal(0, 0, 0);
        REQUIRE_VOICE_COUNTS(1, 0);
        tp.processFor(10);
        REQUIRE_NO_VOICES;
    }

    SECTION("Sustain pedal piles up in multi voice for comparison")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;

        vm.updateSustainPedal(0, 0, 120);
        for (int i = 0; i < 4; ++i)
        {
            vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
            vm.processNoteOffEvent(0, 0, 60, -1, 0.4);
            tp.processFor(2);
        }
        REQUIRE_VOICE_COUNTS(4, 4);
    }

    SECTION("Ignored by a mono group")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;
        vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                       (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_MONO);
        vm.setRepeatedKeyMode(0, vm_t::RepeatedKeyMode::MONO_PER_KEY);

        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
        vm.processNoteOnEvent(0, 0, 64, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(1, 1);
        vm.processNoteOffEvent(0, 0, 64, -1, 0.4);
        REQUIRE_VOICE_COUNTS(1, 1);
        REQUIRE_VOICE_MATCH(1, v.key() == 60);
    }
}

TEST_CASE("Mono Per Key With Delayed Termination")
{
    using vm_t = TestPlayer<32>::voiceManager_t;

    SECTION("Ending the old voice frees its slot rather than stealing another")
    {
        TestPlayer<32> tp;
        tp.terminateInstantly = false;
        auto &vm = tp.voiceManager;
        vm.setPolyphonyGroupVoiceLimit(0, 4);
        vm.setRepeatedKeyMode(0, vm_t::RepeatedKeyMode::MONO_PER_KEY);

        for (int k = 60; k < 64; ++k)
            vm.processNoteOnEvent(0, 0, k, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(4, 4);

        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
        // the old 60 is still counted until its end callback fires
        REQUIRE(vm.getVoiceCount() == 5);
        REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.slatedForTerminate; }) == 1);
        REQUIRE(tp.activeVoicesMatching([](auto &v)
                                        { return v.slatedForTerminate && v.key() == 60; }) == 1);
    }

    SECTION("A new key into a full group still steals")
    {
        TestPlayer<32> tp;
        tp.terminateInstantly = false;
        auto &vm = tp.voiceManager;
        vm.setPolyphonyGroupVoiceLimit(0, 4);
        vm.setRepeatedKeyMode(0, vm_t::RepeatedKeyMode::MONO_PER_KEY);

        for (int k = 60; k < 64; ++k)
            vm.processNoteOnEvent(0, 0, k, -1, 0.8, 0.0);
        vm.processNoteOnEvent(0, 0, 64, -1, 0.8, 0.0);
        REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.slatedForTerminate; }) == 1);
        REQUIRE(tp.activeVoicesMatching([](auto &v)
                                        { return v.slatedForTerminate && v.key() == 60; }) == 1);
    }

    SECTION("Restriking again does not end the same voice twice")
    {
        TestPlayer<32> tp;
        tp.terminateInstantly = false;
        auto &vm = tp.voiceManager;
        vm.setPolyphonyGroupVoiceLimit(0, 4);
        vm.setRepeatedKeyMode(0, vm_t::RepeatedKeyMode::MONO_PER_KEY);

        for (int k = 60; k < 64; ++k)
            vm.processNoteOnEvent(0, 0, k, -1, 0.8, 0.0);
        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
        // both earlier 60s are ending and no other key was taken
        REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.slatedForTerminate; }) == 2);
        REQUIRE(tp.activeVoicesMatching([](auto &v)
                                        { return v.slatedForTerminate && v.key() != 60; }) == 0);
    }
}

TEST_CASE("Repeated Key Modes Across Layered Groups")
{
    using vm_t = TwoGroupsEveryKey<32>::voiceManager_t;

    SECTION("Mono per key group layered with a multi voice group")
    {
        auto tp = TwoGroupsEveryKey<32>();
        auto &vm = tp.voiceManager;
        vm.setRepeatedKeyMode(2112, vm_t::RepeatedKeyMode::MONO_PER_KEY);

        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(2, 2);
        vm.processNoteOffEvent(0, 0, 60, -1, 0.4);
        tp.processFor(1);
        REQUIRE_VOICE_COUNTS(2, 0);

        // 2112 replaces its voice, 90125 adds one next to its releasing voice
        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(3, 2);
        tp.processFor(10);
        REQUIRE_VOICE_COUNTS(2, 2);
    }

    SECTION("Piano group layered with a multi voice group")
    {
        auto tp = TwoGroupsEveryKey<32>();
        auto &vm = tp.voiceManager;
        vm.setRepeatedKeyMode(2112, vm_t::RepeatedKeyMode::PIANO);

        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(2, 2);
        vm.processNoteOffEvent(0, 0, 60, -1, 0.4);
        tp.processFor(1);
        REQUIRE_VOICE_COUNTS(2, 0);

        // 2112 retriggers in place, 90125 still gets its new voice
        REQUIRE(vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0) == true);
        REQUIRE_VOICE_COUNTS(3, 2);
        tp.processFor(10);
        REQUIRE_VOICE_COUNTS(2, 2);
    }

    SECTION("Two piano groups where only one is still sounding")
    {
        auto tp = TwoGroupsEveryKey<32>();
        auto &vm = tp.voiceManager;
        vm.setRepeatedKeyMode(2112, vm_t::RepeatedKeyMode::PIANO);
        vm.setRepeatedKeyMode(90125, vm_t::RepeatedKeyMode::PIANO);

        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
        vm.processNoteOffEvent(0, 0, 60, -1, 0.4);
        tp.processFor(10);
        REQUIRE_NO_VOICES;

        // a fresh pair, then end one of them so only one group has a voice on 60
        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(2, 2);
        vm.allSoundsOffMatching([](auto *v) { return v->creationCount % 2 == 1; });
        REQUIRE_VOICE_COUNTS(1, 1);
        vm.processNoteOffEvent(0, 0, 60, -1, 0.4);
        tp.processFor(1);
        REQUIRE_VOICE_COUNTS(1, 0);

        // the sounding group retriggers and the silent one launches fresh
        REQUIRE(vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0) == true);
        REQUIRE_VOICE_COUNTS(2, 2);
    }

    SECTION("A piano group with every voice retriggered launches nothing new")
    {
        auto tp = TwoGroupsEveryKey<32>();
        auto &vm = tp.voiceManager;
        vm.setRepeatedKeyMode(2112, vm_t::RepeatedKeyMode::PIANO);
        vm.setRepeatedKeyMode(90125, vm_t::RepeatedKeyMode::PIANO);

        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
        vm.processNoteOffEvent(0, 0, 60, -1, 0.4);
        tp.processFor(1);
        REQUIRE_VOICE_COUNTS(2, 0);

        REQUIRE(vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0) == true);
        REQUIRE_VOICE_COUNTS(2, 2);
    }
}
