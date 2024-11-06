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

TEST_CASE("Mono Mode - Single key releases not terminates")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_MONO);

    REQUIRE_NO_VOICES;

    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
    REQUIRE_VOICE_COUNTS(1, 1);
    vm.processNoteOffEvent(0, 0, 60, -1, 0.8);
    REQUIRE_VOICE_COUNTS(1, 0);
    tp.processFor(10);
    REQUIRE_VOICE_COUNTS(0, 0);
}
TEST_CASE("Mono Mode - Single Layer")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    REQUIRE_NO_VOICES;
    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_MONO);

    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);

    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);
    REQUIRE_VOICE_MATCH(0, v.key() != 60);

    vm.processNoteOnEvent(0, 0, 62, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 62);
    REQUIRE_VOICE_MATCH(0, v.key() != 62);

    vm.processNoteOffEvent(0, 0, 62, -1, 0.0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);
    REQUIRE_VOICE_MATCH(0, v.key() != 60);

    vm.processNoteOffEvent(0, 0, 60, -1, 0.0);
    REQUIRE_VOICE_COUNTS(1, 0);
    tp.processFor(10);
    REQUIRE_NO_VOICES;
}

TEST_CASE("Mono Mode - Release Non Playing")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    REQUIRE_NO_VOICES;
    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_MONO);

    for (int k = 60; k <= 64; ++k)
    {
        INFO("Launching note " << k);
        vm.processNoteOnEvent(0, 0, k, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(1, 1);
        REQUIRE_VOICE_MATCH_FN(1, [k](auto &v) { return v.key() == k; });
    }

    for (int k = 60; k < 64; ++k)
    {
        INFO("Launching note " << k);
        vm.processNoteOffEvent(0, 0, k, -1, 0.8);
        REQUIRE_VOICE_COUNTS(1, 1);
        REQUIRE_VOICE_MATCH_FN(1, [](auto &v) { return v.key() == 64; });
    }

    vm.processNoteOffEvent(0, 0, 64, -1, 0.8);
    REQUIRE_VOICE_COUNTS(1, 0);
    tp.processFor(10);
    REQUIRE_NO_VOICES;
}

TEST_CASE("Mono Mode - Three Notes, Most Recent (default)")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    REQUIRE_NO_VOICES;
    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_MONO);

    INFO("Play notes in order, 60, 58, 62");
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);
    REQUIRE_VOICE_MATCH(0, v.key() != 60);

    vm.processNoteOnEvent(0, 0, 58, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 58);
    REQUIRE_VOICE_MATCH(0, v.key() != 58);

    vm.processNoteOnEvent(0, 0, 62, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 62);
    REQUIRE_VOICE_MATCH(0, v.key() != 62);

    INFO("With 62 sounding, releasing it returns to newest, which is 58");
    vm.processNoteOffEvent(0, 0, 62, -1, 0.0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 58);
    REQUIRE_VOICE_MATCH(0, v.key() != 58);

    INFO("And releasing 68 returns to 60");
    vm.processNoteOffEvent(0, 0, 58, -1, 0.0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);
    REQUIRE_VOICE_MATCH(0, v.key() != 60);

    INFO("The release of which returns to empty");
    vm.processNoteOffEvent(0, 0, 60, -1, 0.0);
    REQUIRE_VOICE_COUNTS(1, 0);
    tp.processFor(10);
    REQUIRE_NO_VOICES;
}

