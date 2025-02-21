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

TEST_CASE("Note ID in Poly Mode")
{
    SECTION("No Overlapping PCK")
    {
        auto tp = TestPlayer<32, false>();
        typedef TestPlayer<32, false>::voiceManager_t vm_t;
        typedef TestPlayer<32, false>::Voice vc_t;
        vm_t &vm = tp.voiceManager;

        vm.processNoteOnEvent(0, 1, 60, 173, 0.8, 0.0);
        vm.processNoteOnEvent(0, 1, 62, 179, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(2, 2);
        REQUIRE(tp.activeVoicesMatching([](const vc_t &v) { return v.noteid() == 173; }) == 1);
        REQUIRE(tp.activeVoicesMatching([](const vc_t &v) { return v.noteid() == 179; }) == 1);

        tp.processFor(10);
        REQUIRE_VOICE_COUNTS(2, 2);
        REQUIRE(tp.activeVoicesMatching([](const vc_t &v) { return v.noteid() == 173; }) == 1);
        REQUIRE(tp.activeVoicesMatching([](const vc_t &v) { return v.noteid() == 179; }) == 1);
        vm.processNoteOffEvent(0, 1, 60, 173, 0.8);
        REQUIRE_VOICE_COUNTS(2, 1);
        REQUIRE(tp.activeVoicesMatching([](const vc_t &v)
                                        { return !v.isGated && v.noteid() == 173; }) == 1);
        REQUIRE(tp.activeVoicesMatching([](const vc_t &v)
                                        { return v.isGated && v.noteid() == 179; }) == 1);
        tp.processFor(20);
        REQUIRE_VOICE_COUNTS(1, 1);
        REQUIRE(tp.activeVoicesMatching([](const vc_t &v)
                                        { return v.isGated && v.noteid() == 179; }) == 1);
        vm.processNoteOffEvent(0, 1, 62, 179, 0.8);
        REQUIRE_VOICE_COUNTS(1, 0);
        REQUIRE(tp.activeVoicesMatching([](const vc_t &v)
                                        { return !v.isGated && v.noteid() == 179; }) == 1);
        tp.processFor(20);
        REQUIRE_NO_VOICES;
    }

    SECTION("Incorrect off note id doesn't end")
    {
        auto tp = TestPlayer<32, false>();
        typedef TestPlayer<32, false>::voiceManager_t vm_t;
        typedef TestPlayer<32, false>::Voice vc_t;
        vm_t &vm = tp.voiceManager;

        vm.processNoteOnEvent(0, 1, 60, 173, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(1, 1);
        REQUIRE(tp.activeVoicesMatching([](const vc_t &v) { return v.noteid() == 173; }) == 1);

        tp.processFor(10);

        INFO("Bad NoteID doesn't turn off the note");
        vm.processNoteOffEvent(0, 1, 60, 188242, 0.8);
        REQUIRE_VOICE_COUNTS(1, 1);
        REQUIRE(tp.activeVoicesMatching([](const vc_t &v)
                                        { return v.isGated && v.noteid() == 173; }) == 1);
        tp.processFor(20);

        REQUIRE_VOICE_COUNTS(1, 1);
        vm.processNoteOffEvent(0, 1, 60, 173, 0.8);
        REQUIRE_VOICE_COUNTS(1, 0);
        REQUIRE(tp.activeVoicesMatching([](const vc_t &v)
                                        { return !v.isGated && v.noteid() == 173; }) == 1);
        tp.processFor(20);
        REQUIRE_NO_VOICES;
    }

    SECTION("Overlapping PCK (voice stacking)")
    {
        auto tp = TestPlayer<32, false>();
        typedef TestPlayer<32, false>::voiceManager_t vm_t;
        typedef TestPlayer<32, false>::Voice vc_t;
        vm_t &vm = tp.voiceManager;

        vm.processNoteOnEvent(0, 1, 60, 173, 0.8, 0.0);
        vm.processNoteOnEvent(0, 1, 60, 179, 0.8, 0.0);
        vm.processNoteOnEvent(0, 1, 60, 184, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(3, 3);
        REQUIRE_VOICE_MATCH_FN(3, [](const vc_t &v) { return v.key() == 60; });

        tp.processFor(20);
        vm.processNoteOffEvent(0, 1, 60, 179, 0.8);
        REQUIRE_VOICE_COUNTS(3, 2);
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v)
                               { return v.key() == 60 && v.noteid() == 173 && v.isGated; });
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v)
                               { return v.key() == 60 && v.noteid() == 179 && !v.isGated; });
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v)
                               { return v.key() == 60 && v.noteid() == 184 && v.isGated; });
        tp.processFor(20);

        REQUIRE_VOICE_COUNTS(2, 2);
        vm.processNoteOffEvent(0, 1, 60, 173, 0.8);
        REQUIRE_VOICE_COUNTS(2, 1)
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v)
                               { return v.key() == 60 && v.noteid() == 173 && !v.isGated; });
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v)
                               { return v.key() == 60 && v.noteid() == 184 && v.isGated; });
        tp.processFor(20);

        REQUIRE_VOICE_COUNTS(1, 1);
        vm.processNoteOffEvent(0, 1, 60, 184, 0.8);
        REQUIRE_VOICE_COUNTS(1, 0)
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v)
                               { return v.key() == 60 && v.noteid() == 184 && !v.isGated; });

        tp.processFor(20);
        REQUIRE_NO_VOICES;
    }

    SECTION("Overlapping PCK on with Wildcard Off")
    {
        auto tp = TestPlayer<32, false>();
        typedef TestPlayer<32, false>::voiceManager_t vm_t;
        typedef TestPlayer<32, false>::Voice vc_t;
        vm_t &vm = tp.voiceManager;

        vm.processNoteOnEvent(0, 1, 60, 173, 0.8, 0.0);
        vm.processNoteOnEvent(0, 1, 60, 179, 0.8, 0.0);
        vm.processNoteOnEvent(0, 1, 60, 184, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(3, 3);
        REQUIRE_VOICE_MATCH_FN(3, [](const vc_t &v) { return v.key() == 60; });

        vm.processNoteOffEvent(0, 1, 60, -1, 0.8);
        REQUIRE_VOICE_COUNTS(3, 0);
        tp.processFor(20);
        REQUIRE_NO_VOICES;
    }
}

