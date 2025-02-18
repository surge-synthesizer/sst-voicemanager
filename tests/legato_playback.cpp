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

TEST_CASE("Legato Mono Mode - Single Key Releases")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_LEGATO);

    REQUIRE_NO_VOICES;

    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    vm.processNoteOffEvent(0, 0, 60, -1, 0.8);
    REQUIRE_VOICE_COUNTS(1, 0);
    tp.processFor(10);
    REQUIRE_VOICE_COUNTS(0, 0);
}

TEST_CASE("Legato Mono Mode - Simplest Move")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_LEGATO);

    REQUIRE_NO_VOICES;

    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);
    REQUIRE_VOICE_MATCH(1, v.originalKey() == 60);
    tp.processFor(2);
    vm.processNoteOnEvent(0, 0, 62, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 62);
    REQUIRE_VOICE_MATCH(1, v.originalKey() == 60);
}

TEST_CASE("Legato Mono Mode - Release while Gated")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_LEGATO);

    REQUIRE_NO_VOICES;

    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);
    REQUIRE_VOICE_MATCH(1, v.originalKey() == 60);
    REQUIRE_VOICE_MATCH(1, v.creationCount == 1);

    tp.processFor(2);
    vm.processNoteOnEvent(0, 0, 62, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 62);
    REQUIRE_VOICE_MATCH(1, v.originalKey() == 60);
    REQUIRE_VOICE_MATCH(1, v.runtime >= 2);
    REQUIRE_VOICE_MATCH(1, v.creationCount == 1);

    vm.processNoteOffEvent(0, 0, 60, -1, 0.8);
    tp.processFor(2);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 62);
    REQUIRE_VOICE_MATCH(1, v.originalKey() == 60);
    REQUIRE_VOICE_MATCH(1, v.runtime >= 4);
    REQUIRE_VOICE_MATCH(1, v.creationCount == 1);

    tp.processFor(20);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 62);
    REQUIRE_VOICE_MATCH(1, v.originalKey() == 60);
    REQUIRE_VOICE_MATCH(1, v.runtime >= 24);
    REQUIRE_VOICE_MATCH(1, v.creationCount == 1);

    vm.processNoteOffEvent(0, 0, 62, -1, 0.8);
    tp.processFor(2);
    REQUIRE_VOICE_COUNTS(1, 0);
    REQUIRE_VOICE_MATCH(1, v.key() == 62);
    REQUIRE_VOICE_MATCH(1, v.originalKey() == 60);
    REQUIRE_VOICE_MATCH(1, v.runtime >= 26);
    REQUIRE_VOICE_MATCH(1, v.creationCount == 1);

    tp.processFor(20);
    REQUIRE_NO_VOICES;
}

TEST_CASE("Legato Mono Mode - Multi-voice simple")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_LEGATO);

    REQUIRE_NO_VOICES;
    vm.processNoteOnEvent(0, 0, 90, -1, 0.9, 0);
    tp.processFor(1);
    REQUIRE_VOICE_COUNTS(3, 3);
    REQUIRE_VOICE_MATCH(3, v.key() == 90);
    REQUIRE_VOICE_MATCH(3, v.originalKey() == 90);

    vm.processNoteOnEvent(0, 0, 92, -1, 0.9, 0);
    tp.processFor(1);
    REQUIRE_VOICE_COUNTS(3, 3);
    REQUIRE_VOICE_MATCH(3, v.key() == 92);
    REQUIRE_VOICE_MATCH(3, v.originalKey() == 90);

    vm.processNoteOffEvent(0, 0, 90, -1, 0.9);
    tp.processFor(1);
    REQUIRE_VOICE_COUNTS(3, 3);
    REQUIRE_VOICE_MATCH(3, v.key() == 92);
    REQUIRE_VOICE_MATCH(3, v.originalKey() == 90);

    vm.processNoteOffEvent(0, 0, 92, -1, 0.9);
    tp.processFor(1);
    REQUIRE_VOICE_COUNTS(3, 0);
    REQUIRE_VOICE_MATCH(3, v.key() == 92);
    REQUIRE_VOICE_MATCH(3, v.originalKey() == 90);

    tp.processFor(20);
    REQUIRE_NO_VOICES;
}