TEST_CASE("Mono Mode - Highest Prio")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    REQUIRE_NO_VOICES;
    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::MONO_RETRIGGER |
                       (uint64_t)vm_t::MonoPlayModeFeatures::ON_RELEASE_TO_HIGHEST);

    INFO("Play notes in order, 60, 58, 62");
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);
    REQUIRE_VOICE_MATCH(0, v.key() != 60);

    vm.processNoteOnEvent(0, 0, 58, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 58);
    REQUIRE_VOICE_MATCH(0, v.key() != 58);

    vm.processNoteOnEvent(0, 0, 62, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 62);
    REQUIRE_VOICE_MATCH(0, v.key() != 62);

    INFO("With 62 sounding, releasing it returns to highest, which is 60");
    vm.processNoteOffEvent(0, 0, 62, -1, 0.0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);
    REQUIRE_VOICE_MATCH(0, v.key() != 60);

    INFO("And releasing 60 returns to 58");
    vm.processNoteOffEvent(0, 0, 60, -1, 0.0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 58);
    REQUIRE_VOICE_MATCH(0, v.key() != 58);

    INFO("The release of which returns to empty");
    vm.processNoteOffEvent(0, 0, 58, -1, 0.0);
    REQUIRE_VOICE_COUNTS(1, 0);
    tp.processFor(10);
    REQUIRE_NO_VOICES;
}
TEST_CASE("Mono Mode - Lowest Prio")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    REQUIRE_NO_VOICES;
    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::MONO_RETRIGGER |
                       (uint64_t)vm_t::MonoPlayModeFeatures::ON_RELEASE_TO_LOWEST);

    INFO("Play notes in order, 60, 58, 62");

    vm.processNoteOnEvent(0, 0, 58, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 58);
    REQUIRE_VOICE_MATCH(0, v.key() != 58);

    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);
    REQUIRE_VOICE_MATCH(0, v.key() != 60);

    vm.processNoteOnEvent(0, 0, 62, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 62);
    REQUIRE_VOICE_MATCH(0, v.key() != 62);

    INFO("With 62 sounding, releasing it returns to lowest, which is 58");
    vm.processNoteOffEvent(0, 0, 62, -1, 0.0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 58);
    REQUIRE_VOICE_MATCH(0, v.key() != 58);

    INFO("And releasing 58 returns to 60");
    vm.processNoteOffEvent(0, 0, 58, -1, 0.0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);
    REQUIRE_VOICE_MATCH(0, v.key() != 60);

    INFO("The release of which returns to empty");
    vm.processNoteOffEvent(0, 0, 60, -1, 0.0);
    REQUIRE_VOICE_COUNTS(1, 0);
    tp.processFor(10);
    REQUIRE_NO_VOICES;
}

TEST_CASE("Mono Mode - Two Layers (Duophonic)")
{
    auto tp = TestPlayer<32, false>();
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    INFO("Put EVEN and ODD keys in different monophonic groups");
    tp.polyGroupForKey = [](auto k) { return (k % 2 == 0 ? 1477 : 1832); };
    vm.setPlaymode(1477, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_MONO);
    vm.setPlaymode(1832, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_MONO);

    REQUIRE_NO_VOICES;
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);
    REQUIRE_VOICE_MATCH(0, v.key() != 60);

    vm.processNoteOnEvent(0, 0, 61, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(2, 2);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);
    REQUIRE_VOICE_MATCH(1, v.key() == 61);

    vm.processNoteOnEvent(0, 0, 64, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(2, 2);
    REQUIRE_VOICE_MATCH(1, v.key() == 64);
    REQUIRE_VOICE_MATCH(1, v.key() == 61);

    vm.processNoteOnEvent(0, 0, 63, -1, 0.8, 0.0);
    REQUIRE_VOICE_COUNTS(2, 2);
    REQUIRE_VOICE_MATCH(1, v.key() == 64);
    REQUIRE_VOICE_MATCH(1, v.key() == 63);

    vm.processNoteOffEvent(0, 0, 64, -1, 0.9);
    REQUIRE_VOICE_COUNTS(2, 2);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);
    REQUIRE_VOICE_MATCH(1, v.key() == 63);

    INFO("The 60 release should put one of the groups in release state");
    vm.processNoteOffEvent(0, 0, 60, -1, 0.9);
    REQUIRE_VOICE_COUNTS(2, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 63);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);
    tp.processFor(10);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 63);

    vm.processNoteOffEvent(0, 0, 63, -1, 0.9);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE_VOICE_MATCH(1, v.key() == 61);

    vm.processNoteOffEvent(0, 0, 61, -1, 0.9);
    REQUIRE_VOICE_COUNTS(1, 0);
    tp.processFor(10);
    REQUIRE_NO_VOICES;
}

