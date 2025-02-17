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

#ifndef SST_VOICEMANAGER_TESTS_TEST_PLAYER_H
#define SST_VOICEMANAGER_TESTS_TEST_PLAYER_H

#include <iostream>
#include <iomanip>
#include <cstdint>
#include <functional>
#include <set>
#include <array>
#include <vector>
#include <tuple>
#include <sstream>
#include <map>

#include "sst/voicemanager/voicemanager.h"
#include <algorithm>

#define TPT(...)                                                                                   \
    if constexpr (doLog)                                                                           \
    {                                                                                              \
        std::cout << "tests/test_player.h:" << __LINE__ << " " << __VA_ARGS__ << std::endl;        \
    }
#define TPD(x) #x << "=" << (x) << " "
#define TPF TPT(__func__)
#define TPUN TPT(__func__ << " - unimplemented");

/**
 * The TestPlayer class provides an implementation of a voice manager target wihch has an internal
 * voice array that it 'plays' by just updating state and providing debug apis. This allows us to
 * use it to write regtests of various features with lots of input and output set.
 *
 * This test player has the feature that it makes one voice for a note in range 0..72 and 3 voices
 * per note over 72. A released voice fades over 5 calls to process then terminates
 */
template <size_t voiceCount, bool doLog = false> struct TestPlayer
{
    using pckn_t = std::tuple<int16_t, int16_t, int16_t, int32_t>;
    int32_t lastCreationCount{1};

    struct Voice
    {
        enum State
        {
            UNUSED,
            ACTIVE
        } state{UNUSED};
        int32_t runtime{0};
        int32_t creationCount{1};
        bool isGated{false};
        int32_t releaseCountdown{0};

        float velocity, releaseVelocity;

        pckn_t pckn{-1, -1, -1, -1};
        pckn_t original_pckn{-1, -1, -1, -1};

        int8_t polyATValue{0};

        auto port() const { return std::get<0>(pckn); }
        auto channel() const { return std::get<1>(pckn); }
        auto key() const { return std::get<2>(pckn); }
        auto noteid() const { return std::get<3>(pckn); }

        auto originalKey() const { return std::get<2>(original_pckn); }

        // Note these make the voices non-fixed size so in a real synth you wouldn't want this
        std::map<int32_t, double> noteExpressionCache;
        std::map<int32_t, double> paramModulationCache;

        int16_t mpeBend{0};
        int8_t mpePressure{0}, mpeTimbre{0};
    };

    struct Config
    {
        using voice_t = Voice;
        static constexpr size_t maxVoiceCount{voiceCount};
    };

    std::array<Voice, voiceCount> voiceStorage;
    std::function<void(Voice *)> voiceEndCallback{nullptr};

    // voice manager responder
    struct Responder
    {
        TestPlayer &testPlayer;
        Responder(TestPlayer &p) : testPlayer(p) {}

        void setVoiceEndCallback(std::function<void(Voice *)> f)
        {
            testPlayer.voiceEndCallback = f;
        }

        int32_t initializeMultipleVoices(
            int32_t voices,
            const typename sst::voicemanager::VoiceInitInstructionsEntry<Config>::buffer_t
                &voiceInitInstructionBuffer,
            typename sst::voicemanager::VoiceInitBufferEntry<Config>::buffer_t
                &voiceInitWorkingBuffer,
            uint16_t port, uint16_t channel, uint16_t key, int32_t noteId, float velocity,
            float retune)
        {
            return testPlayer.initFn(voices, voiceInitInstructionBuffer, voiceInitWorkingBuffer,
                                     port, channel, key, noteId, velocity, retune);
        }

        void moveVoice(typename Config::voice_t *voice, uint16_t port, uint16_t channel,
                       uint16_t key, float velocity)
        {
            TPT(__func__ << " " << TPD(testPlayer.pcknToString(voice->pckn)));
            voice->pckn = {port, channel, key, std::get<3>(voice->original_pckn)};
        }

        void moveAndRetriggerVoice(typename Config::voice_t *voice, uint16_t port, uint16_t channel,
                                   uint16_t key, float velocity)
        {
            TPT(__func__ << " " << TPD(testPlayer.pcknToString(voice->pckn)));
            assert(!voice->isGated);
            voice->pckn = {port, channel, key, std::get<3>(voice->original_pckn)};
            voice->isGated = true;
            voice->releaseCountdown = 0;
            voice->velocity = velocity;
        }

        void terminateVoice(Voice *v)
        {
            TPT("Terminate voice at " << TPD(testPlayer.pcknToString(v->pckn)));
            testPlayer.voiceEndCallback(v);
            *v = Voice();
        }
        void releaseVoice(Voice *v, float velocity)
        {
            TPT("Release voice at " << TPD(testPlayer.pcknToString(v->pckn)));
            v->isGated = false;
            v->releaseCountdown = 5;
            v->releaseVelocity = velocity;
        }
        void retriggerVoiceWithNewNoteID(Voice *v, int32_t noteid, float velocity)
        {
            TPF;
            v->isGated = true;
            v->releaseCountdown = 0;
            v->velocity = velocity;
            std::get<3>(v->pckn) = noteid;
        }

        int beginVoiceCreationTransaction(
            typename sst::voicemanager::VoiceBeginBufferEntry<Config>::buffer_t &buf, uint16_t port,
            uint16_t channel, uint16_t key, int32_t noteid, float velocity)
        {
            TPF;
            return testPlayer.beginFn(buf, port, channel, key, noteid, velocity);
        }
        void endVoiceCreationTransaction(uint16_t port, uint16_t channel, uint16_t key,
                                         int32_t noteid, float velocity)
        {
            TPF;
        }

        void setNoteExpression(Voice *v, int32_t e, double val)
        {
            TPF;
            v->noteExpressionCache[e] = val;
        }
        void setVoicePolyphonicParameterModulation(Voice *v, uint32_t e, double val)
        {
            TPF;
            v->paramModulationCache[e] = val;
        }
        void setPolyphonicAftertouch(Voice *v, int8_t val)
        {
            TPF;
            v->polyATValue = val;
        }
        void setVoiceMIDIMPEChannelPitchBend(Voice *v, uint16_t b)
        {
            TPF;
            v->mpeBend = b;
        }
        void setVoiceMIDIMPEChannelPressure(Voice *v, int8_t p)
        {
            TPF;
            v->mpePressure = p;
        }
        void setVoiceMIDIMPETimbre(Voice *v, int8_t t)
        {
            TPF;
            v->mpeTimbre = t;
        }

    } responder;

    /*
     * Make these virtual for test purposes. You probably wouldn't do this in the wild
     * and virutal and templates together suck and stuff etc
     */
    virtual int beginFn(typename sst::voicemanager::VoiceBeginBufferEntry<Config>::buffer_t &buf,
                        uint16_t port, uint16_t channel, uint16_t key, int32_t noteid,
                        float velocity)
    {
        auto res{1};
        if (key > 72)
            res = 3;

        for (int i = 0; i < res; ++i)
        {
            if (polyGroupForKey)
            {
                buf[i].polyphonyGroup = polyGroupForKey(key);
                TPT(TPD(i) << TPD(buf[i].polyphonyGroup));
            }
            else
                buf[i].polyphonyGroup = 0;
        }
        return res;
    }

    virtual int32_t initFn(
        int32_t voices,
        const typename sst::voicemanager::VoiceInitInstructionsEntry<Config>::buffer_t
            &voiceInitInstructionBuffer,
        typename sst::voicemanager::VoiceInitBufferEntry<Config>::buffer_t &voiceInitWorkingBuffer,
        uint16_t port, uint16_t channel, uint16_t key, int32_t noteId, float velocity, float retune)
    {
        TPF;
        Voice *nv[3]{nullptr, nullptr, nullptr};
        int idx{0};
        int midx{key <= 72 ? 1 : 3};
        assert(voices == midx);

        for (auto &v : voiceStorage)
        {
            if (v.state != Voice::ACTIVE)
            {
                nv[idx] = &v;
                TPT("Assigning voice at " << idx << " from " << &v);
                ++idx;
                if (idx == midx)
                    break;
            }
        }

        if (idx != midx)
            return 0;

        for (int i = 0; i < midx; ++i)
        {
            auto v = nv[i];
            if (!v)
                continue;
            if (voiceInitInstructionBuffer[i].instruction ==
                sst::voicemanager::VoiceInitInstructionsEntry<Config>::Instruction::SKIP)
            {
                voiceInitWorkingBuffer[i].voice = nullptr;
                continue;
            }
            v->state = Voice::ACTIVE;
            v->runtime = 0;
            v->isGated = true;
            v->pckn = {port, channel, key, noteId};
            v->original_pckn = v->pckn;
            v->velocity = velocity;
            v->creationCount = lastCreationCount++;
            TPT("Last Creation Count is now " << lastCreationCount);

            voiceInitWorkingBuffer[i].voice = v;
            TPT("  Set voice at " << i << " voices " << pcknToString(v->pckn));
        }

        TPT("Created " << midx << " voices ");
        return midx;
    }

    struct MonoResponder
    {
        TestPlayer &testPlayer;
        MonoResponder(TestPlayer &p) : testPlayer(p) {}
        void setMIDIPitchBend(int16_t channel, int16_t pb14bit)
        {
            testPlayer.pitchBend[std::clamp(channel, (int16_t)0, (int16_t)15)] = pb14bit;
        }
        void setMIDI1CC(int16_t channel, int16_t cc, int8_t val)
        {
            assert(channel >= 0 && channel < 16);
            testPlayer.midi1CC[std::clamp(channel, (int16_t)0, (int16_t)15)][cc] = val;
        }
        void setMIDIChannelPressure(int16_t channel, int16_t pres)
        {
            assert(channel >= 0 && channel < 16);
            testPlayer.channelPressure[std::clamp(channel, (int16_t)0, (int16_t)15)] = pres;
        }
    } monoResponder;

    std::array<int16_t, 16> channelPressure{}, pitchBend{};
    std::array<std::array<int8_t, 128>, 16> midi1CC{};

    using voiceManager_t = sst::voicemanager::VoiceManager<Config, Responder, MonoResponder>;
    voiceManager_t voiceManager;

    void processFor(size_t times)
    {
        for (auto i = 0U; i < times; ++i)
            process();
    }
    void process()
    {
        for (auto &v : voiceStorage)
        {
            if (v.state == Voice::State::ACTIVE)
            {
                ++v.runtime;
                if (!v.isGated)
                {
                    --v.releaseCountdown;
                    if (v.releaseCountdown == 0)
                    {
                        if (voiceEndCallback)
                        {
                            voiceEndCallback(&v);
                        }
                        v.state = Voice::State::UNUSED;
                    }
                }
            }
        }
    }

    TestPlayer() : responder(*this), monoResponder(*this), voiceManager(responder, monoResponder)
    {
        TPT("Constructed TestPlayer with " << TPD(voiceCount));
    }

    /*
     * This test player has a large collection of debug apis to let us probe the internal
     * state and write regtests. These APIs are below, and are not part of what most voice
     * manager users would use
     */

    std::function<uint64_t(int16_t)> polyGroupForKey{nullptr};

    std::vector<pckn_t> getGatedVoicePCKNS() const
    {
        auto res = std::vector<pckn_t>();
        for (auto &v : voiceStorage)
        {
            if (v.state == Voice::ACTIVE && v.isGated)
                res.push_back(v.pckn);
        }
        return res;
    }

    std::vector<pckn_t> getActiveVoicePCKNS() const
    {
        auto res = std::vector<pckn_t>();
        for (auto &v : voiceStorage)
        {
            if (v.state == Voice::ACTIVE)
                res.push_back(v.pckn);
        }
        return res;
    }

    std::string pcknToString(const pckn_t &v)
    {
        std::ostringstream oss;
        auto [p, c, k, n] = v;
        oss << "p=" << p << ",c=" << c << ",k=" << k << ",n=" << n;
        return oss.str();
    }
    std::string voiceToString(const Voice &v)
    {
        std::ostringstream oss;
        oss << "Voice[";
        if (v.state == Voice::UNUSED)
        {
            oss << "Unused";
        }
        else
        {
            oss << "ptr=" << &v << ",rt=" << v.runtime << ",gate=" << v.isGated
                << ",rc=" << v.releaseCountdown << "," << pcknToString(v.pckn)
                << ",mpeBend=" << v.mpeBend << ",mpePres=" << (int)v.mpePressure
                << ",mpeTim=" << (int)v.mpeTimbre;
        }
        oss << "]";
        return oss.str();
    }

    void dumpAllVoices(bool includeUnused = false)
    {
        TPT("Dump all voices " << (includeUnused ? " including unused" : ""));
        for (const auto &v : voiceStorage)
            if (includeUnused || v.state == Voice::ACTIVE)
                TPT(voiceToString(v));
        TPT("Voice dump complete");
    }

    bool hasKeysActive(const std::set<int16_t> &keySet)
    {
        for (auto &k : keySet)
        {
            auto found = false;
            for (auto &v : voiceStorage)
            {
                found = found || (v.state == Voice::ACTIVE && std::get<2>(v.pckn) == k);
            }
            if (!found)
                return false;
        }
        return true;
    }

    int32_t activeVoicesMatching(std::function<bool(const Voice &)> cond)
    {
        int32_t res = 0;
        for (const auto &v : voiceStorage)
        {
            if (v.state == Voice::ACTIVE && cond(v))
            {
                ++res;
            }
        }
        return res;
    }

    bool activeVoiceCheck(std::function<bool(const Voice &)> voiceFilter,
                          std::function<bool(const Voice &)> condition)
    {
        auto res = true;
        auto checkedAny = false;
        for (const auto &v : voiceStorage)
        {
            if (v.state == Voice::ACTIVE && voiceFilter(v))
            {
                checkedAny = true;
                res = res & condition(v);
            }
        }
        return res && checkedAny;
    }
};

