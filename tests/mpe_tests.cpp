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

TEST_CASE("MPE Basic")
{
    auto tp = TestPlayer<32, false>();
    auto &vm = tp.voiceManager;
    vm.dialect = TestPlayer<32, false>::voiceManager_t::MIDI1Dialect::MIDI1_MPE;

    REQUIRE_NO_VOICES;

    vm.processNoteOnEvent(0, 1, 60, -1, 0.8, 0.0);
    vm.processNoteOnEvent(0, 2, 62, -1, 0.8, 0.0);

    REQUIRE_VOICE_COUNTS(2, 2);

    REQUIRE(tp.activeVoicesMatching(
                [](auto &v)
                {
                    return (v.key() == 60 && v.channel() == 1) ||
                           (v.key() == 62 && v.channel() == 2);
                }) == 2);
    vm.routeMIDIPitchBend(0, 1, 9000);
    tp.dumpAllVoices();

    REQUIRE(tp.activeVoicesMatching(
                [](auto &v)
                { return (v.key() == 60 && v.channel() == 1 && v.mpeBend == 9000); }) == 1);
    REQUIRE(tp.activeVoicesMatching(
                [](auto &v)
                { return (v.key() == 62 && v.channel() == 2 && v.mpeBend == 0); }) == 1);

    vm.routeChannelPressure(0, 2, 77);
    tp.dumpAllVoices();
    REQUIRE(tp.activeVoicesMatching(
                [](auto &v)
                { return (v.key() == 62 && v.channel() == 2 && v.mpePressure == 77); }) == 1);

    vm.routeMIDI1CC(0, 1, 74, 13);
    REQUIRE(tp.activeVoicesMatching(
                [](auto &v)
                { return (v.key() == 60 && v.channel() == 1 && v.mpeTimbre == 13); }) == 1);
}

TEST_CASE("MPE After Release")
{
    INFO("MPE has one gated voice per channel. Test that by doing a release and send");
    auto tp = TestPlayer<32, false>();

    auto &vm = tp.voiceManager;
    vm.dialect = TestPlayer<32, false>::voiceManager_t::MIDI1Dialect::MIDI1_MPE;

    REQUIRE_NO_VOICES;

    vm.processNoteOnEvent(0, 1, 60, -1, 0.8, 0.0);

    REQUIRE_VOICE_COUNTS(1, 1);

    REQUIRE(tp.activeVoicesMatching([](auto &v) { return (v.key() == 60 && v.channel() == 1); }) ==
            1);
    vm.routeMIDIPitchBend(0, 1, 9000);
    vm.routeChannelPressure(0, 1, 88);
    vm.routeMIDI1CC(0, 1, 74, 17);
    tp.dumpAllVoices();
    REQUIRE(tp.activeVoicesMatching(
                [](auto &v)
                {
                    return (v.key() == 60 && v.channel() == 1 && v.mpeBend == 9000 &&
                            v.mpePressure == 88 && v.mpeTimbre == 17 && v.isGated);
                }) == 1);
    tp.processFor(3);
    vm.processNoteOffEvent(0, 1, 60, -1, 0.8);
    REQUIRE_VOICE_COUNTS(1, 0);
    tp.processFor(1);
    REQUIRE(tp.activeVoicesMatching(
                [](auto &v)
                {
                    return (v.key() == 60 && v.channel() == 1 && v.mpeBend == 9000 &&
                            v.mpePressure == 88 && v.mpeTimbre == 17 && !v.isGated);
                }) == 1);
    tp.processFor(1);

    vm.processNoteOnEvent(0, 1, 60, -1, 0.8, 0.0);

    REQUIRE_VOICE_COUNTS(2, 1);

    REQUIRE(tp.activeVoicesMatching([](auto &v) { return (v.key() == 60 && v.channel() == 1); }) ==
            2);
    vm.routeMIDIPitchBend(0, 1, 7000);
    vm.routeChannelPressure(0, 1, 14);
    vm.routeMIDI1CC(0, 1, 74, 55);
    tp.dumpAllVoices();
    REQUIRE(tp.activeVoicesMatching(
                [](auto &v)
                {
                    return (v.key() == 60 && v.channel() == 1 && v.mpeBend == 9000 &&
                            v.mpePressure == 88 && v.mpeTimbre == 17 && !v.isGated);
                }) == 1);
    REQUIRE(tp.activeVoicesMatching(
                [](auto &v)
                {
                    return (v.key() == 60 && v.channel() == 1 && v.mpeBend == 7000 &&
                            v.mpePressure == 14 && v.mpeTimbre == 55 && v.isGated);
                }) == 1);
}

TEST_CASE("MPE Timbre CC Is Constant 74")
{
    using vm_t = TestPlayer<32, false>::voiceManager_t;
    static_assert(vm_t::mpeTimbreCC == 74, "MPE timbre CC must be 74 per spec");

    auto tp = TestPlayer<32, false>();
    auto &vm = tp.voiceManager;
    vm.dialect = vm_t::MIDI1Dialect::MIDI1_MPE;

    vm.processNoteOnEvent(0, 1, 60, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(1, 1);

    vm.routeMIDI1CC(0, 1, 74, 42);
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 60 && v.mpeTimbre == 42; }) ==
            1);

    // A non-timbre CC on an MPE channel must not affect mpeTimbre
    vm.routeMIDI1CC(0, 1, 73, 99);
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 60 && v.mpeTimbre == 42; }) ==
            1);
}

