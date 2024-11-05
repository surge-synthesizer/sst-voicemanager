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
#include <limits>

TEST_CASE("Stealing Groups - global group")
{
    SECTION("one voice 4 on 32")
    {
        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;
        vm.setPolyphonyGroupVoiceLimit(0, 4);

        REQUIRE_NO_VOICES;

        for (auto i = 0; i < 10; ++i)
        {
            vm.processNoteOnEvent(0, 0, 50 + i, -1, 0.8, 0.0);
            REQUIRE_VOICE_COUNTS(std::min(i + 1, 4), std::min(i + 1, 4));
        }
    }

    for (auto lim : {12, 13, 14})
    {
        DYNAMIC_SECTION("three voice " << lim << " on 32")
        {
            TestPlayer<32> tp;
            auto &vm = tp.voiceManager;
            vm.setPolyphonyGroupVoiceLimit(0, lim);

            REQUIRE_NO_VOICES;

            for (auto i = 0; i < 10; ++i)
            {
                vm.processNoteOnEvent(0, 0, 90 + i, -1, 0.8, 0.0);
                REQUIRE_VOICE_COUNTS(std::min(3 * (i + 1), 12), std::min(3 * (i + 1), 12));
            }
        }
    }
}

TEST_CASE("Stealing Groups - two groups single voice")
{
    INFO("In a 32 voice manager, make two groups with size 4 and 6");
    TestPlayer<32, false> tp;
    auto &vm = tp.voiceManager;
    vm.setPolyphonyGroupVoiceLimit(77, 4);
    vm.setPolyphonyGroupVoiceLimit(1752, 6);

    INFO("Even keys go to the 4 group, odd to the 6");
    tp.polyGroupForKey = [](auto k) { return (k % 2 == 0 ? 77 : 1752); };

    REQUIRE_NO_VOICES;

    INFO("Filling up evens stops at 4");
    for (int i = 0; i < 10; ++i)
    {
        // Even keys are group 77 and have limit 4
        vm.processNoteOnEvent(0, 0, 50 + i * 2, -1, 0.8, 0.0);
        tp.dumpAllVoices();
        REQUIRE_VOICE_COUNTS(std::min(i + 1, 4), std::min(i + 1, 4));
    }

    vm.allSoundsOff();
    REQUIRE_NO_VOICES;

    INFO("Filling up odds stops at 6");
    for (int i = 0; i < 10; ++i)
    {
        // Even keys are group 1752 and have limit 6
        vm.processNoteOnEvent(0, 0, 51 + i * 2, -1, 0.8, 0.0);
        tp.dumpAllVoices();
        REQUIRE_VOICE_COUNTS(std::min(i + 1, 6), std::min(i + 1, 6));
    }

    vm.allSoundsOff();
    REQUIRE_NO_VOICES;

    INFO("Fill up even voices");
    for (int i = 0; i < 10; ++i)
    {
        // Even keys are group 77 and have limit 4
        vm.processNoteOnEvent(0, 0, 50 + i * 2, -1, 0.8, 0.0);
    }
    REQUIRE_VOICE_COUNTS(4, 4);

    INFO("Add one more voice in the even side and you don't get another voice");
    vm.processNoteOnEvent(0, 0, 48, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(4, 4);
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() % 2 == 0; }) == 4);

    INFO("But add a voice in the odd side and you do");
    vm.processNoteOnEvent(0, 0, 49, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(5, 5);
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() % 2 == 0; }) == 4);
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() % 2 == 1; }) == 1);

    vm.allSoundsOff();
    REQUIRE_NO_VOICES;

    INFO("Do it the other way");
    for (int i = 0; i < 10; ++i)
    {
        // Even keys are group 77 and have limit 4
        vm.processNoteOnEvent(0, 0, 51 + i * 2, -1, 0.8, 0.0);
    }
    REQUIRE_VOICE_COUNTS(6, 6);

    INFO("Add one more voice in the even side and you don't get another voice");
    vm.processNoteOnEvent(0, 0, 49, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(6, 6);

    INFO("But add a voice in the odd side and you do");
    vm.processNoteOnEvent(0, 0, 50, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(7, 7);
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() % 2 == 0; }) == 1);
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() % 2 == 1; }) == 6);

    vm.allSoundsOff();
    REQUIRE_NO_VOICES;
}

