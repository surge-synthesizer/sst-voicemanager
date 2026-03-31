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

TEST_CASE("Routing Midi CC")
{
    auto tp = TestPlayer<32>();
    auto &vm = tp.voiceManager;

    REQUIRE_NO_VOICES;
    REQUIRE(vm.dialect == TestPlayer<32>::voiceManager_t::MIDI1Dialect::MIDI1);

    // Test one: Does routing midi cc on cc 0 and 6 stay independent
    vm.routeMIDI1CC(0, 0, 0, 17);
    REQUIRE(tp.midi1CC[0][0] == 17);
    REQUIRE(tp.midi1CC[0][6] == 0);
    REQUIRE(tp.midi1CC[4][0] == 0);
    REQUIRE(tp.midi1CC[4][6] == 0);

    vm.routeMIDI1CC(0, 0, 6, 23);
    REQUIRE(tp.midi1CC[0][0] == 17);
    REQUIRE(tp.midi1CC[0][6] == 23);
    REQUIRE(tp.midi1CC[4][0] == 0);
    REQUIRE(tp.midi1CC[4][6] == 0);

    vm.routeMIDI1CC(0, 1, 6, 88);
    REQUIRE(tp.midi1CC[0][0] == 17);
    REQUIRE(tp.midi1CC[0][6] == 23);
    REQUIRE(tp.midi1CC[4][0] == 0);
    REQUIRE(tp.midi1CC[4][6] == 0);

    vm.routeMIDI1CC(0, 4, 6, 74);
    REQUIRE(tp.midi1CC[0][0] == 17);
    REQUIRE(tp.midi1CC[0][6] == 23);
    REQUIRE(tp.midi1CC[4][0] == 0);
    REQUIRE(tp.midi1CC[4][6] == 74);

    // And in MIDI1 mode make sure that CC 74 works properly
    vm.routeMIDI1CC(0, 0, 74, 63);
    REQUIRE(tp.midi1CC[0][74] == 63);
}

TEST_CASE("Routing Mono Pitch Bend")
{
    auto tp = TestPlayer<32>();
    auto &vm = tp.voiceManager;

    REQUIRE_NO_VOICES;

    vm.processNoteOnEvent(0, 0, 55, -1, 0.5, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE(tp.pitchBend[0] == 0);
    REQUIRE(tp.pitchBend[4] == 0);
    tp.processFor(3);

    vm.routeMIDIPitchBend(0, 0, 9000);
    REQUIRE(tp.pitchBend[0] == 9000);
    REQUIRE(tp.pitchBend[4] == 0);

    vm.routeMIDIPitchBend(0, 2, 74);
    REQUIRE(tp.pitchBend[0] == 9000);
    REQUIRE(tp.pitchBend[4] == 0);

    vm.routeMIDIPitchBend(0, 4, 4000);
    REQUIRE(tp.pitchBend[0] == 9000);
    REQUIRE(tp.pitchBend[4] == 4000);
}

TEST_CASE("Routing Channel AT")
{
    auto tp = TestPlayer<32>();
    auto &vm = tp.voiceManager;

    REQUIRE_NO_VOICES;

    vm.processNoteOnEvent(0, 0, 55, -1, 0.5, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE(tp.channelPressure[0] == 0);
    tp.processFor(3);

    vm.routeChannelPressure(0, 0, 17);
    REQUIRE(tp.channelPressure[0] == 17);
    REQUIRE(tp.channelPressure[4] == 0);

    vm.routeChannelPressure(0, 2, 85);
    REQUIRE(tp.channelPressure[0] == 17);
    REQUIRE(tp.channelPressure[4] == 0);

    vm.routeChannelPressure(0, 4, 71);
    REQUIRE(tp.channelPressure[0] == 17);
    REQUIRE(tp.channelPressure[4] == 71);
}

TEST_CASE("Routing Poly AT")
{
    auto tp = TestPlayer<32>();
    auto &vm = tp.voiceManager;

    REQUIRE_NO_VOICES;

    vm.processNoteOnEvent(0, 0, 55, -1, 0.5, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    tp.processFor(3);

    vm.processNoteOnEvent(0, 0, 85, -1, 0.5, 0);
    REQUIRE_VOICE_COUNTS(4, 4);

    vm.routePolyphonicAftertouch(0, 0, 55, 17);
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v) { return v.polyATValue == 17; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 85; },
                                [](auto &v) { return v.polyATValue == 0; }));
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.polyATValue == 0; }) == 3);

    vm.routePolyphonicAftertouch(0, 0, 85, 23);
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v) { return v.polyATValue == 17; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 85; },
                                [](auto &v) { return v.polyATValue == 23; }));
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.polyATValue == 23; }) == 3);

    // Make sure off port channel key routing doesnt screw us up

    vm.routePolyphonicAftertouch(0, 2, 85, 74);
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v) { return v.polyATValue == 17; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 85; },
                                [](auto &v) { return v.polyATValue == 23; }));
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.polyATValue == 23; }) == 3);

    vm.routePolyphonicAftertouch(2, 0, 85, 74);
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v) { return v.polyATValue == 17; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 85; },
                                [](auto &v) { return v.polyATValue == 23; }));
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.polyATValue == 23; }) == 3);

    vm.routePolyphonicAftertouch(0, 0, 83, 74);
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v) { return v.polyATValue == 17; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 85; },
                                [](auto &v) { return v.polyATValue == 23; }));
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.polyATValue == 23; }) == 3);
}

