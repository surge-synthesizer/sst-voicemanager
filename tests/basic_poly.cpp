/*
 * sst-voicemanager - a header only library providing synth
 * voice management in response to midi and clap event streams
 * with support for a variety of play, trigger, and midi nodes
 *
 * Copyright 2023, various authors, as described in the GitHub
 * transaction log.
 *
 * sst-voicemanager is released under the MIT license, available
 * as LICENSE.md in the root of this repository.
 *
 * All source in sst-voicemanager available at
 * https://github.com/surge-synthesizer/sst-voicemanager
 */

#include <functional>
#include "catch2.hpp"

#include "sst/voicemanager/voicemanager.h"
#include <set>

// voice manager config
struct Config
{
    using voice_t = void;
    static constexpr size_t maxVoiceCount{32};
};

// voice manager responder
struct ConcreteResp
{
    // API
    void setVoiceEndCallback(std::function<void(void *)> f) { innards.voiceEndCallback = f; }

    void stopAllVoices() {}
    void *initializeVoice(uint16_t port, uint16_t channel, uint16_t key, int32_t noteId,
                          float velocity, float retune)
    {
        return innards.newVoice(port, channel, key, noteId, velocity, retune);
    }

    int32_t initializeMultipleVoices(
        std::array<typename Config::voice_t *, Config::maxVoiceCount> &voiceInitWorkingBuffer,
        uint16_t port, uint16_t channel, uint16_t key, int32_t noteId, float velocity, float retune)
    {
        auto voice = innards.newVoice(port, channel, key, noteId, velocity, retune);
        for (auto &v : voiceInitWorkingBuffer)
        {
            if (!v)
            {
                v = voice;
                break;
            }
        }
        return 1;
    }

    void releaseVoice(void *v, float velocity) { innards.release(v, velocity); }
    void retriggerVoiceWithNewNoteID(void *v, int32_t noteid, float velocity) {}

    int beginVoiceCreationTransaction(uint16_t port, uint16_t channel, uint16_t key, int32_t noteid,
                                      float velocity)
    {
        return 1;
    }
    void endVoiceCreationTransaction(uint16_t port, uint16_t channel, uint16_t key, int32_t noteid,
                                     float velocity)
    {
    }

    // Innards
    struct Innards
    {
        std::function<void(void *)> voiceEndCallback;

        struct VoiceImpl
        {
            bool playing{false};
            bool released{false};
            int deadCount = 0;
        };

        std::set<VoiceImpl *> voices;

        void *newVoice(uint16_t port, uint16_t channel, uint16_t key, int32_t noteId,
                       float velocity, float retune)
        {
            auto v = new VoiceImpl();
            v->playing = true;
            voices.insert(v);
            return v;
        }

        void release(void *v, float fel)
        {
            auto vi = static_cast<VoiceImpl *>(v);
            vi->released = true;
            vi->deadCount = 5;
        }
        void process()
        {
            auto vit = voices.begin();
            while (vit != voices.end())
            {
                auto *vi = *vit;

                if (vi->released)
                {
                    vi->deadCount--;
                }
                if (vi->released && vi->deadCount == 0)
                {
                    voiceEndCallback(vi);
                    vi->playing = false;
                    vit = voices.erase(vit);
                }
                else
                {
                    vit++;
                }
            }
        }
    } innards;
};

struct ConcreteMonoResp
{
    void setMIDIPitchBend(int16_t channel, int16_t pb14bit) {}
    void setMIDI1CC(int16_t channel, int16_t cc, int16_t val) {}
    void setMIDIChannelPressure(int16_t channel, int16_t pres) {}
};

using voicemanager_t = sst::voicemanager::VoiceManager<Config, ConcreteResp, ConcreteMonoResp>;

TEST_CASE("Basic Poly Note On Note Off")
{
    ConcreteResp concreteResp;
    ConcreteMonoResp concreteMonoResp;

    SECTION("OnOff")
    {
        // voice manager: consumes config
        voicemanager_t vm(concreteResp, concreteMonoResp);

        // Send a midi message note on, see voice and gated voice tick up
        uint16_t port{0}, channel{0}, key{60}, velocity{90};
        int32_t noteid{-1};
        float retune{0.f};
        vm.processNoteOnEvent(port, channel, key, noteid, vm.midiToFloatVelocity(velocity), retune);
        REQUIRE(vm.getVoiceCount() == 1);
        REQUIRE(vm.getGatedVoiceCount() == 1);

        // process to voice end, see message come back and voice drop and gated voice stay down
        for (auto i = 0U; i < 10; ++i)
        {
            concreteResp.innards.process();
        }

        // send a midi message note off, see voice stay constant but gated voice drop down
        vm.processNoteOffEvent(port, channel, key, noteid, velocity);
        REQUIRE(vm.getVoiceCount() == 1);
        REQUIRE(vm.getGatedVoiceCount() == 0);

        // process to voice end, see message come back and voice drop and gated voice stay down
        for (auto i = 0U; i < 10; ++i)
        {
            concreteResp.innards.process();
        }

        REQUIRE(vm.getVoiceCount() == 0);
        REQUIRE(vm.getGatedVoiceCount() == 0);
    }
}