TEST_CASE("Stealing Groups are Independentt")
{
    SECTION("Single Voice")
    {
        // Make sure we preserve the uint64_t
        auto hgid = std::numeric_limits<uint64_t>::max() - 72431;

        TestPlayer<32> tp;
        auto &vm = tp.voiceManager;
        tp.polyGroupForKey = [hgid](auto k) { return (k % 2 == 0 ? hgid : 887); };
        vm.setPolyphonyGroupVoiceLimit(hgid, 8);
        vm.setPolyphonyGroupVoiceLimit(887, 4);

        auto oddKey = [](auto &v) { return v.key() % 2 == 1; };
        auto evenKey = [](auto &v) { return v.key() % 2 == 0; };

        for (int i = 0; i < 15; ++i)
        {
            vm.processNoteOnEvent(0, 0, 20 + i, -1, 0.7, 0.0);
            vm.processNoteOnEvent(0, 0, 21 + i, -1, 0.7, 0.0);

            REQUIRE(tp.activeVoicesMatching(oddKey) == std::min(i + 1, 4));
            REQUIRE(tp.activeVoicesMatching(evenKey) == std::min(i + 1, 8));
        }
    }

    for (int off : {0, 1, 2})
    {
        DYNAMIC_SECTION("Double Voice " << off)
        {
            auto hgid = std::numeric_limits<uint64_t>::max() - 172431;

            TestPlayer<64> tp;
            auto &vm = tp.voiceManager;
            tp.polyGroupForKey = [hgid](auto k) { return (k % 2 == 0 ? hgid : 887); };
            vm.setPolyphonyGroupVoiceLimit(hgid, 12 + off);
            vm.setPolyphonyGroupVoiceLimit(887, 9 + off);

            auto oddKey = [](auto &v) { return v.key() % 2 == 1; };
            auto evenKey = [](auto &v) { return v.key() % 2 == 0; };

            for (int i = 0; i < 15; ++i)
            {
                vm.processNoteOnEvent(0, 0, 80 + i, -1, 0.7, 0.0);
                vm.processNoteOnEvent(0, 0, 81 + i, -1, 0.7, 0.0);

                REQUIRE(tp.activeVoicesMatching(oddKey) == std::min(3 * (i + 1), 9));
                REQUIRE(tp.activeVoicesMatching(evenKey) == std::min(3 * (i + 1), 12));
            }
        }
    }
}

