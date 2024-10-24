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
#include "sst/voicemanager/midi1_to_voicemanager.h"
#include "test_player.h"

/*
 * This test could really use expansion
 */

TEST_CASE("MIDI1 Note Basics")
{
    SECTION("Note On Off")
    {
        auto tp = TestPlayer<32>();
        auto &vm = tp.voiceManager;

        auto ap = [](auto &vm, uint8_t d0, uint8_t d1, uint8_t d2)
        {
            const uint8_t d[3]{d0, d1, d2};
            sst::voicemanager::applyMidi1Message<TestPlayer<32>::voiceManager_t>(vm, 0, d);
        };

        REQUIRE_NO_VOICES;
        ap(vm, 0x90, 60, 127);
        REQUIRE_VOICE_COUNTS(1, 1);
        REQUIRE(tp.activeVoicesMatching(
                    [](auto &v) { return v.key() == 60 && v.velocity == 1.0 && v.isGated; }) == 1);
        ap(vm, 0x80, 60, 127);
        REQUIRE_VOICE_COUNTS(1, 0);
    }

    SECTION("Vel 0 is Off")
    {
        auto tp = TestPlayer<32>();
        auto &vm = tp.voiceManager;

        auto ap = [](auto &vm, uint8_t d0, uint8_t d1, uint8_t d2)
        {
            const uint8_t d[3]{d0, d1, d2};
            sst::voicemanager::applyMidi1Message<TestPlayer<32>::voiceManager_t>(vm, 0, d);
        };

        REQUIRE_NO_VOICES;
        ap(vm, 0x90, 60, 127);
        REQUIRE_VOICE_COUNTS(1, 1);
        ap(vm, 0x90, 60, 0);
        REQUIRE_VOICE_COUNTS(1, 0);
    }
}