TEST_CASE("Note ID in Poly Piano Mode")
{
    /*
     * Piano Mode is the same as poly *except* for repeated key.
     * So the trick is to test this with a single key and see if
     * the note id re-assigns, and then also to test it with a voice
     * stack on a single key (a harder case)
     */
    SECTION("Single Key")
    {
        auto tp = TestPlayer<32, false>();
        typedef TestPlayer<32, false>::voiceManager_t vm_t;
        typedef TestPlayer<32, false>::Voice vc_t;
        vm_t &vm = tp.voiceManager;

        vm.repeatedKeyMode = vm_t::RepeatedKeyMode::PIANO;

        vm.processNoteOnEvent(0, 1, 60, 173, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(1, 1);
        tp.processFor(10);
        vm.processNoteOffEvent(0, 1, 60, 173, 0.8);
        REQUIRE_VOICE_COUNTS(1, 0);
        tp.processFor(20);
        REQUIRE_NO_VOICES;

        // now do the same thing where we restart
        vm.processNoteOnEvent(0, 1, 60, 864, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(1, 1);
        tp.processFor(10);
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v) { return v.key() == 60 && v.noteid() == 864; });

        vm.processNoteOffEvent(0, 1, 60, 864, 0.8);
        REQUIRE_VOICE_COUNTS(1, 0);
        tp.processFor(2);
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v) { return v.key() == 60 && v.noteid() == 864; });
        REQUIRE_VOICE_COUNTS(1, 0);

        vm.processNoteOnEvent(0, 1, 60, 7742, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(1, 1);
        REQUIRE_VOICE_MATCH_FN(1,
                               [](const vc_t &v) { return v.key() == 60 && v.noteid() == 7742; });
        tp.processFor(10);
        vm.processNoteOffEvent(0, 1, 60, 7742, 0.8);
        REQUIRE_VOICE_COUNTS(1, 0);
        tp.processFor(20);
        REQUIRE_VOICE_COUNTS(0, 0);
    }

    SECTION("Stacked Voices")
    {
        auto tp = TestPlayer<32, false>();
        typedef TestPlayer<32, false>::voiceManager_t vm_t;
        typedef TestPlayer<32, false>::Voice vc_t;
        vm_t &vm = tp.voiceManager;

        vm.repeatedKeyMode = vm_t::RepeatedKeyMode::PIANO;

        vm.processNoteOnEvent(0, 1, 60, 173, 0.8, 0.0);
        vm.processNoteOnEvent(0, 1, 60, 174, 0.8, 0.0);
        vm.processNoteOnEvent(0, 1, 60, 175, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(3, 3);
        tp.processFor(10);
        vm.processNoteOffEvent(0, 1, 60, 173, 0.8);
        vm.processNoteOffEvent(0, 1, 60, 174, 0.8);
        vm.processNoteOffEvent(0, 1, 60, 175, 0.8);
        REQUIRE_VOICE_COUNTS(3, 0);
        tp.processFor(2);
        REQUIRE_VOICE_COUNTS(3, 0);

        // now do the same thing where we restart
        vm.processNoteOnEvent(0, 1, 60, 864, 0.8, 0.0);
        vm.processNoteOnEvent(0, 1, 60, 865, 0.8, 0.0);
        vm.processNoteOnEvent(0, 1, 60, 866, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(3, 3);
        tp.processFor(10);
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v) { return v.key() == 60 && v.noteid() == 864; });
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v) { return v.key() == 60 && v.noteid() == 865; });
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v) { return v.key() == 60 && v.noteid() == 866; });
    }

    SECTION("Stacked Voices with Varied Stack Sizes")
    {
        auto tp = TestPlayer<32, false>();
        typedef TestPlayer<32, false>::voiceManager_t vm_t;
        typedef TestPlayer<32, false>::Voice vc_t;
        vm_t &vm = tp.voiceManager;

        vm.repeatedKeyMode = vm_t::RepeatedKeyMode::PIANO;

        vm.processNoteOnEvent(0, 1, 60, 173, 0.8, 0.0);
        vm.processNoteOnEvent(0, 1, 60, 174, 0.8, 0.0);
        vm.processNoteOnEvent(0, 1, 60, 175, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(3, 3);
        tp.processFor(10);
        vm.processNoteOffEvent(0, 1, 60, 173, 0.8);
        vm.processNoteOffEvent(0, 1, 60, 174, 0.8);
        vm.processNoteOffEvent(0, 1, 60, 175, 0.8);
        REQUIRE_VOICE_COUNTS(3, 0);
        tp.processFor(2);
        REQUIRE_VOICE_COUNTS(3, 0);

        // now do the same thing where we restart
        vm.processNoteOnEvent(0, 1, 60, 864, 0.8, 0.0);
        vm.processNoteOnEvent(0, 1, 60, 865, 0.8, 0.0);
        REQUIRE_VOICE_COUNTS(3, 2);
        tp.processFor(2);
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v) { return v.key() == 60 && v.noteid() == 864; });
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v) { return v.key() == 60 && v.noteid() == 865; });
        // this test is stronger than our guarantee. It just so happens that last is the
        // one we don't re-steal but it could be any.
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v) { return v.key() == 60 && v.noteid() == 175; });

        tp.processFor(10);
        REQUIRE_VOICE_COUNTS(2, 2);
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v) { return v.key() == 60 && v.noteid() == 864; });
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v) { return v.key() == 60 && v.noteid() == 865; });
    }
}