TEST_CASE("Mono Mode - Sustain Pedal")
{
    SECTION("Release with Gated When Releasing")
    {
        auto tp = TestPlayer<32, false>();
        typedef TestPlayer<32, false>::voiceManager_t vm_t;
        auto &vm = tp.voiceManager;

        vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                       (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_MONO);
        REQUIRE_NO_VOICES;

        vm.updateSustainPedal(0, 0, 127);
        REQUIRE_NO_VOICES;
        INFO("PLay 60 launches 60");
        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
        REQUIRE_VOICE_MATCH(1, v.key() == 60);

        INFO("Mono move to 62 gets us there");
        vm.processNoteOnEvent(0, 0, 62, -1, 0.8, 0);
        REQUIRE_VOICE_MATCH(1, v.key() == 62);

        INFO("The release of 62 jumps back to 60 because it is still held as a gated key");
        vm.processNoteOffEvent(0, 0, 62, -1, 0.8);
        REQUIRE_VOICE_MATCH(1, v.key() == 60);

        INFO("THe release of 60 keeps it playing due to pedal");
        vm.processNoteOffEvent(0, 0, 60, -1, 0.8);
        REQUIRE_VOICE_MATCH(1, v.key() == 60);

        INFO("Release the pedal, kill the voices");
        vm.updateSustainPedal(0, 0, 0);
        REQUIRE_NO_VOICES
    }

    SECTION("Release when non-gated but pedal held (surge 6620)")
    {
        auto tp = TestPlayer<32, false>();
        typedef TestPlayer<32, false>::voiceManager_t vm_t;
        auto &vm = tp.voiceManager;

        vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                       (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_MONO);
        REQUIRE_NO_VOICES;

        vm.updateSustainPedal(0, 0, 127);
        REQUIRE_NO_VOICES;
        INFO("Note 60 gives us note 60");
        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
        REQUIRE_VOICE_MATCH(1, v.key() == 60);

        INFO("The release of 60 doesn't do anything musically");
        vm.processNoteOffEvent(0, 0, 60, -1, 0.8);
        REQUIRE_VOICE_MATCH(1, v.key() == 60);

        INFO("The press of 62 moves you to 62");
        vm.processNoteOnEvent(0, 0, 62, -1, 0.8, 0.f);
        REQUIRE_VOICE_COUNTS(1, 1);
        REQUIRE_VOICE_MATCH(1, v.key() == 62);

        INFO("The release of 62 keeys you on 62 since nothing is gated");
        vm.processNoteOffEvent(0, 0, 62, -1, 0.8);
        REQUIRE_VOICE_MATCH(1, v.key() == 62);

        INFO("The release of the pdeal silences us");
        vm.updateSustainPedal(0, 0, 0);
        REQUIRE_NO_VOICES
    }
}

