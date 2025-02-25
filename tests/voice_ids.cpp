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

TEST_CASE("Voice ID in Piano Mode")
{
    typedef TestPlayer<32, false> player_t;
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    typedef TestPlayer<32, false>::Voice vc_t;

    SECTION("Single Note and Termination")
    {
        auto tp = player_t();
        vm_t &vm = tp.voiceManager;
        vm.repeatedKeyMode = vm_t::RepeatedKeyMode::PIANO;

        REQUIRE(vm.getVoiceCount() == 0);

        tp.terminatedVoiceSet.clear();

        vm.processNoteOnEvent(0, 0, 60, 742, 0.0, 0);
        REQUIRE_VOICE_COUNTS(1, 1);
        tp.processFor(10);
        vm.processNoteOffEvent(0, 0, 60, 742, 0.0);
        REQUIRE_VOICE_COUNTS(1, 0);
        tp.processFor(10);
        REQUIRE_VOICE_COUNTS(0, 0);

        REQUIRE(tp.terminatedVoiceSet.find(742) != tp.terminatedVoiceSet.end());
    }

    SECTION("Two Notes; Instant old note termination")
    {
        auto tp = player_t();
        vm_t &vm = tp.voiceManager;
        vm.repeatedKeyMode = vm_t::RepeatedKeyMode::PIANO;
        REQUIRE(vm.getVoiceCount() == 0);

        tp.terminatedVoiceSet.clear();

        vm.processNoteOnEvent(0, 0, 60, 742, 0.0, 0);
        REQUIRE_VOICE_COUNTS(1, 1);
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v) { return v.voiceId == 742; });
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v) { return v.key() == 60; });
        REQUIRE(tp.terminatedVoiceSet.empty());
        tp.processFor(10);
        vm.processNoteOffEvent(0, 0, 60, 742, 0.0);
        tp.processFor(2);

        vm.processNoteOnEvent(0, 0, 60, 8433, 0.0, 0);
        REQUIRE_VOICE_COUNTS(1, 1);
        INFO("In piano mode, the *new* voice id should win since its a new voice");
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v) { return v.voiceId == 8433; });
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v) { return v.key() == 60; });
        REQUIRE(tp.terminatedVoiceSet.find(742) != tp.terminatedVoiceSet.end());
        REQUIRE(tp.terminatedVoiceSet.size() == 1);
        tp.terminatedVoiceSet.clear();
        tp.processFor(20);
        vm.processNoteOffEvent(0, 0, 60, 8433, 0.0);
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v) { return v.voiceId == 8433; });
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v) { return v.key() == 60; });
        REQUIRE(tp.terminatedVoiceSet.empty());
        tp.processFor(10);

        REQUIRE_VOICE_COUNTS(0, 0);
        REQUIRE(tp.terminatedVoiceSet.find(8433) != tp.terminatedVoiceSet.end());
        REQUIRE(tp.terminatedVoiceSet.size() == 1);
    }

    SECTION("Two Notes; Properties Route")
    {
        auto tp = player_t();
        vm_t &vm = tp.voiceManager;
        vm.repeatedKeyMode = vm_t::RepeatedKeyMode::PIANO;
        REQUIRE(vm.getVoiceCount() == 0);

        tp.terminatedVoiceSet.clear();

        vm.processNoteOnEvent(0, 0, 60, 742, 0.0, 0);
        REQUIRE_VOICE_COUNTS(1, 1);
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v) { return v.voiceId == 742; });
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v) { return v.key() == 60; });

        vm.routePolyphonicParameterModulation(0, 0, 60, 742, 123, 17.2);
        vm.routeNoteExpression(0, 0, 60, 742, 11, 0.2);
        REQUIRE_VOICE_MATCH_FN(
            1, [](const vc_t &v)
            { return v.voiceId == 742 && v.paramModulationCache.at(123) == 17.2; });
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v)
                               { return v.voiceId == 742 && v.noteExpressionCache.at(11) == 0.2; });

        vm.processNoteOffEvent(0, 0, 60, 742, 0.0);
        tp.processFor(2);

        vm.routePolyphonicParameterModulation(0, 0, 60, 742, 123, 9.2);
        REQUIRE_VOICE_MATCH_FN(1,
                               [](const vc_t &v) {
                                   return v.voiceId == 742 && v.paramModulationCache.at(123) == 9.2;
                               });

        vm.processNoteOnEvent(0, 0, 60, 8433, 0.0, 0);
        REQUIRE_VOICE_COUNTS(1, 1);
        INFO("In piano mode, the *new* voice id should win since its a new voice");
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v) { return v.voiceId == 8433; });
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v) { return v.key() == 60; });
        vm.routePolyphonicParameterModulation(0, 0, 60, 8433, 123, 17.7);
        vm.routeNoteExpression(0, 0, 60, 8433, 11, 0.7);
        REQUIRE_VOICE_MATCH_FN(
            1, [](const vc_t &v)
            { return v.voiceId == 8433 && v.paramModulationCache.at(123) == 17.7; });
        REQUIRE_VOICE_MATCH_FN(
            1,
            [](const vc_t &v) { return v.voiceId == 8433 && v.noteExpressionCache.at(11) == 0.7; });
        REQUIRE(tp.terminatedVoiceSet.find(742) != tp.terminatedVoiceSet.end());
        REQUIRE(tp.terminatedVoiceSet.size() == 1);
        tp.terminatedVoiceSet.clear();
        tp.processFor(20);
        vm.processNoteOffEvent(0, 0, 60, 8433, 0.0);
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v) { return v.voiceId == 8433; });
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v) { return v.key() == 60; });
        vm.routePolyphonicParameterModulation(0, 0, 60, 8433, 123, 7.7);
        REQUIRE_VOICE_MATCH_FN(
            1, [](const vc_t &v)
            { return v.voiceId == 8433 && v.paramModulationCache.at(123) == 7.7; });

        REQUIRE(tp.terminatedVoiceSet.empty());
        tp.processFor(10);

        REQUIRE_VOICE_COUNTS(0, 0);
        REQUIRE(tp.terminatedVoiceSet.find(8433) != tp.terminatedVoiceSet.end());
        REQUIRE(tp.terminatedVoiceSet.size() == 1);
    }
}

