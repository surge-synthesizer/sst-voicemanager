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

TEST_CASE("Stealing Priority - Oldest")
{
    SECTION("Single Voice Mode")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;

        vm.setPolyphonyGroupVoiceLimit(0, 4);
        for (int i = 0; i < 4; ++i)
        {
            vm.processNoteOnEvent(0, 0, 60 + i, -1, 0.8, 0.0);
            REQUIRE_VOICE_COUNTS(i + 1, i + 1);
        }
        REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 60; }) == 1);
        vm.processNoteOnEvent(0, 0, 68, -1, 0.8, 0.0);
        REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 60; }) == 0);
    }

    for (auto lim : {12, 13, 14})
    {
        DYNAMIC_SECTION("Triple Voice Mode limit " << lim)
        {
            TestPlayer<32> tp;
            auto &vm = tp.voiceManager;

            vm.setPolyphonyGroupVoiceLimit(0, lim);
            for (int i = 0; i < 4; ++i)
            {
                vm.processNoteOnEvent(0, 0, 80 + i, -1, 0.8, 0.0);
                REQUIRE_VOICE_COUNTS(3 * (i + 1), 3 * (i + 1));
            }
            REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 80; }) == 3);
            vm.processNoteOnEvent(0, 0, 90, -1, 0.8, 0.0);
            REQUIRE_VOICE_COUNTS(12, 12);
            REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 80; }) == 0);
        }
    }
}

TEST_CASE("Stealing Priority - Highest")
{
    SECTION("Single Voice Mode")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;

        vm.setPolyphonyGroupVoiceLimit(0, 4);
        vm.setStealingPriorityMode(0, TestPlayer<32>::voiceManager_t::HIGHEST);
        for (int i = 0; i < 4; ++i)
        {
            vm.processNoteOnEvent(0, 0, 60 + i, -1, 0.8, 0.0);
            REQUIRE_VOICE_COUNTS(i + 1, i + 1);
        }
        REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 63; }) == 1);
        vm.processNoteOnEvent(0, 0, 68, -1, 0.8, 0.0);
        REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 63; }) == 0);
    }

    for (auto lim : {12, 13, 14})
    {
        DYNAMIC_SECTION("Triple Voice Mode limit " << lim)
        {
            TestPlayer<32> tp;
            auto &vm = tp.voiceManager;

            vm.setPolyphonyGroupVoiceLimit(0, lim);
            vm.setStealingPriorityMode(0, TestPlayer<32>::voiceManager_t::HIGHEST);

            for (int i = 0; i < 4; ++i)
            {
                vm.processNoteOnEvent(0, 0, 80 + i, -1, 0.8, 0.0);
                REQUIRE_VOICE_COUNTS(3 * (i + 1), 3 * (i + 1));
            }
            REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 83; }) == 3);
            vm.processNoteOnEvent(0, 0, 90, -1, 0.8, 0.0);
            REQUIRE_VOICE_COUNTS(12, 12);
            REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 83; }) == 0);
        }
    }
}

TEST_CASE("Stealing Priority - Lowest")
{
    SECTION("Single Voice Mode")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;

        vm.setPolyphonyGroupVoiceLimit(0, 4);
        vm.setStealingPriorityMode(0, TestPlayer<32>::voiceManager_t::LOWEST);
        for (int i = 0; i < 4; ++i)
        {
            vm.processNoteOnEvent(0, 0, 60 - i, -1, 0.8, 0.0);
            REQUIRE_VOICE_COUNTS(i + 1, i + 1);
        }
        REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 57; }) == 1);
        vm.processNoteOnEvent(0, 0, 68, -1, 0.8, 0.0);
        REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 57; }) == 0);
    }

    for (auto lim : {12, 13, 14})
    {
        DYNAMIC_SECTION("Triple Voice Mode limit " << lim)
        {
            TestPlayer<32> tp;
            auto &vm = tp.voiceManager;

            vm.setPolyphonyGroupVoiceLimit(0, lim);
            vm.setStealingPriorityMode(0, TestPlayer<32>::voiceManager_t::LOWEST);

            for (int i = 0; i < 4; ++i)
            {
                vm.processNoteOnEvent(0, 0, 89 - i, -1, 0.8, 0.0);
                REQUIRE_VOICE_COUNTS(3 * (i + 1), 3 * (i + 1));
            }
            REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 86; }) == 3);
            vm.processNoteOnEvent(0, 0, 90, -1, 0.8, 0.0);
            REQUIRE_VOICE_COUNTS(12, 12);
            REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 86; }) == 0);
        }
    }
}