TEST_CASE("Routing Note Expressions")
{
    auto tp = TestPlayer<32>();
    auto &vm = tp.voiceManager;

    REQUIRE_NO_VOICES;

    vm.processNoteOnEvent(0, 0, 55, 10455, 0.5, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    tp.processFor(3);

    vm.processNoteOnEvent(0, 0, 85, 10485, 0.5, 0);
    REQUIRE_VOICE_COUNTS(4, 4);

    // Test basic routing
    vm.routeNoteExpression(0, 0, 55, 10455, 3, 0.74);
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v) { return v.noteExpressionCache.at(3) == 0.74; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 85; },
                                [](auto &v) { return v.noteExpressionCache.empty(); }));
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.noteExpressionCache.empty(); }) == 3);

    vm.routeNoteExpression(0, 0, 85, 10485, 2, 0.77);
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v) { return v.noteExpressionCache.at(3) == 0.74; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 85; },
                                [](auto &v) { return v.noteExpressionCache.at(2) == 0.77; }));
    REQUIRE(tp.activeVoicesMatching(
                [](auto &v)
                { return v.noteExpressionCache.find(2) != v.noteExpressionCache.end(); }) == 3);

    vm.routeNoteExpression(0, 0, 55, 10455, 2, 0.11);
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v) { return v.noteExpressionCache.at(3) == 0.74; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v) { return v.noteExpressionCache.at(2) == 0.11; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 85; },
                                [](auto &v) { return v.noteExpressionCache.at(2) == 0.77; }));
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.noteExpressionCache.at(2) == 0.77; }) ==
            3);

    // Does routing to the wrong note id not update me?

    vm.routeNoteExpression(0, 0, 55, 70455, 2, 0.99);
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v) { return v.noteExpressionCache.at(3) == 0.74; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v) { return v.noteExpressionCache.at(2) == 0.11; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 85; },
                                [](auto &v) { return v.noteExpressionCache.at(2) == 0.77; }));
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.noteExpressionCache.at(2) == 0.77; }) ==
            3);

    // Add a second note 55 and try and modulate it
    vm.processNoteOnEvent(0, 0, 55, 20455, 0.5, 0);
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 55; }) == 2);
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.noteExpressionCache.empty(); }) == 1);

    vm.routeNoteExpression(0, 0, 55, 20455, 2, -0.33);
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 55; }) == 2);
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.noteExpressionCache.empty(); }) == 0);
    REQUIRE(tp.activeVoicesMatching(
                [](auto &v)
                {
                    return v.key() == 55 &&
                           v.noteExpressionCache.find(2) != v.noteExpressionCache.end();
                }) == 2);
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v)
                                {
                                    auto nexp = v.noteExpressionCache.at(2);
                                    return nexp == -0.33 || nexp == 0.11;
                                }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.noteid() == 10455; },
                                [](auto &v) { return v.noteExpressionCache.at(2) == 0.11; }));

    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.noteid() == 20455; },
                                [](auto &v) { return v.noteExpressionCache.at(2) == -0.33; }));
}

