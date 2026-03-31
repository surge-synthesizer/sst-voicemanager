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
#include "test_player.h" // for REQUIRE_VOICE_COUNTS and friends

/*
 * ContTestPlayer is a minimal single-voice-per-note test player whose Config
 * defines continuationData_t, satisfying HasVoiceContinuationData<Cfg> and
 * activating all continuation-data code paths in the voice manager.
 *
 * continuationData_t = int32_t.
 * Each voice stores donationState = key * 100 at creation.
 * getContinuationData() returns donationState, giving tests a predictable value
 * to assert against without any magic numbers.
 *
 * Each voice also records receivedFromPlayingVoice and receivedContData from
 * the VoiceInitInstructionsEntry it was started with, so tests can inspect
 * exactly what the voice manager passed.
 */
template <size_t N, bool doLog = false> struct ContTestPlayer
{
    using pckn_t = std::tuple<int16_t, int16_t, int16_t, int32_t>;

    struct Voice
    {
        enum State
        {
            UNUSED,
            ACTIVE
        } state{UNUSED};

        int32_t runtime{0};
        bool isGated{false};
        int32_t releaseCountdown{0};
        float velocity{0.f};

        pckn_t pckn{-1, -1, -1, -1};
        pckn_t original_pckn{-1, -1, -1, -1};
        int32_t voiceId{-1};

        // The value this voice would hand off if stolen: set to key*100 at creation.
        int32_t donationState{0};

        // What the voice manager passed in VoiceInitInstructionsEntry when this voice started.
        bool receivedFromPlayingVoice{false};
        int32_t receivedContData{0};

        auto port() const { return std::get<0>(pckn); }
        auto channel() const { return std::get<1>(pckn); }
        auto key() const { return std::get<2>(pckn); }
        auto noteid() const { return std::get<3>(pckn); }
    };

    struct Config
    {
        using voice_t = Voice;
        using continuationData_t = int32_t;
        static constexpr size_t maxVoiceCount{N};
    };

    std::array<Voice, N> voiceStorage{};
    std::function<void(Voice *)> voiceEndCallback{nullptr};

    struct Responder
    {
        ContTestPlayer &player;
        explicit Responder(ContTestPlayer &p) : player(p) {}

        void setVoiceEndCallback(std::function<void(Voice *)> f) { player.voiceEndCallback = f; }

        int32_t beginVoiceCreationTransaction(
            sst::voicemanager::VoiceBeginBufferEntry<Config>::buffer_t &buf, uint16_t /*port*/,
            uint16_t /*channel*/, uint16_t /*key*/, int32_t /*noteid*/, float /*velocity*/)
        {
            buf[0].polyphonyGroup = 0;
            return 1;
        }

        void endVoiceCreationTransaction(uint16_t, uint16_t, uint16_t, int32_t, float) {}

        int32_t initializeMultipleVoices(
            int32_t /*voices*/,
            const sst::voicemanager::VoiceInitInstructionsEntry<Config>::buffer_t &instructions,
            sst::voicemanager::VoiceInitBufferEntry<Config>::buffer_t &initBuf, uint16_t port,
            uint16_t channel, uint16_t key, int32_t noteId, float velocity, float /*retune*/)
        {
            using Instr = sst::voicemanager::VoiceInitInstructionsEntry<Config>::Instruction;
            if (instructions[0].instruction == Instr::SKIP)
            {
                initBuf[0].voice = nullptr;
                return 0;
            }
            for (auto &v : player.voiceStorage)
            {
                if (v.state != Voice::ACTIVE)
                {
                    v.state = Voice::ACTIVE;
                    v.runtime = 0;
                    v.isGated = true;
                    v.releaseCountdown = 0;
                    v.velocity = velocity;
                    v.pckn = {(int16_t)port, (int16_t)channel, (int16_t)key, noteId};
                    v.original_pckn = v.pckn;
                    v.voiceId = noteId;
                    v.donationState = (int16_t)key * 100;
                    v.receivedFromPlayingVoice = instructions[0].fromPlayingVoice;
                    v.receivedContData = instructions[0].continuationData;
                    initBuf[0].voice = &v;
                    return 1;
                }
            }
            return 0;
        }

        void terminateVoice(Voice *v)
        {
            player.voiceEndCallback(v);
            *v = Voice();
        }

        void releaseVoice(Voice *v, float /*velocity*/)
        {
            v->isGated = false;
            v->releaseCountdown = 5;
        }

        void retriggerVoiceWithNewNoteID(Voice *v, int32_t noteid, float velocity)
        {
            v->isGated = true;
            v->releaseCountdown = 0;
            v->velocity = velocity;
            v->voiceId = noteid;
            std::get<3>(v->pckn) = noteid;
        }

        void moveVoice(Voice *v, uint16_t port, uint16_t channel, uint16_t key, float /*velocity*/)
        {
            v->pckn = {(int16_t)port, (int16_t)channel, (int16_t)key,
                       std::get<3>(v->original_pckn)};
        }

        void moveAndRetriggerVoice(Voice *v, uint16_t port, uint16_t channel, uint16_t key,
                                   float velocity)
        {
            v->pckn = {(int16_t)port, (int16_t)channel, (int16_t)key,
                       std::get<3>(v->original_pckn)};
            v->isGated = true;
            v->releaseCountdown = 0;
            v->velocity = velocity;
        }

        void discardHostVoice(int32_t) {}

        // Required because HasVoiceContinuationData<Config> is true.
        // Returns the value the new voice will receive in continuationData.
        int32_t getContinuationData(Voice *v) { return v->donationState; }

        // Routing stubs required by ConstraintsChecker
        void setNoteExpression(Voice *, int32_t, double) {}
        void setVoicePolyphonicParameterModulation(Voice *, uint32_t, double) {}
        void setVoiceMonophonicParameterModulation(Voice *, uint32_t, double) {}
        void setPolyphonicAftertouch(Voice *, int8_t) {}
        void setVoiceMIDIMPEChannelPitchBend(Voice *, uint16_t) {}
        void setVoiceMIDIMPEChannelPressure(Voice *, int8_t) {}
        void setVoiceMIDIMPETimbre(Voice *, int8_t) {}
    } responder;

    struct MonoResponder
    {
        ContTestPlayer &player;
        explicit MonoResponder(ContTestPlayer &p) : player(p) {}
        void setMIDIPitchBend(int16_t, int16_t) {}
        void setMIDI1CC(int16_t, int16_t, int8_t) {}
        void setMIDIChannelPressure(int16_t, int16_t) {}
    } monoResponder;

    using voiceManager_t = sst::voicemanager::VoiceManager<Config, Responder, MonoResponder>;
    voiceManager_t voiceManager;

    ContTestPlayer()
        : responder(*this), monoResponder(*this), voiceManager(responder, monoResponder)
    {
    }

    void process()
    {
        for (auto &v : voiceStorage)
        {
            if (v.state == Voice::ACTIVE && !v.isGated)
            {
                if (--v.releaseCountdown == 0)
                {
                    if (voiceEndCallback)
                        voiceEndCallback(&v);
                    v.state = Voice::UNUSED;
                }
            }
        }
    }

    void processFor(int n)
    {
        while (n--)
            process();
    }

    std::vector<pckn_t> getActiveVoicePCKNS() const
    {
        std::vector<pckn_t> r;
        for (auto &v : voiceStorage)
            if (v.state == Voice::ACTIVE)
                r.push_back(v.pckn);
        return r;
    }

    std::vector<pckn_t> getGatedVoicePCKNS() const
    {
        std::vector<pckn_t> r;
        for (auto &v : voiceStorage)
            if (v.state == Voice::ACTIVE && v.isGated)
                r.push_back(v.pckn);
        return r;
    }

    int32_t activeVoicesMatching(std::function<bool(const Voice &)> cond) const
    {
        int32_t n = 0;
        for (auto &v : voiceStorage)
            if (v.state == Voice::ACTIVE && cond(v))
                ++n;
        return n;
    }
};