TEST_CASE("Legato Mono Mode - Simple Release Moves Back")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_LEGATO);

    REQUIRE_NO_VOICES;

    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);
    REQUIRE_VOICE_MATCH(1, v.originalKey() == 60);
    REQUIRE_VOICE_MATCH(1, v.creationCount == 1);

    tp.processFor(2);
    vm.processNoteOnEvent(0, 0, 62, -1, 0.8, 0);
    tp.processFor(2);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 62);
    REQUIRE_VOICE_MATCH(1, v.originalKey() == 60);
    REQUIRE_VOICE_MATCH(1, v.runtime > 2);
    REQUIRE_VOICE_MATCH(1, v.creationCount == 1);

    vm.processNoteOffEvent(0, 0, 62, -1, 0.8);
    tp.processFor(2);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);
    REQUIRE_VOICE_MATCH(1, v.originalKey() == 60);
    INFO("Do not create a new voice when releasing retriggering");
    REQUIRE_VOICE_MATCH(1, v.creationCount == 1);
    REQUIRE_VOICE_MATCH(1, v.runtime > 4); // It should be the same voice

    vm.processNoteOffEvent(0, 0, 60, -1, 0.8);
    tp.processFor(2);
    REQUIRE_VOICE_COUNTS(1, 0);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);
    REQUIRE_VOICE_MATCH(1, v.originalKey() == 60);
    REQUIRE_VOICE_MATCH(1, v.runtime > 6);
    REQUIRE_VOICE_MATCH(1, v.creationCount == 1);

    tp.processFor(20);
    REQUIRE_NO_VOICES;
}

TEST_CASE("Legato Mono Mode - Low Release Pri")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    REQUIRE_NO_VOICES;
    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::MONO_LEGATO |
                       (uint64_t)vm_t::MonoPlayModeFeatures::ON_RELEASE_TO_LOWEST);

    INFO("Play notes in order, 58, 60, 62");

    vm.processNoteOnEvent(0, 0, 58, -1, 0.8, 0.0);
    tp.processFor(1);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 58);
    REQUIRE_VOICE_MATCH(1, v.creationCount == 1);
    REQUIRE_VOICE_MATCH(0, v.key() != 58);

    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
    tp.processFor(1);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);
    REQUIRE_VOICE_MATCH(1, v.originalKey() == 58);
    REQUIRE_VOICE_MATCH(1, v.creationCount == 1);
    REQUIRE_VOICE_MATCH(0, v.key() != 60);

    vm.processNoteOnEvent(0, 0, 62, -1, 0.8, 0.0);
    tp.processFor(1);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 62);
    REQUIRE_VOICE_MATCH(1, v.originalKey() == 58);
    REQUIRE_VOICE_MATCH(1, v.creationCount == 1);
    REQUIRE_VOICE_MATCH(0, v.key() != 62);

    INFO("With 62 sounding, releasing it returns to lowest, which is 58");
    vm.processNoteOffEvent(0, 0, 62, -1, 0.0);
    tp.processFor(1);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 58);
    REQUIRE_VOICE_MATCH(1, v.originalKey() == 58);
    REQUIRE_VOICE_MATCH(1, v.creationCount == 1);
    REQUIRE_VOICE_MATCH(0, v.key() != 58);

    INFO("And releasing 58 returns to 60");
    vm.processNoteOffEvent(0, 0, 58, -1, 0.0);
    tp.processFor(1);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);
    REQUIRE_VOICE_MATCH(1, v.originalKey() == 58);
    REQUIRE_VOICE_MATCH(1, v.creationCount == 1);
    REQUIRE_VOICE_MATCH(0, v.key() != 60);

    INFO("The release of which returns to empty");
    vm.processNoteOffEvent(0, 0, 60, -1, 0.0);
    REQUIRE_VOICE_COUNTS(1, 0);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);
    REQUIRE_VOICE_MATCH(1, v.originalKey() == 58);
    REQUIRE_VOICE_MATCH(1, v.creationCount == 1);
    tp.processFor(10);
    REQUIRE_NO_VOICES;
}