TEST_CASE("CC and Pitch Bend Replay on New Voice Creation")
{
    INFO("The voice manager caches the last CC and pitch-bend value per channel and replays "
         "them to the monoResponder whenever a new voice is created, so a voice that starts "
         "mid-performance receives the current controller state immediately.");

    SECTION("Pitch Bend Is Replayed To New Voice")
    {
        auto tp = TestPlayer<32>();
        auto &vm = tp.voiceManager;
        REQUIRE_NO_VOICES;

        // Send pitch bend with no voices playing; value is cached but also forwarded now.
        vm.routeMIDIPitchBend(0, 0, 9500);
        REQUIRE(tp.pitchBend[0] == 9500);

        // Simulate state loss so the replay is distinguishable from the original send.
        tp.pitchBend[0] = 0;
        REQUIRE(tp.pitchBend[0] == 0);

        // A new note-on must replay the cached bend to the monoResponder.
        vm.processNoteOnEvent(0, 0, 60, -1, 0.8f, 0.f);
        REQUIRE_VOICE_COUNTS(1, 1);
        REQUIRE(tp.pitchBend[0] == 9500);
    }

    SECTION("CC Is Replayed To New Voice")
    {
        auto tp = TestPlayer<32>();
        auto &vm = tp.voiceManager;
        REQUIRE_NO_VOICES;

        vm.routeMIDI1CC(0, 0, 1, 100); // modwheel = 100
        REQUIRE(tp.midi1CC[0][1] == 100);

        tp.midi1CC[0][1] = 0;
        REQUIRE(tp.midi1CC[0][1] == 0);

        vm.processNoteOnEvent(0, 0, 60, -1, 0.8f, 0.f);
        REQUIRE_VOICE_COUNTS(1, 1);
        REQUIRE(tp.midi1CC[0][1] == 100);
    }

    SECTION("Multiple CCs On Same Channel Are All Replayed")
    {
        auto tp = TestPlayer<32>();
        auto &vm = tp.voiceManager;
        REQUIRE_NO_VOICES;

        vm.routeMIDI1CC(0, 0, 1, 80);  // mod wheel
        vm.routeMIDI1CC(0, 0, 11, 64); // expression
        vm.routeMIDI1CC(0, 0, 7, 100); // volume

        tp.midi1CC[0][1] = 0;
        tp.midi1CC[0][11] = 0;
        tp.midi1CC[0][7] = 0;

        vm.processNoteOnEvent(0, 0, 60, -1, 0.8f, 0.f);
        REQUIRE_VOICE_COUNTS(1, 1);
        REQUIRE(tp.midi1CC[0][1] == 80);
        REQUIRE(tp.midi1CC[0][11] == 64);
        REQUIRE(tp.midi1CC[0][7] == 100);
    }

    SECTION("Replay Is Channel-Specific")
    {
        INFO("CC and PB sent on channel 0 must not be replayed to a voice created on channel 1");
        auto tp = TestPlayer<32>();
        auto &vm = tp.voiceManager;
        REQUIRE_NO_VOICES;

        vm.routeMIDI1CC(0, 0, 1, 99);      // channel 0 mod wheel
        vm.routeMIDIPitchBend(0, 0, 9500); // channel 0 bend
        tp.midi1CC[0][1] = 0;
        tp.pitchBend[0] = 0;

        // New voice on channel 1 — replay should fire on channel 0 only because
        // the voice is on channel 1 and the cache is keyed by the note's channel.
        vm.processNoteOnEvent(0, 1, 60, -1, 0.8f, 0.f);
        REQUIRE_VOICE_COUNTS(1, 1);

        // Channel 0 cache was not replayed (voice is on channel 1)
        REQUIRE(tp.midi1CC[0][1] == 0);
        REQUIRE(tp.pitchBend[0] == 0);

        // Channel 1 has no cached values, so nothing changes there either
        REQUIRE(tp.midi1CC[1][1] == 0);
        REQUIRE(tp.pitchBend[1] == 0);
    }

    SECTION("Center Pitch Bend Is Not Replayed")
    {
        INFO("lastPBByChannel stores pb14bit-8192; if the result is 0 (center), no replay fires");
        auto tp = TestPlayer<32>();
        auto &vm = tp.voiceManager;
        REQUIRE_NO_VOICES;

        vm.routeMIDIPitchBend(0, 0, 9500);
        vm.routeMIDIPitchBend(0, 0, 8192); // return to center
        tp.pitchBend[0] = 9999;            // sentinel; should remain if no replay fires

        vm.processNoteOnEvent(0, 0, 60, -1, 0.8f, 0.f);
        REQUIRE_VOICE_COUNTS(1, 1);
        REQUIRE(tp.pitchBend[0] == 9999); // center bend triggers no replay
    }
}