// ---------------------------------------------------------------------------
// Helpers to reduce test verbosity
// ---------------------------------------------------------------------------

using CTP = ContTestPlayer<16>;
using vm_t = CTP::voiceManager_t;

static void setNaturalMono(vm_t &vm)
{
    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   static_cast<uint64_t>(vm_t::MonoPlayModeFeatures::NATURAL_MONO));
}

static void setNaturalLegato(vm_t &vm)
{
    vm.setPlaymode(0, vm_t::PlayMode::MONO_NOTES,
                   static_cast<uint64_t>(vm_t::MonoPlayModeFeatures::NATURAL_LEGATO));
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("Continuation Data - Fresh Voice Has No Prior")
{
    INFO("A voice started with no predecessor must have fromPlayingVoice=false");

    auto tp = CTP();
    auto &vm = tp.voiceManager;
    setNaturalMono(vm);

    REQUIRE_NO_VOICES;

    vm.processNoteOnEvent(0, 0, 60, -1, 0.8f, 0.f);
    REQUIRE_VOICE_COUNTS(1, 1);

    REQUIRE(tp.activeVoicesMatching([](const CTP::Voice &v)
                                    { return v.key() == 60 && !v.receivedFromPlayingVoice; }) == 1);
}

TEST_CASE("Continuation Data - Mono Retrigger Note-On Steal")
{
    INFO("When a new note-on steals the active mono voice, getContinuationData is called on the "
         "stolen voice and the result is delivered to the replacement voice via "
         "VoiceInitInstructionsEntry");

    auto tp = CTP();
    auto &vm = tp.voiceManager;
    setNaturalMono(vm);

    // Press key 60; donationState will be 60*100 = 6000
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8f, 0.f);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE(tp.activeVoicesMatching(
                [](const CTP::Voice &v)
                {
                    return v.key() == 60 && v.donationState == 6000 && !v.receivedFromPlayingVoice;
                }) == 1);

    // Press key 62; key 60's voice is stolen and its donationState (6000) must reach key 62's voice
    vm.processNoteOnEvent(0, 0, 62, -1, 0.8f, 0.f);
    REQUIRE_VOICE_COUNTS(1, 1);

    REQUIRE(tp.activeVoicesMatching(
                [](const CTP::Voice &v)
                {
                    return v.key() == 62 && v.receivedFromPlayingVoice &&
                           v.receivedContData == 6000; // donated by key 60
                }) == 1);
}