template <size_t voiceCount, bool doLog = false>
struct TwoGroupsEveryKey : TestPlayer<voiceCount, doLog>
{
    TwoGroupsEveryKey()
    {
        this->voiceManager.guaranteeGroup(2112);
        this->voiceManager.guaranteeGroup(90125);
    }
    int beginFn(typename sst::voicemanager::VoiceBeginBufferEntry<
                    typename TestPlayer<voiceCount, doLog>::Config>::buffer_t &buf,
                uint16_t port, uint16_t channel, uint16_t key, int32_t noteid,
                float velocity) override
    {
        buf[0].polyphonyGroup = 2112;
        buf[1].polyphonyGroup = 90125;
        return 2;
    }
    int32_t initFn(
        int voices,
        const typename sst::voicemanager::VoiceInitInstructionsEntry<
            typename TestPlayer<voiceCount, doLog>::Config>::buffer_t &voiceInitInstructionBuffer,
        typename sst::voicemanager::VoiceInitBufferEntry<
            typename TestPlayer<voiceCount, doLog>::Config>::buffer_t &voiceInitWorkingBuffer,
        uint16_t port, uint16_t channel, uint16_t key, int32_t noteId, float velocity,
        float retune) override
    {
        assert(voices == 2);
        typename TestPlayer<voiceCount, doLog>::Voice *nv[2]{nullptr, nullptr};

        int idx = 0;
        for (auto &v : this->voiceStorage)
        {
            if (v.state != TestPlayer<voiceCount, doLog>::Voice::ACTIVE)
            {
                nv[idx] = &v;
                TPT("Assigning voice at " << idx << " from " << &v);
                ++idx;
                if (idx == 2)
                    break;
            }
        }

        idx = 0;
        for (int i = 0; i < voices; ++i)
        {
            if (voiceInitInstructionBuffer[i].instruction ==
                TestPlayer<voiceCount, doLog>::voiceManager_t::initInstruction_t::Instruction::SKIP)
            {
                TPT("Skipping voice at " << i);
                continue;
            }
            auto v = nv[idx];
            idx++;
            if (!v)
                continue;
            v->state = TestPlayer<voiceCount, doLog>::Voice::ACTIVE;
            v->runtime = 0;
            v->isGated = true;
            v->pckn = {port, channel, key, noteId};
            v->original_pckn = v->pckn;
            v->velocity = velocity;
            v->creationCount = this->lastCreationCount++;

            voiceInitWorkingBuffer[i].voice = v;
            TPT("  Set voice at " << i << " voices " << this->pcknToString(v->pckn));
        }

        return idx;
    }
};