TEST_CASE("Mono Mode - Two Layers, One Poly")
{
    SECTION("Case one")
    {
        auto tp = TestPlayer<32, true>();
        typedef TestPlayer<32, true>::voiceManager_t vm_t;
        auto &vm = tp.voiceManager;

        INFO("Put EVEN and ODD keys in different groups, even poly odd mono");
        tp.polyGroupForKey = [](auto k) { return (k % 2 == 0 ? 19884 : 8675309); };
        vm.setPlaymode(19884, vm_t::PlayMode::POLY_VOICES);
        vm.setPlaymode(8675309, vm_t::PlayMode::MONO_NOTES,
                       (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_MONO);

        INFO("Play three poly voices");
        for (int k = 60; k < 65; k += 2)
        {
            vm.processNoteOnEvent(0, 0, k, -1, 0.8, 0.0);
        }
        REQUIRE_VOICE_COUNTS(3, 3);

        INFO("Play note 61 for an extra voice");
        vm.processNoteOnEvent(0, 0, 61, -1, 0.8, 0.0);
        tp.processFor(1);
        REQUIRE_VOICE_COUNTS(4, 4);
        REQUIRE_VOICE_MATCH(1, v.key() == 61);

        INFO("Mono move that to 63");
        vm.processNoteOnEvent(0, 0, 63, -1, 0.8, 0.0);
        tp.processFor(1);
        REQUIRE_VOICE_COUNTS(4, 4);
        REQUIRE_VOICE_MATCH(1, v.key() == 63);

        INFO("Release a poly voice");
        vm.processNoteOffEvent(0, 0, 62, -1, 0.8);
        tp.processFor(1);
        REQUIRE_VOICE_COUNTS(4, 3); // remember that poly voice is in release mode

        INFO("Release the 63 to go back to 61");
        vm.processNoteOffEvent(0, 0, 63, -1, 0.8);
        tp.processFor(1);
        REQUIRE_VOICE_COUNTS(4, 3); // remember that poly voice is in release mode
        tp.processFor(10);
        REQUIRE_VOICE_COUNTS(3, 3); // and that poly voice is no longer released

        INFO("Release the 61 and you get a release mono voice");
        vm.processNoteOffEvent(0, 0, 61, -1, 0.8);
        tp.processFor(1);
        REQUIRE_VOICE_COUNTS(3, 2);
        REQUIRE_VOICE_MATCH(1, v.key() == 61);
        tp.processFor(10);
        REQUIRE_VOICE_COUNTS(2, 2);
        REQUIRE_VOICE_MATCH(0, v.key() == 61);
    }

    SECTION("Retrigger during release - base case")
    {
        auto tp = TwoGroupsEveryKey<32, true>();
        typedef TwoGroupsEveryKey<32, true>::voiceManager_t vm_t;
        auto &vm = tp.voiceManager;

        vm.setPlaymode(2112, vm_t::PlayMode::POLY_VOICES);
        vm.setPlaymode(90125, vm_t::PlayMode::MONO_NOTES,
                       (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_MONO);

        INFO("Play three poly voices overlapped with one mono");
        for (int k = 60; k < 65; k += 2)
        {
            vm.processNoteOnEvent(0, 0, k, -1, 0.8, 0.0);
        }
        REQUIRE_VOICE_COUNTS(4, 4);

        tp.processFor(2);
        INFO("Now release everything");
        for (int k = 60; k < 65; k += 2)
        {
            vm.processNoteOffEvent(0, 0, k, -1, 0.8);
        }
        REQUIRE_VOICE_COUNTS(4, 0);
        tp.processFor(10);
        REQUIRE_VOICE_COUNTS(0, 0);
    }

    SECTION("Retrigger in Release - Test Case")
    {
        auto tp = TwoGroupsEveryKey<32, true>();
        typedef TwoGroupsEveryKey<32, true>::voiceManager_t vm_t;
        auto &vm = tp.voiceManager;
        vm.setPlaymode(2112, vm_t::PlayMode::POLY_VOICES);
        vm.setPlaymode(90125, vm_t::PlayMode::MONO_NOTES,
                       (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_MONO);

        INFO("Now do it again but retrigger during release");
        for (int k = 60; k < 65; k += 2)
        {
            vm.processNoteOnEvent(0, 0, k, -1, 0.8, 0.0);
        }
        REQUIRE_VOICE_COUNTS(4, 4);

        tp.processFor(2);
        INFO("Now release everything");
        for (int k = 60; k < 65; k += 2)
        {
            vm.processNoteOffEvent(0, 0, k, -1, 0.8);
        }
        REQUIRE_VOICE_COUNTS(4, 0);
        tp.processFor(1);
        REQUIRE_VOICE_COUNTS(4, 0);

        INFO("Now retrigger two notes - the ringing mono moves and we add a poly");
        vm.processNoteOnEvent(0, 0, 55, -1, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(5, 2);
        tp.processFor(1);

        vm.processNoteOnEvent(0, 0, 54, -1, 0.8, 0.0);
        INFO("This should add 2 poly gated voices and one mono since we are in piano mode");
        REQUIRE_VOICE_COUNTS(6, 3);

        INFO("Let the ungated ring out");
        tp.processFor(10);
        REQUIRE_VOICE_COUNTS(3, 3);
    }
}

TEST_CASE("Mono terminates a non-gated release voice")
{
    SECTION("Case one - different key")
    {
        auto tp = TestPlayer<32, false>();
        typedef TestPlayer<32, false>::voiceManager_t vm_t;
        auto &vm = tp.voiceManager;

        vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                       (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_MONO);

        REQUIRE_NO_VOICES;

        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
        REQUIRE_VOICE_COUNTS(1, 1);
        REQUIRE_VOICE_MATCH(1, v.key() == 60);
        tp.processFor(4);
        vm.processNoteOffEvent(0, 0, 60, -1, 0.8);
        REQUIRE_VOICE_COUNTS(1, 0);
        REQUIRE_VOICE_MATCH(1, v.key() == 60);
        tp.processFor(2);
        REQUIRE_VOICE_COUNTS(1, 0);
        REQUIRE_VOICE_MATCH(1, v.key() == 60);
        vm.processNoteOnEvent(0, 0, 64, -1, 0.8, 0);
        REQUIRE_VOICE_COUNTS(1, 1);
        REQUIRE_VOICE_MATCH(0, v.key() == 60);
        REQUIRE_VOICE_MATCH(1, v.key() == 64);
    }

    SECTION("Case one - same key")
    {
        auto tp = TestPlayer<32, false>();
        typedef TestPlayer<32, false>::voiceManager_t vm_t;
        auto &vm = tp.voiceManager;

        vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                       (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_MONO);

        REQUIRE_NO_VOICES;

        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
        REQUIRE_VOICE_COUNTS(1, 1);
        REQUIRE_VOICE_MATCH(1, v.key() == 60);
        tp.processFor(4);
        vm.processNoteOffEvent(0, 0, 60, -1, 0.8);
        REQUIRE_VOICE_COUNTS(1, 0);
        REQUIRE_VOICE_MATCH(1, v.key() == 60);
        tp.processFor(2);
        REQUIRE_VOICE_COUNTS(1, 0);
        REQUIRE_VOICE_MATCH(1, v.key() == 60);
        vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
        REQUIRE_VOICE_COUNTS(1, 1);
        REQUIRE_VOICE_MATCH(1, v.key() == 60);
    }
}

TEST_CASE("Mono Mode - Poly and Mono on same key with multi-voice start")
{
    // A special player which for every key makes 2 voices, one in
    // group 2112, one in group 90125
    auto tp = TwoGroupsEveryKey<32, true>();
    typedef TwoGroupsEveryKey<32, true>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    vm.setPlaymode(2112, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_MONO);

    REQUIRE_NO_VOICES;
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0.);
    REQUIRE_VOICE_COUNTS(2, 2);
    REQUIRE_VOICE_MATCH(2, v.key() == 60);
    tp.processFor(1);

    vm.processNoteOnEvent(0, 0, 62, -1, 0.8, 0.);
    REQUIRE_VOICE_COUNTS(3, 3);
    REQUIRE_VOICE_MATCH(1, v.key() == 60);
    REQUIRE_VOICE_MATCH(2, v.key() == 62);
    tp.processFor(1);

    vm.processNoteOffEvent(0, 0, 62, -1, 0.0);
    REQUIRE_VOICE_COUNTS(3, 2);
    REQUIRE_VOICE_MATCH(2, v.key() == 60);
    REQUIRE_VOICE_MATCH(1, v.key() == 62);
}

TEST_CASE("Mono Mode - Layerd Retrigger Miss")
{
    auto tp = TwoGroupsEveryKey<32, true>();
    typedef TwoGroupsEveryKey<32, true>::voiceManager_t vm_t;
    auto &vm = tp.voiceManager;

    vm.setPlaymode(2112, vm_t::PlayMode::MONO_NOTES,
                   (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_MONO);

    vm.processNoteOnEvent(0, 0, 58, -1, 0.8, 0);
    tp.processFor(1);
    REQUIRE_KEY_COUNT(2, 58);
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
    tp.processFor(1);
    REQUIRE_KEY_COUNT(1, 58);
    REQUIRE_KEY_COUNT(2, 60);
    vm.processNoteOffEvent(0, 0, 60, -1, 0.8);
    tp.processFor(1);
    REQUIRE_KEY_COUNT(2, 58);
    REQUIRE_KEY_COUNT(1, 60);
    // Ring out the poly release
    tp.processFor(10);
    REQUIRE_KEY_COUNT(2, 58);
    REQUIRE_KEY_COUNT(0, 60);

    vm.processNoteOnEvent(0, 0, 60, -1, 0.8, 0);
    tp.processFor(1);
    REQUIRE_KEY_COUNT(1, 58);
    REQUIRE_KEY_COUNT(2, 60);
}