TEST_CASE("MPE Global Channel Pitch Bend")
{
    INFO("Pitch bend on the MPE global channel (0) must route to the mono responder, "
         "not set mpeBend on voices. Pitch bend on a member channel must set mpeBend "
         "on the voice playing on that channel, not update the mono responder.");

    using vm_t = TestPlayer<32, false>::voiceManager_t;
    auto tp = TestPlayer<32, false>();
    auto &vm = tp.voiceManager;
    vm.dialect = vm_t::MIDI1Dialect::MIDI1_MPE;

    // Confirm the default global channel is 0
    REQUIRE(vm.mpeGlobalChannel == 0);

    // Play a note on MPE member channel 1
    vm.processNoteOnEvent(0, 1, 60, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(1, 1);

    // Verify initial state: voice mpeBend is 0, mono pitchBend on ch 0 is 0
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 60 && v.mpeBend == 0; }) == 1);
    REQUIRE(tp.pitchBend[0] == 0);

    // Send pitch bend on global channel 0
    // Expected: mono responder pitchBend[0] updated, voice mpeBend stays 0
    vm.routeMIDIPitchBend(0, 0, 4000);

    REQUIRE(tp.pitchBend[0] == 4000);
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 60 && v.mpeBend == 0; }) == 1);

    // Send pitch bend on member channel 1
    // Expected: voice mpeBend updated, mono responder pitchBend[0] stays 4000
    vm.routeMIDIPitchBend(0, 1, 9000);

    REQUIRE(tp.activeVoicesMatching(
                [](auto &v)
                { return v.key() == 60 && v.channel() == 1 && v.mpeBend == 9000; }) == 1);
    REQUIRE(tp.pitchBend[0] == 4000);
}

TEST_CASE("MPE Sustain On Global Channel")
{
    INFO("In MPE, sustain arrives on the global channel (0 by default) and holds notes "
         "playing on every member channel. Release on the global channel unlatches them.");

    using vm_t = TestPlayer<32, false>::voiceManager_t;
    auto tp = TestPlayer<32, false>();
    auto &vm = tp.voiceManager;
    vm.dialect = vm_t::MIDI1Dialect::MIDI1_MPE;

    REQUIRE(vm.mpeGlobalChannel == 0);
    REQUIRE_NO_VOICES;

    // One note per member channel 1..15 (channel 0 is the global channel)
    for (int16_t ch = 1; ch < 16; ++ch)
        vm.processNoteOnEvent(0, ch, 60, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(15, 15);

    // Sustain down on the global channel
    vm.updateSustainPedal(0, 0, 127);

    // Release every note — all stay gated, held by sustain
    for (int16_t ch = 1; ch < 16; ++ch)
        vm.processNoteOffEvent(0, ch, 60, -1, 0.8);
    REQUIRE_VOICE_COUNTS(15, 15);

    // Sustain up on the global channel releases all of them at once
    vm.updateSustainPedal(0, 0, 0);
    REQUIRE_VOICE_COUNTS(15, 0);
    tp.processFor(6);
    REQUIRE_NO_VOICES;
}

TEST_CASE("MPE Global Channel Eight")
{
    INFO("With mpeGlobalChannel set to 8, member-channel notes, global pitch bend on "
         "channel 8, and sustain on channel 8 must all behave as for the default global "
         "channel 0.");

    using vm_t = TestPlayer<32, false>::voiceManager_t;
    auto tp = TestPlayer<32, false>();
    auto &vm = tp.voiceManager;
    vm.dialect = vm_t::MIDI1Dialect::MIDI1_MPE;
    vm.mpeGlobalChannel = 8;

    REQUIRE_NO_VOICES;

    // Notes on member channels surrounding the global channel 8
    const int16_t members[] = {1, 7, 9, 15};
    for (auto ch : members)
        vm.processNoteOnEvent(0, ch, 60, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(4, 4);
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 60 && v.mpeBend == 0; }) == 4);

    // Pitch bend on the global channel 8 routes to the mono responder (pitchBend[0]),
    // and must not set mpeBend on any voice
    vm.routeMIDIPitchBend(0, 8, 4000);
    REQUIRE(tp.pitchBend[0] == 4000);
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 60 && v.mpeBend == 0; }) == 4);

    // Pitch bend on a member channel sets mpeBend on that channel's voice only
    vm.routeMIDIPitchBend(0, 9, 9000);
    REQUIRE(tp.activeVoicesMatching(
                [](auto &v)
                { return v.key() == 60 && v.channel() == 9 && v.mpeBend == 9000; }) == 1);
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return v.key() == 60 && v.mpeBend == 0; }) == 3);
    REQUIRE(tp.pitchBend[0] == 4000);

    // Sustain arrives on the global channel 8 and holds all member-channel notes
    vm.updateSustainPedal(0, 8, 127);
    for (auto ch : members)
        vm.processNoteOffEvent(0, ch, 60, -1, 0.8);
    REQUIRE_VOICE_COUNTS(4, 4);

    // Sustain release on channel 8 unlatches them
    vm.updateSustainPedal(0, 8, 0);
    REQUIRE_VOICE_COUNTS(4, 0);
    tp.processFor(6);
    REQUIRE_NO_VOICES;
}