TEST_CASE("Routing Poly Parameter Modulations")
{
    auto tp = TestPlayer<32>();
    auto &vm = tp.voiceManager;

    REQUIRE_NO_VOICES;

    vm.processNoteOnEvent(0, 0, 55, 10455, 0.5, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    tp.processFor(3);

    vm.processNoteOnEvent(0, 0, 85, 10485, 0.5, 0);
    REQUIRE_VOICE_COUNTS(4, 4);

    // Test basic routing
    vm.routePolyphonicParameterModulation(0, 0, 55, 10455, 3, 0.74);
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v) { return v.paramModulationCache.at(3) == 0.74; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 85; },
                                [](auto &v) { return v.paramModulationCache.empty(); }));
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.paramModulationCache.empty(); }) == 3);

    vm.routePolyphonicParameterModulation(0, 0, 85, 10485, 2, 0.77);
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v) { return v.paramModulationCache.at(3) == 0.74; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 85; },
                                [](auto &v) { return v.paramModulationCache.at(2) == 0.77; }));
    REQUIRE(tp.activeVoicesMatching(
                [](auto &v)
                { return v.paramModulationCache.find(2) != v.paramModulationCache.end(); }) == 3);

    vm.routePolyphonicParameterModulation(0, 0, 55, 10455, 2, 0.11);
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v) { return v.paramModulationCache.at(3) == 0.74; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v) { return v.paramModulationCache.at(2) == 0.11; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 85; },
                                [](auto &v) { return v.paramModulationCache.at(2) == 0.77; }));
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.paramModulationCache.at(2) == 0.77; }) ==
            3);

    // Does routing to the wrong note id not update me?

    vm.routePolyphonicParameterModulation(0, 0, 55, 70455, 2, 0.99);
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v) { return v.paramModulationCache.at(3) == 0.74; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v) { return v.paramModulationCache.at(2) == 0.11; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 85; },
                                [](auto &v) { return v.paramModulationCache.at(2) == 0.77; }));
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.paramModulationCache.at(2) == 0.77; }) ==
            3);

    // Add a second note 55 and try and modulate it
    vm.processNoteOnEvent(0, 0, 55, 20455, 0.5, 0);
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 55; }) == 2);
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.paramModulationCache.empty(); }) == 1);

    vm.routePolyphonicParameterModulation(0, 0, 55, 20455, 2, -0.33);
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 55; }) == 2);
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.paramModulationCache.empty(); }) == 0);
    REQUIRE(tp.activeVoicesMatching(
                [](auto &v)
                {
                    return v.key() == 55 &&
                           v.paramModulationCache.find(2) != v.paramModulationCache.end();
                }) == 2);
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v)
                                {
                                    auto nexp = v.paramModulationCache.at(2);
                                    return nexp == -0.33 || nexp == 0.11;
                                }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.noteid() == 10455; },
                                [](auto &v) { return v.paramModulationCache.at(2) == 0.11; }));

    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.noteid() == 20455; },
                                [](auto &v) { return v.paramModulationCache.at(2) == -0.33; }));
}

TEST_CASE("Routing Mono Parameter Modulations")
{
    INFO("routeMonophonicParameterModulation delivers to all active voices regardless of "
         "port/channel/key, and stores the value in monoParamModulationCache.");

    auto tp = TestPlayer<32>();
    auto &vm = tp.voiceManager;

    REQUIRE_NO_VOICES;

    // Start a voice on channel 0, key 55
    vm.processNoteOnEvent(0, 0, 55, 10455, 0.5, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    tp.processFor(3);

    // Start a voice on channel 1, key 60
    vm.processNoteOnEvent(0, 1, 60, 10460, 0.5, 0);
    REQUIRE_VOICE_COUNTS(2, 2);

    // All voices start with empty monoParamModulationCache
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.monoParamModulationCache.empty(); }) ==
            2);

    // Route a mono param modulation — it should reach ALL active voices
    vm.routeMonophonicParameterModulation(0, 0, 55, 7, 0.42);
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.monoParamModulationCache.empty(); }) ==
            0);
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v) { return v.monoParamModulationCache.at(7) == 0.42; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 60; },
                                [](auto &v) { return v.monoParamModulationCache.at(7) == 0.42; }));

    // Route a second param ID — both voices should receive it
    vm.routeMonophonicParameterModulation(0, 0, 55, 3, 0.99);
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 55; },
                                [](auto &v) { return v.monoParamModulationCache.at(3) == 0.99; }));
    REQUIRE(tp.activeVoiceCheck([](auto &v) { return v.key() == 60; },
                                [](auto &v) { return v.monoParamModulationCache.at(3) == 0.99; }));

    // Update param 7 with a new value — both voices should reflect it
    vm.routeMonophonicParameterModulation(0, 0, 55, 7, -0.11);
    REQUIRE(tp.activeVoicesMatching([](auto &v)
                                    { return v.monoParamModulationCache.at(7) == -0.11; }) == 2);
    // Param 3 must be unchanged
    REQUIRE(tp.activeVoicesMatching([](auto &v)
                                    { return v.monoParamModulationCache.at(3) == 0.99; }) == 2);

    // Poly param modulation cache must remain untouched by mono routing
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.paramModulationCache.empty(); }) == 2);
}