template <size_t voiceCount, bool doLog = false>
struct ThreeGroupsEveryKey : TestPlayer<voiceCount, doLog>
{
    ThreeGroupsEveryKey()
    {
        this->voiceManager.guaranteeGroup(2112);
        this->voiceManager.guaranteeGroup(90125);
        this->voiceManager.guaranteeGroup(8675309);
    }
    int beginFn(typename sst::voicemanager::VoiceBeginBufferEntry<
                    typename TestPlayer<voiceCount, doLog>::Config>::buffer_t &buf,
                uint16_t port, uint16_t channel, uint16_t key, int32_t noteid,
                float velocity) override
    {
        buf[0].polyphonyGroup = 2112;
        buf[1].polyphonyGroup = 90125;
        buf[2].polyphonyGroup = 8675309;
        return 3;
    }
    int32_t initFn(
        int voices,
        const typename sst::voicemanager::VoiceInitInstructionsEntry<
            typename TestPlayer<voiceCount, doLog>::Config>::buffer_t &voiceInitInstructionBuffer,
        typename sst::voicemanager::VoiceInitBufferEntry<
            typename TestPlayer<voiceCount, doLog>::Config>::buffer_t &voiceInitWorkingBuffer,
        uint16_t port, uint16_t channel, uint16_t key, int32_t noteId, float velocity,
        float retune) override
    {
        assert(voices == 3);
        typename TestPlayer<voiceCount, doLog>::Voice *nv[3]{nullptr, nullptr, nullptr};

        int idx = 0;
        for (auto &v : this->voiceStorage)
        {
            if (v.state != TestPlayer<voiceCount, doLog>::Voice::ACTIVE)
            {
                nv[idx] = &v;
                TPT("Assigning voice at " << idx << " from " << &v);
                ++idx;
                if (idx == 3)
                    break;
            }
        }

        idx = 0;
        for (int i = 0; i < voices; ++i)
        {
            if (voiceInitInstructionBuffer[i].instruction ==
                TestPlayer<voiceCount, doLog>::voiceManager_t::initInstruction_t::Instruction::SKIP)
                continue;
            auto v = nv[idx];
            idx++;
            if (!v)
                continue;
            v->state = TestPlayer<voiceCount, doLog>::Voice::ACTIVE;
            v->runtime = 0;
            v->isGated = true;
            v->pckn = {port, channel, key, noteId};
            v->original_pckn = v->pckn;
            v->velocity = velocity;
            v->creationCount = this->lastCreationCount++;

            voiceInitWorkingBuffer[i].voice = v;
            TPT("  Set voice at " << i << " voices " << this->pcknToString(v->pckn));
        }

        return idx;
    }
};