TEST_CASE("Stealing Groups - one group plus global group single voice")
{
    SECTION("All the voices in global group")
    {
        TestPlayer<8> tp;
        auto &vm = tp.voiceManager;
        vm.setPolyphonyGroupVoiceLimit(77, 5);

        INFO("Even keys go to the 5 group, odd to the global");
        tp.polyGroupForKey = [](auto k) { return (k % 2 == 0 ? 77 : 0); };

        INFO("Should be able to fill the odd keysto 8");
        for (int i = 0; i < 10; ++i)
        {
            vm.processNoteOnEvent(0, 0, 51 + i * 2, -1, 0.8, 0.0);
            REQUIRE_VOICE_COUNTS(std::min(i + 1, 8), std::min(i + 1, 8));
        }
        REQUIRE_VOICE_COUNTS(8, 8);

        vm.allSoundsOff();
        REQUIRE_NO_VOICES;

        INFO("And should be able to fill the even keys to 5");
        for (int i = 0; i < 10; ++i)
        {
            vm.processNoteOnEvent(0, 0, 50 + i * 2, -1, 0.8, 0.0);
            REQUIRE_VOICE_COUNTS(std::min(i + 1, 5), std::min(i + 1, 5));
        }

        REQUIRE_VOICE_COUNTS(5, 5);

        vm.allSoundsOff();
        REQUIRE_NO_VOICES;
    }

    SECTION("Sub-group global group interaction one")
    {
        TestPlayer<8> tp;
        auto &vm = tp.voiceManager;
        vm.setPolyphonyGroupVoiceLimit(77, 5);

        INFO("Even keys go to the 5 group, odd to the global");
        tp.polyGroupForKey = [](auto k) { return (k % 2 == 0 ? 77 : 0); };

        INFO("So fill up the even (5) group");

        auto oddKey = [](auto &v) { return v.key() % 2 == 1; };
        auto evenKey = [](auto &v) { return v.key() % 2 == 0; };

        for (int i = 0; i < 10; ++i)
        {
            vm.processNoteOnEvent(0, 0, 50 + i * 2, -1, 0.8, 0.0);
            REQUIRE_VOICE_COUNTS(std::min(i + 1, 5), std::min(i + 1, 5));
        }
        REQUIRE(tp.activeVoicesMatching(evenKey) == 5);
        REQUIRE(tp.activeVoicesMatching(oddKey) == 0);

        INFO("So now there's 3 voices left in the global group");
        for (int i = 0; i < 3; ++i)
        {
            vm.processNoteOnEvent(0, 0, 51 + i * 2, -1, 0.8, 0.0);
            REQUIRE_VOICE_COUNTS(std::min(5 + i + 1, 8), std::min(5 + i + 1, 8));
        }

        INFO("Check we have 3 odd and 5 even voices");
        REQUIRE(tp.activeVoicesMatching(evenKey) == 5);
        REQUIRE(tp.activeVoicesMatching(oddKey) == 3);

        INFO("Add another even key and it should  steal from the even group");
        vm.processNoteOnEvent(0, 0, 22, -1, 0.8, 0.0);
        REQUIRE(tp.activeVoicesMatching(evenKey) == 5);
        REQUIRE(tp.activeVoicesMatching(oddKey) == 3);

        INFO("Add another odd key and it should steal from even");
        vm.processNoteOnEvent(0, 0, 23, -1, 0.8, 0.0);
        REQUIRE(tp.activeVoicesMatching(evenKey) == 4);
        REQUIRE(tp.activeVoicesMatching(oddKey) == 4);
    }
}

TEST_CASE("Stealing - up to physical limit with groups")
{
    SECTION("Sub-group global group interaction single voice")
    {
        TestPlayer<8> tp;
        auto &vm = tp.voiceManager;
        vm.setPolyphonyGroupVoiceLimit(77, 5);

        INFO("If we fill the global group, stealing on subgroup should work still");
        tp.polyGroupForKey = [](auto k) { return (k % 2 == 0 ? 77 : 0); };

        INFO("So fill up the global (8) group");

        auto oddKey = [](auto &v) { return v.key() % 2 == 1; };
        auto evenKey = [](auto &v) { return v.key() % 2 == 0; };

        for (int i = 0; i < 10; ++i)
        {
            vm.processNoteOnEvent(0, 0, 51 + i * 2, -1, 0.8, 0.0);
            REQUIRE_VOICE_COUNTS(std::min(i + 1, 8), std::min(i + 1, 8));
        }
        REQUIRE(tp.activeVoicesMatching(evenKey) == 0);
        REQUIRE(tp.activeVoicesMatching(oddKey) == 8);
        tp.dumpAllVoices();

        INFO("So now odd even groups should cap out at 5 but this requires "
             "stealing from somewhere other than myself");
        // HINT we probalby need a 'total voices' counter
        // HINT we need a 'find-to-steal-with-predicate' in details
        // FACTOR we need a 'do steal' in details to clean up
        for (int i = 0; i < 10; ++i)
        {
            vm.processNoteOnEvent(0, 0, 50 + i * 2, -1, 0.8, 0.0);
            tp.dumpAllVoices();
            REQUIRE(tp.activeVoicesMatching(evenKey) == std::min(i + 1, 5));
            REQUIRE(tp.activeVoicesMatching(oddKey) == std::max(8 - i - 1, 3));
        }
    }
}