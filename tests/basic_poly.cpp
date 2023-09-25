//
// Created by Paul Walker on 9/24/23.
//

#include <functional>
#include "catch2.hpp"

#include "sst/voicemanager/voicemanager.h"
#include <set>


// voice manager config
struct Config
{
    static constexpr size_t maxVoiceCount{32};
};

// voice manager responder
struct ConcreteResp
{
    // API
    void setVoiceEndCallback(std::function<void(void*)> f) {
        innards.voiceEndCallback = f;
    }

    void stopAllVoices() {}
    void* initializeVoice(uint16_t port, uint16_t channel, uint16_t key, int32_t noteId,
                         float velocity, float retune) {
        return innards.newVoice(port, channel, key, noteId, velocity, retune);
    }
    void releaseVoice(void *v, float velocity) {
        innards.release(v, velocity);
    }
    void retriggerVoiceWithNewNoteID(void *v, int32_t noteid, float velocity) {
    }

    // Innards
    struct Innards
    {
        std::function<void(void *)> voiceEndCallback;

        struct VoiceImpl {
            bool playing{false};
            bool released{false};
            int deadCount = 0;
        };

        std::set<VoiceImpl *> voices;

        void* newVoice(uint16_t port, uint16_t channel, uint16_t key, int32_t noteId,
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
        void process() {
            auto vit = voices.begin();
            while (vit !=voices.end())
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
                    vit ++;
                }
            }
        }
    } innards;
};

using voicemanager_t = sst::voicemanager::VoiceManager<Config, ConcreteResp>;

TEST_CASE("Basic Poly Note On Note Off")
{
    ConcreteResp concreteResp;

    SECTION("OnOff")
    {
        // voice manager: consumes config
        voicemanager_t vm(concreteResp);

        // Send a midi message note on, see voice and gated voice tick up
        uint16_t port{0}, channel{0}, key{60}, velocity{90};
        int32_t noteid{-1};
        float retune{0.f};
        vm.processNoteOnEvent(port, channel, key, noteid, vm.midiToFloatVelocity(velocity), retune);
        REQUIRE(vm.getVoiceCount() == 1);
        REQUIRE(vm.getGatedVoiceCount() == 1);

        // process to voice end, see message come back and voice drop and gated voice stay down
        for (auto i=0U; i<10; ++i)
        {
            concreteResp.innards.process();
        }

        // send a midi message note off, see voice stay constant but gated voice drop down
        vm.processNoteOffEvent(port, channel, key, noteid, velocity);
        REQUIRE(vm.getVoiceCount() == 1);
        REQUIRE(vm.getGatedVoiceCount() == 0);

        // process to voice end, see message come back and voice drop and gated voice stay down
        for (auto i=0U; i<10; ++i)
        {
            concreteResp.innards.process();
        }

        REQUIRE(vm.getVoiceCount() == 0);
        REQUIRE(vm.getGatedVoiceCount() == 0);
    }
}