TEST_CASE("Continuation Data - Mono Retrigger Note-Off Returns With Prior")
{
    INFO("When a mono note-off finds another key held, getContinuationData is called on the "
         "terminating voice before it is destroyed; the value reaches the voice that takes over");

    auto tp = CTP();
    auto &vm = tp.voiceManager;
    setNaturalMono(vm);

    // Hold key 60 throughout
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8f, 0.f);
    REQUIRE_VOICE_COUNTS(1, 1);

    // Press key 62 while 60 is held; 60 is stolen, 62 starts (receivedContData=6000)
    vm.processNoteOnEvent(0, 0, 62, -1, 0.8f, 0.f);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE(tp.activeVoicesMatching([](const CTP::Voice &v) { return v.key() == 62; }) == 1);

    // Release key 62 with key 60 still held.
    // Key 62's voice (donationState=6200) is terminated; key 60 retriggered with contData=6200.
    vm.processNoteOffEvent(0, 0, 62, -1, 0.0f);
    REQUIRE_VOICE_COUNTS(1, 1);

    REQUIRE(tp.activeVoicesMatching(
                [](const CTP::Voice &v)
                {
                    return v.key() == 60 && v.receivedFromPlayingVoice &&
                           v.receivedContData == 6200; // donated by key 62
                }) == 1);
}

