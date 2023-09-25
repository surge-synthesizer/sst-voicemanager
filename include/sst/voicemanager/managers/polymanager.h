//
// Created by Paul Walker on 9/24/23.
//

#ifndef SST_VOICEMANAGER_POLYMANAGER_H
#define SST_VOICEMANAGER_POLYMANAGER_H

#include <array>
#include <cstdint>

namespace sst::voicemanager::managers
{
template<typename Cfg, typename Responder>
struct PolyManager
{
    struct VoiceInfo
    {
        uint16_t port{0}, channel{0}, key{0};
        int32_t noteId{-1};

        bool gated{false};

        typename Cfg::voice_t *activeVoiceCookie{nullptr};
    };
    std::array<VoiceInfo, Cfg::maxVoiceCount> voiceInfo;

    Responder &responder;
    PolyManager(Responder &r) : responder(r) {}

    void registerVoiceEndCallback()
    {
        responder.setVoiceEndCallback([this](typename Cfg::voice_t *t) {
                                          endVoice(t);
        });
    }

    /*
     * Are we poly on all channels or on a single channel
     */
    enum ChannelMode
    {
        OMNI,
        SINGLE
    } channelMode{OMNI};
    uint16_t filterChannel{0};

    void setChannelMode(ChannelMode c, int16_t ch = -1)
    {
        channelMode = c;
        if (channelMode == OMNI)
        {
            filterChannel = (uint16_t) ch;
        }
    }

    /*
     * If a key is struck twice while still gated or sustained, do we start a new voice
     * or do we re-use the voice (and move the note id)
     */
    enum RepeatedKeyMode
    {
        MULTI_VOICE,
        PIANO
    } repeatedKeyMode{MULTI_VOICE};

    void setRepeatedKeyMode(RepeatedKeyMode k) { repeatedKeyMode = k; }

    bool processNoteOnEvent(uint16_t port,
                            uint16_t channel,
                            uint16_t key,
                            int32_t noteid,
                            float velocity,
                            float retune)
    {
        if (channelMode == SINGLE && filterChannel != channel)
        {
            return false;
        }

        if (repeatedKeyMode == PIANO)
        {
            // First look for a matching PCK
            for (auto &vi : voiceInfo)
            {
                if (vi.activeVoiceCookie && vi.port == port && vi.channel == channel
                    && vi.key == key)
                {
                    responder.retriggerVoiceWithNewNoteID(vi.activeVoiceCookie, noteid, velocity);
                    return true;
                }
            }
        }

        for (auto &vi : voiceInfo)
        {
            if (!vi.activeVoiceCookie)
            {
                vi.port = port;
                vi.channel = channel;
                vi.key = key;
                vi.noteId = noteid;

                vi.gated = true;
                vi.activeVoiceCookie = responder.initializeVoice(port, channel, key, noteid, velocity, retune);
                if (lastPBByChannel[channel] != 0)
                {
                    responder.setVoiceMIDIPitchBend(vi.activeVoiceCookie, lastPBByChannel[channel] + 8192);
                }
                return true;
            }
        }

        // TODO: Stealing

        return false;
    }

    void processNoteOffEvent(uint16_t port,
                             uint16_t channel,
                             uint16_t key,
                             int32_t noteid,
                             float velocity)
    {
        for (auto &vi : voiceInfo)
        {
            if (vi.port == port && vi.channel == channel && vi.key == key
                && (vi.noteId == noteid || noteid == -1))
            {
                responder.releaseVoice(vi.activeVoiceCookie, velocity);
                vi.gated = false;
            }
        }
    }
    size_t getVoiceCount() {
        size_t res{0};
        for (const auto &vi : voiceInfo)
        {
            res += (vi.activeVoiceCookie != nullptr);
        }
        return res;
    }
    size_t getGatedVoiceCount() {
        size_t res{0};
        for (const auto &vi : voiceInfo)
        {
            res += (vi.gated);
        }
        return res;
    }

    void endVoice(typename Cfg::voice_t *v)
    {
        for (auto &vi : voiceInfo)
        {
            if (vi.activeVoiceCookie == v)
            {
                vi.activeVoiceCookie = nullptr;
            }
        }
    }

    std::array<uint16_t, 16> lastPBByChannel;
    void routeMIDIPitchBend(uint16_t port, uint16_t channel, uint16_t pb14bit)
    {
        lastPBByChannel[channel] = pb14bit - 8192;
        for (auto &vi : voiceInfo)
        {
            if (vi.port == port && vi.channel == channel && vi.activeVoiceCookie)
            {
                responder.setVoiceMIDIPitchBend(vi.activeVoiceCookie, pb14bit);
            }
        }
    }
};
}

#endif // SST_VOICEMANAGER_POLYMANAGER_H
