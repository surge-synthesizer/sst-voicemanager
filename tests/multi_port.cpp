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

TEST_CASE("Multi Port Poly Independence")
{
    // Two notes on the same channel+key but different ports are independent voices.
    // Releasing on port=0 only releases the port=0 voice; port=1 voice stays alive.
    TestPlayer<32> tp;
    auto &vm = tp.voiceManager;

    REQUIRE_NO_VOICES;

    uint16_t channel{0}, key{60};
    int32_t noteid{-1};
    float velocity{0.8f}, retune{0.f};

    vm.processNoteOnEvent(0, channel, key, noteid, velocity, retune);
    vm.processNoteOnEvent(1, channel, key, noteid, velocity, retune);
    REQUIRE_VOICE_COUNTS(2, 2);

    // Both voices exist — one on port 0, one on port 1
    REQUIRE(tp.activeVoicesMatching([](const auto &v) { return v.port() == 0; }) == 1);
    REQUIRE(tp.activeVoicesMatching([](const auto &v) { return v.port() == 1; }) == 1);

    tp.processFor(4);

    // Release only the port=0 voice
    vm.processNoteOffEvent(0, channel, key, noteid, velocity);
    REQUIRE_VOICE_COUNTS(2, 1);

    // Port=0 voice is ungated; port=1 voice is still gated
    REQUIRE(tp.activeVoicesMatching([](const auto &v) { return v.port() == 0 && !v.isGated; }) ==
            1);
    REQUIRE(tp.activeVoicesMatching([](const auto &v) { return v.port() == 1 && v.isGated; }) == 1);

    // Let port=0 voice ring out
    tp.processFor(10);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE(tp.activeVoicesMatching([](const auto &v) { return v.port() == 1 && v.isGated; }) == 1);

    // Release port=1 voice
    vm.processNoteOffEvent(1, channel, key, noteid, velocity);
    REQUIRE_VOICE_COUNTS(1, 0);
    tp.processFor(10);
    REQUIRE_NO_VOICES;
}

TEST_CASE("Multi Port Sustain Is Per Channel")
{
    // Sustain is gated per channel (any port's sustain-on sets the channel state).
    // Both voices on ch=0 (different ports) are held when sustain is active on ch=0.
    // Sustain release is port-scoped: updateSustainPedal(port, ch, 0) only unlatches
    // voices that match that port. Send sustain-off on each port to unlatch each voice.
    TestPlayer<32> tp;
    auto &vm = tp.voiceManager;

    REQUIRE_NO_VOICES;

    vm.processNoteOnEvent(0, 0, 60, -1, 0.8f, 0.f);
    vm.processNoteOnEvent(1, 0, 60, -1, 0.8f, 0.f);
    REQUIRE_VOICE_COUNTS(2, 2);

    // Apply sustain on port=0 ch=0 (sets sustainOn[ch=0] = true for all ports)
    vm.updateSustainPedal(0, 0, 127);

    // Release both notes — both voices should be held by sustain
    vm.processNoteOffEvent(0, 0, 60, -1, 0.8f);
    vm.processNoteOffEvent(1, 0, 60, -1, 0.8f);
    REQUIRE_VOICE_COUNTS(2, 2);

    // sustainOn[channel] is a single bool. Sustain-off on port=0 clears it and releases
    // only voices matching port=0. The port=1 voice remains gated.
    vm.updateSustainPedal(0, 0, 0);
    REQUIRE_VOICE_COUNTS(2, 1);
    REQUIRE(tp.activeVoicesMatching([](const auto &v) { return v.port() == 1 && v.isGated; }) == 1);

    // sustainOn is already false; a second sustain-off on port=1 is a no-op.
    // Release the port=1 voice via a normal note-off (sustain is now off).
    vm.processNoteOffEvent(1, 0, 60, -1, 0.8f);
    REQUIRE_VOICE_COUNTS(2, 0);

    tp.processFor(10);
    REQUIRE_NO_VOICES;
}

TEST_CASE("Multi Port Mono Moves Across Ports")
{
    // Mono mode is group-wide: a note on port=1 retriggers the group's voice regardless of port.
    // However, doMonoRetrigger on release only searches keyState for the releasing port,
    // so releasing on port=1 does NOT retrigger to a held key on port=0.
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_MONO);

    REQUIRE_NO_VOICES;

    // Press key=60 on port=0
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8f, 0.f);
    REQUIRE_VOICE_COUNTS(1, 1);

    // Press key=62 on port=1 — mono is group-wide: old voice terminates, new voice on port=1
    vm.processNoteOnEvent(1, 0, 62, -1, 0.8f, 0.f);
    tp.processFor(10);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE(tp.activeVoicesMatching([](const auto &v)
                                    { return v.port() == 1 && v.key() == 62 && v.isGated; }) == 1);

    // Release key=62 on port=1 — doMonoRetrigger only searches port=1 held keys,
    // so key=60 on port=0 is NOT picked up; voice terminates.
    vm.processNoteOffEvent(1, 0, 62, -1, 0.8f);
    tp.processFor(10);
    REQUIRE_NO_VOICES;

    // Clean up port=0 held key (no voice, but keyState still has it)
    vm.processNoteOffEvent(0, 0, 60, -1, 0.8f);
}

TEST_CASE("Multi Port Legato Moves Across Ports")
{
    // Legato is group-wide: pressing on port=1 legato-moves the voice from port=0.
    // Releasing on port=1 with no other port=1 held keys releases the voice
    // (does not retrigger to port=0's still-held key).
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_LEGATO);

    REQUIRE_NO_VOICES;

    // Press key=60 on port=0
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8f, 0.f);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60 && v.originalKey() == 60);

    // Press key=62 on port=1 — legato move of the existing voice to key=62, port=1
    vm.processNoteOnEvent(1, 0, 62, -1, 0.8f, 0.f);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 62 && v.originalKey() == 60);

    // Release key=62 on port=1 — no port=1 held keys remain, so voice releases
    // (key=60 on port=0 is still held but not searched by doMonoRetrigger for port=1)
    vm.processNoteOffEvent(1, 0, 62, -1, 0.8f);
    REQUIRE_VOICE_COUNTS(1, 0);
    tp.processFor(10);
    REQUIRE_NO_VOICES;

    // Clean up port=0 held key
    vm.processNoteOffEvent(0, 0, 60, -1, 0.8f);
}