TEST_CASE("Legato Mono Mode - High Release Pri")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    REQUIRE_NO_VOICES;
    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::MONO_LEGATO |
                       (uint64_t)vm_t::MonoPlayModeFeatures::ON_RELEASE_TO_HIGHEST);

    INFO("Play notes in order, 62, 60, 58");

    vm.processNoteOnEvent(0, 0, 62, -1, 0.8, 0.0);
    tp.processFor(1);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 62);
    REQUIRE_VOICE_MATCH(1, v.creationCount == 1);
    REQUIRE_VOICE_MATCH(0, v.key() != 62);

    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
    tp.processFor(1);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);
    REQUIRE_VOICE_MATCH(1, v.originalKey() == 62);
    REQUIRE_VOICE_MATCH(1, v.creationCount == 1);
    REQUIRE_VOICE_MATCH(0, v.key() != 60);

    vm.processNoteOnEvent(0, 0, 58, -1, 0.8, 0.0);
    tp.processFor(1);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 58);
    REQUIRE_VOICE_MATCH(1, v.originalKey() == 62);
    REQUIRE_VOICE_MATCH(1, v.creationCount == 1);
    REQUIRE_VOICE_MATCH(0, v.key() != 58);

    INFO("With 58 sounding, releasing it returns to highest, which is 62");
    vm.processNoteOffEvent(0, 0, 58, -1, 0.0);
    tp.processFor(1);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 62);
    REQUIRE_VOICE_MATCH(1, v.originalKey() == 62);
    REQUIRE_VOICE_MATCH(1, v.creationCount == 1);
    REQUIRE_VOICE_MATCH(0, v.key() != 62);

    INFO("And releasing 62 returns to 60");
    vm.processNoteOffEvent(0, 0, 62, -1, 0.0);
    tp.processFor(1);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);
    REQUIRE_VOICE_MATCH(1, v.originalKey() == 62);
    REQUIRE_VOICE_MATCH(1, v.creationCount == 1);
    REQUIRE_VOICE_MATCH(0, v.key() != 60);

    INFO("The release of which returns to empty");
    vm.processNoteOffEvent(0, 0, 60, -1, 0.0);
    REQUIRE_VOICE_COUNTS(1, 0);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);
    REQUIRE_VOICE_MATCH(1, v.originalKey() == 62);
    REQUIRE_VOICE_MATCH(1, v.creationCount == 1);
    tp.processFor(10);
    REQUIRE_NO_VOICES;
}

TEST_CASE("Legato Mono Mode - Retrigger during Release")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_LEGATO);

    REQUIRE_NO_VOICES;

    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);
    REQUIRE_VOICE_MATCH(1, v.originalKey() == 60);
    REQUIRE_VOICE_MATCH(1, v.creationCount == 1);

    tp.processFor(2);
    vm.processNoteOnEvent(0, 0, 62, -1, 0.8, 0);
    tp.processFor(2);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 62);
    REQUIRE_VOICE_MATCH(1, v.originalKey() == 60);
    REQUIRE_VOICE_MATCH(1, v.runtime > 2);
    REQUIRE_VOICE_MATCH(1, v.creationCount == 1);

    vm.processNoteOffEvent(0, 0, 62, -1, 0.8);
    tp.processFor(2);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);
    REQUIRE_VOICE_MATCH(1, v.originalKey() == 60);
    INFO("Do not create a new voice when releasing retriggering");
    REQUIRE_VOICE_MATCH(1, v.creationCount == 1);
    REQUIRE_VOICE_MATCH(1, v.runtime > 4); // It should be the same voice

    vm.processNoteOffEvent(0, 0, 60, -1, 0.8);
    tp.processFor(2);
    REQUIRE_VOICE_COUNTS(1, 0);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);
    REQUIRE_VOICE_MATCH(1, v.originalKey() == 60);
    REQUIRE_VOICE_MATCH(1, v.runtime > 6);
    REQUIRE_VOICE_MATCH(1, v.creationCount == 1);

    vm.processNoteOnEvent(0, 0, 64, -1, 0.9, 0);
    tp.processFor(2);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 64);
    REQUIRE_VOICE_MATCH(1, v.originalKey() == 60);
    REQUIRE_VOICE_MATCH(1, v.runtime > 8);
    REQUIRE_VOICE_MATCH(1, v.creationCount == 1);

    vm.processNoteOffEvent(0, 0, 64, -1, 0.9);
    tp.processFor(2);
    REQUIRE_VOICE_COUNTS(1, 0);
    REQUIRE_VOICE_MATCH(1, v.key() == 64);
    REQUIRE_VOICE_MATCH(1, v.originalKey() == 60);
    REQUIRE_VOICE_MATCH(1, v.runtime > 10);
    REQUIRE_VOICE_MATCH(1, v.creationCount == 1);

    tp.processFor(20);
    REQUIRE_NO_VOICES;
}
TEST_CASE("Legato Mono Mode - Mixed Group Poly/Mono/Legato")
{
    auto tp = ThreeGroupsEveryKey<32, false>();
    typedef ThreeGroupsEveryKey<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    vm.setPlaymode(2112, vm_t::PlayMode::POLY_VOICES);
    vm.setPlaymode(90125, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_MONO);
    vm.setPlaymode(8675309, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_LEGATO);

    REQUIRE_NO_VOICES;

    vm.processNoteOnEvent(0, 0, 60, -1, 0.9, 0.0);
    tp.processFor(1);
    REQUIRE_VOICE_COUNTS(3, 3);
    REQUIRE_VOICE_MATCH(3, v.key() == 60);
    REQUIRE_VOICE_MATCH(3, v.creationCount <= 3);

    vm.processNoteOnEvent(0, 0, 62, -1, 0.9, 0.0);
    tp.processFor(1);
    REQUIRE_VOICE_COUNTS(4, 4);
    REQUIRE_VOICE_MATCH(3, v.key() == 62);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);
    REQUIRE_VOICE_MATCH(2, v.creationCount <= 3);

    vm.processNoteOffEvent(0, 0, 60, -1, 0.9);
    tp.processFor(1);
    REQUIRE_VOICE_COUNTS(4, 3);
    REQUIRE_VOICE_MATCH(3, v.key() == 62);
    REQUIRE_VOICE_MATCH(1, v.key() == 60 && !v.isGated);
    REQUIRE_VOICE_MATCH(2, v.creationCount <= 3);

    tp.processFor(10);
    REQUIRE_VOICE_COUNTS(3, 3);
    REQUIRE_VOICE_MATCH(3, v.key() == 62);
    REQUIRE_VOICE_MATCH(0, v.key() == 60);
    REQUIRE_VOICE_MATCH(1, v.creationCount <= 3);
}