TEST_CASE("Continuation Data - Chain Of Steals Propagates Each Time")
{
    INFO("Continuation data must update at every steal, not just the first");

    auto tp = CTP();
    auto &vm = tp.voiceManager;
    setNaturalMono(vm);

    // key 60: donationState=6000, no prior
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8f, 0.f);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE(tp.activeVoicesMatching([](const CTP::Voice &v)
                                    { return v.key() == 60 && !v.receivedFromPlayingVoice; }) == 1);

    // key 62: steals key 60, receives contData=6000; donationState=6200
    vm.processNoteOnEvent(0, 0, 62, -1, 0.8f, 0.f);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE(tp.activeVoicesMatching(
                [](const CTP::Voice &v)
                {
                    return v.key() == 62 && v.receivedFromPlayingVoice &&
                           v.receivedContData == 6000;
                }) == 1);

    // key 64: steals key 62, receives contData=6200; donationState=6400
    vm.processNoteOnEvent(0, 0, 64, -1, 0.8f, 0.f);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE(tp.activeVoicesMatching(
                [](const CTP::Voice &v)
                {
                    return v.key() == 64 && v.receivedFromPlayingVoice &&
                           v.receivedContData == 6200;
                }) == 1);

    // key 48: steals key 64, receives contData=6400
    vm.processNoteOnEvent(0, 0, 48, -1, 0.8f, 0.f);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE(tp.activeVoicesMatching(
                [](const CTP::Voice &v)
                {
                    return v.key() == 48 && v.receivedFromPlayingVoice &&
                           v.receivedContData == 6400;
                }) == 1);
}

TEST_CASE("Continuation Data - Legato Move Does Not Start A New Voice")
{
    INFO("In MONO_LEGATO, a new keypress moves the existing voice via moveVoice rather than "
         "creating a new one. No VoiceInitInstructionsEntry is issued for the moved voice, so "
         "its receivedFromPlayingVoice and receivedContData are unchanged from creation.");

    auto tp = CTP();
    auto &vm = tp.voiceManager;
    setNaturalLegato(vm);

    // key 60: fresh, no prior data
    vm.processNoteOnEvent(0, 0, 60, -1, 0.8f, 0.f);
    REQUIRE_VOICE_COUNTS(1, 1);
    REQUIRE(tp.activeVoicesMatching([](const CTP::Voice &v)
                                    { return v.key() == 60 && !v.receivedFromPlayingVoice; }) == 1);

    // key 62: triggers moveVoice on the existing voice, NOT a new initializeMultipleVoices call
    vm.processNoteOnEvent(0, 0, 62, -1, 0.8f, 0.f);
    REQUIRE_VOICE_COUNTS(1, 1);

    // Still one voice; it moved to key 62 but was never re-initialised with continuation data
    REQUIRE(tp.activeVoicesMatching([](const CTP::Voice &v)
                                    { return v.key() == 62 && !v.receivedFromPlayingVoice; }) == 1);
}

TEST_CASE("Continuation Data - Release To No Held Key Releases Cleanly")
{
    INFO("When a mono voice is released with no other keys held, it decays normally and no "
         "continuation-data retrigger is issued");

    auto tp = CTP();
    auto &vm = tp.voiceManager;
    setNaturalMono(vm);

    vm.processNoteOnEvent(0, 0, 60, -1, 0.8f, 0.f);
    REQUIRE_VOICE_COUNTS(1, 1);

    vm.processNoteOffEvent(0, 0, 60, -1, 0.0f);

    // Voice releases (not terminates) since no other key was held
    REQUIRE_VOICE_COUNTS(1, 0);
    tp.processFor(5);
    REQUIRE_NO_VOICES;
}

TEST_CASE("Continuation Data - Poly Mode Does Not Use Continuation Data")
{
    INFO("In standard POLY_VOICES mode, mono-stealing never runs, so fromPlayingVoice is always "
         "false and continuationData is always the zero-initialised default");

    auto tp = CTP();
    auto &vm = tp.voiceManager;
    // default is POLY_VOICES, no setPlaymode needed

    vm.processNoteOnEvent(0, 0, 60, -1, 0.8f, 0.f);
    vm.processNoteOnEvent(0, 0, 62, -1, 0.8f, 0.f);
    REQUIRE_VOICE_COUNTS(2, 2);

    REQUIRE(tp.activeVoicesMatching(
                [](const CTP::Voice &v)
                { return !v.receivedFromPlayingVoice && v.receivedContData == 0; }) == 2);
}