TEST_CASE("Voice ID in Legato Mode")
{
    INFO("See doc/legatoNID.png for what this tests");
    typedef TestPlayer<32, false> player_t;
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    typedef TestPlayer<32, false>::Voice vc_t;

    SECTION("Single Note and Termination")
    {
        auto tp = player_t();
        vm_t &vm = tp.voiceManager;
        vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                       static_cast<uint64_t>(vm_t::MonoPlayModeFeatures::NATURAL_LEGATO));
        REQUIRE(vm.getVoiceCount() == 0);

        tp.terminatedVoiceSet.clear();

        vm.processNoteOnEvent(0, 0, 60, 742, 0.0, 0);
        REQUIRE_VOICE_COUNTS(1, 1);
        tp.processFor(10);
        vm.processNoteOffEvent(0, 0, 60, 742, 0.0);
        REQUIRE_VOICE_COUNTS(1, 0);
        tp.processFor(10);
        REQUIRE_VOICE_COUNTS(0, 0);

        REQUIRE(tp.terminatedVoiceSet.find(742) != tp.terminatedVoiceSet.end());
    }

    SECTION("Two Notes; Instant new note termination")
    {
        auto tp = player_t();
        vm_t &vm = tp.voiceManager;
        vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                       static_cast<uint64_t>(vm_t::MonoPlayModeFeatures::NATURAL_LEGATO));
        REQUIRE(vm.getVoiceCount() == 0);

        tp.terminatedVoiceSet.clear();

        vm.processNoteOnEvent(0, 0, 60, 742, 0.0, 0);
        REQUIRE_VOICE_COUNTS(1, 1);
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v) { return v.voiceId == 742; });
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v) { return v.key() == 60; });
        REQUIRE(tp.terminatedVoiceSet.empty());
        tp.processFor(10);

        vm.processNoteOnEvent(0, 0, 62, 8433, 0.0, 0);
        REQUIRE_VOICE_COUNTS(1, 1);
        INFO("Legato should keep voice with voice id 742 alive and terminate 8433");
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v) { return v.voiceId == 742; });
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v) { return v.key() == 62; });
        REQUIRE(tp.terminatedVoiceSet.find(8433) != tp.terminatedVoiceSet.end());
        REQUIRE(tp.terminatedVoiceSet.size() == 1);
        tp.terminatedVoiceSet.clear();
        tp.processFor(20);
        vm.processNoteOffEvent(0, 0, 62, 8433, 0.0);
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v) { return v.voiceId == 742; });
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v) { return v.key() == 60; });
        REQUIRE(tp.terminatedVoiceSet.empty());
        tp.processFor(10);

        vm.processNoteOffEvent(0, 0, 60, 742, 0.0);
        REQUIRE_VOICE_COUNTS(1, 0);
        REQUIRE(tp.terminatedVoiceSet.empty());
        tp.processFor(10);
        REQUIRE_VOICE_COUNTS(0, 0);

        REQUIRE(tp.terminatedVoiceSet.find(742) != tp.terminatedVoiceSet.end());
        REQUIRE(tp.terminatedVoiceSet.size() == 1);
    }

    SECTION("Note Expressions by NoteID; Param Mod by Voice ID")
    {
        auto tp = player_t();
        vm_t &vm = tp.voiceManager;
        vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                       static_cast<uint64_t>(vm_t::MonoPlayModeFeatures::NATURAL_LEGATO));
        REQUIRE(vm.getVoiceCount() == 0);

        vm.processNoteOnEvent(0, 0, 60, 742, 0.0, 0);
        tp.processFor(2);
        INFO("Param mod is by voice, so the host remembers original key and voice id");
        INFO("Note expression is by note, so host uses latest note id");
        vm.routePolyphonicParameterModulation(0, 0, 60, 742, 123, 17.2);
        vm.routeNoteExpression(0, 0, 60, 742, 11, 0.2);
        REQUIRE_VOICE_MATCH_FN(
            1, [](const vc_t &v)
            { return v.voiceId == 742 && v.paramModulationCache.at(123) == 17.2; });
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v)
                               { return v.voiceId == 742 && v.noteExpressionCache.at(11) == 0.2; });

        vm.processNoteOnEvent(0, 0, 62, 8433, 0.0, 0);
        tp.processFor(2);
        INFO("Voice id routing should still work");
        vm.routePolyphonicParameterModulation(0, 0, 60, 742, 123, 8.2);
        INFO("And routing to the new voice should do nothing since it is terminated");
        vm.routePolyphonicParameterModulation(0, 0, 62, 8433, 123, 8.7);
        REQUIRE_VOICE_MATCH_FN(1,
                               [](const vc_t &v) {
                                   return v.voiceId == 742 && v.paramModulationCache.at(123) == 8.2;
                               });

        INFO("This one is the under voice and should be ignored");
        vm.routeNoteExpression(0, 0, 60, 742, 11, 0.1);
        INFO("This one is the over voice and should apply");
        vm.routeNoteExpression(0, 0, 62, 8433, 11, 0.7);
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v)
                               { return v.voiceId == 742 && v.noteExpressionCache.at(11) == 0.7; });

        tp.processFor(20);
        vm.processNoteOffEvent(0, 0, 62, 8433, 0.0);
        tp.processFor(2);

        INFO("This one is the lead voice and should be applied");
        vm.routeNoteExpression(0, 0, 60, 742, 11, 0.3);
        INFO("This one is the dead voice and should be ignored (sneding this is actually a host "
             "error)");
        vm.routeNoteExpression(0, 0, 62, 8433, 11, 0.4);
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v)
                               { return v.voiceId == 742 && v.noteExpressionCache.at(11) == 0.3; });

        tp.processFor(10);

        vm.processNoteOffEvent(0, 0, 60, 742, 0.0);
        tp.processFor(10);
    }
}