TEST_CASE("Note ID On Off works in Mono Modes")
{
    typedef TestPlayer<32, false> player_t;
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    typedef TestPlayer<32, false>::Voice vc_t;

    for (auto mode : {(uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_LEGATO,
                      (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_MONO})
    {
        auto modestr = (mode == (uint64_t)vm_t::MonoPlayModeFeatures::NATURAL_LEGATO) ? "legato" : "mono";
        DYNAMIC_SECTION("Note ID for Single Note works " << modestr)
        {
            auto tp = player_t();
            vm_t &vm = tp.voiceManager;

            vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES, mode);

            vm.processNoteOnEvent(0, 1, 60, 173, 0.8, 0.0);
            REQUIRE_VOICE_COUNTS(1, 1);
            REQUIRE(tp.activeVoicesMatching([](const vc_t &v) { return v.noteid() == 173; }) == 1);
            tp.processFor(10);
            vm.processNoteOffEvent(0, 1, 60, 173, 0.8);
            REQUIRE_VOICE_COUNTS(1, 0);
            REQUIRE(tp.activeVoicesMatching([](const vc_t &v) { return v.noteid() == 173; }) == 1);
            tp.processFor(20);
            REQUIRE_NO_VOICES;
        }

        DYNAMIC_SECTION("On On Off Off works with Note ID " << modestr)
        {
            auto tp = TestPlayer<32, false>();
            typedef TestPlayer<32, false>::voiceManager_t vm_t;
            typedef TestPlayer<32, false>::Voice vc_t;
            vm_t &vm = tp.voiceManager;

            vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES, mode);

            vm.processNoteOnEvent(0, 1, 60, 173, 0.8, 0.0);
            REQUIRE_VOICE_COUNTS(1, 1);
            REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v)
                                   { return v.key() == 60 && v.noteid() == 173; });
            tp.processFor(10);

            // play an e and it moves the note
            vm.processNoteOnEvent(0, 1, 65, 184, 0.8, 0.0);
            REQUIRE_VOICE_COUNTS(1, 1);
            REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v) { return v.key() == 65; });
            tp.processFor(10);

            // Now release the E whic returns to the C
            vm.processNoteOffEvent(0, 1, 65, 184, 0.8);
            REQUIRE_VOICE_COUNTS(1, 1);
            REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v) { return v.isGated && v.key() == 60; });
            tp.processFor(10);

            // Then release that held c
            vm.processNoteOffEvent(0, 1, 60, 173, 0.8);
            REQUIRE_VOICE_COUNTS(1, 0);
            REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v) { return !v.isGated && v.key() == 60; });
            tp.processFor(20);

            REQUIRE_NO_VOICES;
        }

        // Now dynamically do a set of three note versions
        std::vector<std::pair<uint8_t, int32_t>> notes{
            {60, 1842}, {65, 104242}, {70, 819}, {65, 2223}};

        struct Instruction
        {
            int idx;
            bool on;
            float expectedidx;
        };

        typedef std::vector<Instruction> testCase;
        std::vector<testCase> testCases{{{0, true, 0},
                                         {1, true, 1},
                                         {2, true, 2},
                                         {2, false, 1},
                                         {1, false, 0},
                                         {0, false, -1}},
                                        {{0, true, 0}, {1, true, 1}, {0, false, 1}, {1, false, -1}},

                                        {
                                            {0, true, 0},
                                            {1, true, 1},
                                            {2, true, 2},
                                            {1, false, 2},
                                            {3, true, 3},
                                            {3, false, 2},
                                            {1, false, 2},
                                            {0, false, 2},
                                            {2, false, -1},
                                        }};

        int tidx{0};

        for (auto &tc : testCases)
        {
            DYNAMIC_SECTION("Test Case " << tidx++ << " " << modestr)
            {
                auto tp = TestPlayer<32, false>();
                typedef TestPlayer<32, false>::voiceManager_t vm_t;
                typedef TestPlayer<32, false>::Voice vc_t;
                vm_t &vm = tp.voiceManager;

                vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES, mode);

                for (const auto &inst : tc)
                {
                    auto n = notes[inst.idx];
                    if (inst.on)
                    {
                        vm.processNoteOnEvent(0, 1, n.first, n.second, 0.8, 0.0);
                    }
                    else
                    {
                        vm.processNoteOffEvent(0, 1, n.first, n.second, 0.8);
                    }
                    if (inst.expectedidx != -1)
                    {
                        REQUIRE_VOICE_MATCH_FN(1, [n = notes[inst.expectedidx]](const vc_t &v)
                                               { return v.key() == n.first; });
                    }
                    else
                    {
                        REQUIRE_VOICE_COUNTS(1, 0);
                    }
                    tp.processFor(5);
                }
            }
        }
    }
}