#undef TPT
#undef TPF
#undef TPD
#undef TPUN

// Some useful test macros
#define REQUIRE_VOICE_COUNTS(count, gatedCount)                                                    \
    REQUIRE(vm.getVoiceCount() == (size_t)(count));                                                \
    REQUIRE(vm.getGatedVoiceCount() == (size_t)(gatedCount));                                      \
    REQUIRE(tp.getActiveVoicePCKNS().size() == (size_t)(count));                                   \
    REQUIRE(tp.getGatedVoicePCKNS().size() == (size_t)(gatedCount));

#define REQUIRE_NO_VOICES                                                                          \
    REQUIRE(vm.getVoiceCount() == 0);                                                              \
    REQUIRE(vm.getGatedVoiceCount() == 0);                                                         \
    REQUIRE(tp.getActiveVoicePCKNS().empty());                                                     \
    REQUIRE(tp.getGatedVoicePCKNS().empty());

#define REQUIRE_VOICE_MATCH(ct, ...)                                                               \
    REQUIRE(tp.activeVoicesMatching([](auto &v) { return __VA_ARGS__; }) == ct)

#define REQUIRE_VOICE_MATCH_FN(ct, ...) REQUIRE(tp.activeVoicesMatching(__VA_ARGS__) == ct)
#define REQUIRE_KEY_COUNT(ct, keyNumber)                                                           \
    REQUIRE(tp.activeVoicesMatching([](const auto &v) { return v.key() == keyNumber; }) == ct)

#define REQUIRE_INCOMPLETE_TEST REQUIRE(false)
// #define REQUIRE_INCOMPLETE_TEST INFO("This test is currently incomplete");

#endif // TEST_RESPONDERS_H