TEST_CASE("Legato Mono Mode - Mixed with Mono Mode across Release")
{
    for (auto cs = 0; cs < 4; ++cs)
    {
        DYNAMIC_SECTION("Testing case " << cs)
        {
            auto tp = TwoGroupsEveryKey<32, false>();
            using vm_t = TwoGroupsEveryKey<32, false>::voiceManager_t;
            auto &vm = tp.voiceManager;
            ;

            auto modea = (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_LEGATO;
            auto modeb = (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_MONO;
            if (cs == 1)
                modeb = modea;
            if (cs == 2)
                modea = modeb;
            if (cs == 3)
                std::swap(modea, modeb);

            INFO("Modes are " << modea << " " << modeb);
            vm.setPlaymode(2112, vm_t::PlayMode::MONO_NOTES, modea);
            vm.setPlaymode(90125, vm_t::PlayMode::MONO_NOTES, modeb);

            REQUIRE_NO_VOICES;
            vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
            tp.processFor(1);
            REQUIRE_VOICE_COUNTS(2, 2);
            vm.processNoteOffEvent(0, 0, 60, -1, 0.8);
            tp.processFor(1);
            REQUIRE_VOICE_COUNTS(2, 0);

            vm.processNoteOnEvent(0, 0, 62, -1, 0.8, 0);
            tp.processFor(1);
            REQUIRE_VOICE_COUNTS(2, 2);
            vm.processNoteOffEvent(0, 0, 62, -1, 0.8);
            tp.processFor(1);
            REQUIRE_VOICE_COUNTS(2, 0);

            vm.processNoteOnEvent(0, 0, 64, -1, 0.8, 0);
            tp.processFor(1);
            REQUIRE_VOICE_COUNTS(2, 2);
            vm.processNoteOffEvent(0, 0, 64, -1, 0.8);
            tp.processFor(1);
            REQUIRE_VOICE_COUNTS(2, 0);
        }
    }
}

TEST_CASE("Legato Mode Sustain Pedal")
{
    SECTION("Single notes, no retrig, sustain")
    {
        TestPlayer<32> tp;
        using vm_t = TestPlayer<32>::voiceManager_t;
        auto &vm = tp.voiceManager;

        vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                       (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_LEGATO);

        REQUIRE_NO_VOICES;

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

    SECTION("Multiple notes, sustain")
    {
        TestPlayer<32> tp;
        using vm_t = TestPlayer<32>::voiceManager_t;

        auto &vm = tp.voiceManager;
        REQUIRE_NO_VOICES;

        vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                       (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_LEGATO);

        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(1, 1);
        tp.processFor(10);
        vm.processNoteOnEvent(0, 0, 64, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(1, 1);
        tp.processFor(10);
        vm.updateSustainPedal(0, 0, 120);
        tp.processFor(10);
        REQUIRE_VOICE_COUNTS(1, 1);
        vm.processNoteOffEvent(0, 0, 60, -1, 0.4);
        REQUIRE_VOICE_COUNTS(1, 1);
        tp.processFor(40);
        REQUIRE_VOICE_COUNTS(1, 1);

        vm.updateSustainPedal(0, 0, 0);
        REQUIRE_VOICE_COUNTS(1, 1);
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
        using vm_t = TestPlayer<32>::voiceManager_t;

        vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                       (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_LEGATO);

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

/*
TEST_CASE("Legato Mono Mode - Multi-voice complex") { REQUIRE_INCOMPLETE_TEST; }
TEST_CASE("Legato Mono Mode - Mixed Group Poly/Legato") { REQUIRE_INCOMPLETE_TEST; }
TEST_CASE("Legato Mono Mode - Mixed Group Mono/Legato") { REQUIRE_INCOMPLETE_TEST; }
*/