TEST_CASE("Voice ID in Mono Mode")
{
    INFO("See doc/legatoNID.png for what this tests");
    typedef TestPlayer<32, false> player_t;
    typedef TestPlayer<32, false>::voiceManager_t vm_t;
    typedef TestPlayer<32, false>::Voice vc_t;

    SECTION("Single Note and Termination")
    {
        auto tp = player_t();
        vm_t &vm = tp.voiceManager;
        vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                       static_cast<uint64_t>(vm_t::MonoPlayModeFeatures::NATURAL_MONO));
        REQUIRE(vm.getVoiceCount() == 0);

        tp.terminatedVoiceSet.clear();

        vm.processNoteOnEvent(0, 0, 60, 742, 0.0, 0);
        REQUIRE_VOICE_COUNTS(1, 1);
        tp.processFor(10);
        vm.processNoteOffEvent(0, 0, 60, 742, 0.0);
        REQUIRE_VOICE_COUNTS(1, 0);
        tp.processFor(10);
        REQUIRE_VOICE_COUNTS(0, 0);

        REQUIRE(tp.terminatedVoiceSet.find(742) != tp.terminatedVoiceSet.end());
    }

    SECTION("Two Notes; Instant new note termination")
    {
        auto tp = player_t();
        vm_t &vm = tp.voiceManager;
        vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                       static_cast<uint64_t>(vm_t::MonoPlayModeFeatures::NATURAL_MONO));
        REQUIRE(vm.getVoiceCount() == 0);

        tp.terminatedVoiceSet.clear();

        vm.processNoteOnEvent(0, 0, 60, 742, 0.0, 0);
        REQUIRE_VOICE_COUNTS(1, 1);
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v) { return v.voiceId == 742; });
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v) { return v.key() == 60; });
        REQUIRE(tp.terminatedVoiceSet.empty());
        tp.processFor(10);

        vm.processNoteOnEvent(0, 0, 62, 8433, 0.9, 0);
        REQUIRE_VOICE_COUNTS(1, 1);
        INFO("Mono should terminate 742 and initiate 8433 alive and terminate 8433");
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v) { return v.voiceId == 8433; });
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v) { return v.key() == 62; });
        REQUIRE(tp.terminatedVoiceSet.find(742) != tp.terminatedVoiceSet.end());
        REQUIRE(tp.terminatedVoiceSet.size() == 1);
        tp.terminatedVoiceSet.clear();
        tp.processFor(20);

        INFO("Note off terminates 8433 and begins a new voice with no host id");
        vm.processNoteOffEvent(0, 0, 62, 8433, 0.0);
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v) { return v.key() == 60; });
        REQUIRE(tp.terminatedVoiceSet.find(8433) != tp.terminatedVoiceSet.end());
        REQUIRE(tp.terminatedVoiceSet.size() == 1);
        tp.terminatedVoiceSet.clear();
        tp.processFor(10);

        vm.processNoteOffEvent(0, 0, 60, 742, 0.0);
        REQUIRE_VOICE_COUNTS(1, 0);
        REQUIRE(tp.terminatedVoiceSet.empty());
        tp.processFor(10);
        REQUIRE_VOICE_COUNTS(0, 0);
        REQUIRE(tp.terminatedVoiceSet.empty());
    }

    SECTION("Note Expressions by NoteID; Param Mod by Voice ID")
    {
        auto tp = player_t();
        vm_t &vm = tp.voiceManager;
        vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                       static_cast<uint64_t>(vm_t::MonoPlayModeFeatures::NATURAL_MONO));
        REQUIRE(vm.getVoiceCount() == 0);

        vm.processNoteOnEvent(0, 0, 60, 742, 0.0, 0);
        tp.processFor(2);
        INFO("Param mod is by voice, so the host remembers original key and voice id");
        INFO("Note expression is by note, so host uses latest note id");
        vm.routePolyphonicParameterModulation(0, 0, 60, 742, 123, 17.2);
        vm.routeNoteExpression(0, 0, 60, 742, 11, 0.2);
        REQUIRE_VOICE_MATCH_FN(
            1, [](const vc_t &v)
            { return v.voiceId == 742 && v.paramModulationCache.at(123) == 17.2; });
        REQUIRE_VOICE_MATCH_FN(1, [](const vc_t &v)
                               { return v.voiceId == 742 && v.noteExpressionCache.at(11) == 0.2; });

        vm.processNoteOnEvent(0, 0, 62, 8433, 0.0, 0);
        tp.processFor(2);
        vm.routePolyphonicParameterModulation(0, 0, 62, 8433, 123, 8.7);
        REQUIRE_VOICE_MATCH_FN(
            1, [](const vc_t &v)
            { return v.voiceId == 8433 && v.paramModulationCache.at(123) == 8.7; });

        INFO("This one is the under voice and should be ignored");
        vm.routeNoteExpression(0, 0, 60, 742, 11, 0.1);
        INFO("This one is the over voice and should apply");
        vm.routeNoteExpression(0, 0, 62, 8433, 11, 0.7);
        REQUIRE_VOICE_MATCH_FN(
            1,
            [](const vc_t &v) { return v.voiceId == 8433 && v.noteExpressionCache.at(11) == 0.7; });

        tp.processFor(20);
        vm.processNoteOffEvent(0, 0, 62, 8433, 0.0);
        tp.processFor(2);

        INFO("At this point there's no voice and note notide in mono mode");
        tp.processFor(10);
        vm.processNoteOffEvent(0, 0, 60, 742, 0.0);
        tp.processFor(10);
        REQUIRE_NO_VOICES;
    }
}
