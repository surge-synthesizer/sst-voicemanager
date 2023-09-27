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

#ifndef INCLUDE_SST_VOICEMANAGER_MANAGERS_POLYMANAGER_H
#define INCLUDE_SST_VOICEMANAGER_MANAGERS_POLYMANAGER_H

#include <array>
#include <cstdint>

namespace sst::voicemanager::managers
{
template <typename Cfg, typename Responder> struct PolyManager
{
    struct VoiceInfo
    {
        int16_t port{0}, channel{0}, key{0};
        int32_t noteId{-1};

        bool gated{false};

        typename Cfg::voice_t *activeVoiceCookie{nullptr};

        bool matches(int16_t pt, int16_t ch, int16_t k, int32_t nid)
        {
            auto res = (activeVoiceCookie != nullptr);
            res = res && (pt == -1 || port == -1 || pt == port);
            res = res && (ch == -1 || channel == -1 || ch == channel);
            res = res && (k == -1 || key == -1 || k == key);
            res = res && (nid == -1 || noteId == -1 || nid == noteId);
            return res;
        }
    };
    std::array<VoiceInfo, Cfg::maxVoiceCount> voiceInfo;

    Responder &responder;
    PolyManager(Responder &r) : responder(r) {}

    size_t polyLimit;
    void setPolyLimit(size_t pl)
    {
        assert(polyLimit > 0);
        polyLimit = pl;
        scanForPolyLimit();
    }

    void registerVoiceEndCallback()
    {
        responder.setVoiceEndCallback([this](typename Cfg::voice_t *t) { endVoice(t); });
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
            filterChannel = (uint16_t)ch;
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

    bool processNoteOnEvent(int16_t port, int16_t channel, int16_t key, int32_t noteid,
                            float velocity, float retune)
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
                if (vi.matches(port, channel, key, -1)) // dont match noteid
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
                vi.activeVoiceCookie =
                    responder.initializeVoice(port, channel, key, noteid, velocity, retune);
                if (lastPBByChannel[channel] != 0)
                {
                    responder.setVoiceMIDIPitchBend(vi.activeVoiceCookie,
                                                    lastPBByChannel[channel] + 8192);
                }
                return true;
            }
        }

        // TODO: Stealing

        return false;
    }

    void processNoteOffEvent(int16_t port, int16_t channel, int16_t key, int32_t noteid,
                             float velocity)
    {
        for (auto &vi : voiceInfo)
        {
            if (vi.matches(port, channel, key, noteid))
            {
                responder.releaseVoice(vi.activeVoiceCookie, velocity);
                vi.gated = false;
            }
        }
    }
    size_t getVoiceCount()
    {
        size_t res{0};
        for (const auto &vi : voiceInfo)
        {
            res += (vi.activeVoiceCookie != nullptr);
        }
        return res;
    }
    size_t getGatedVoiceCount()
    {
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
    void routeMIDIPitchBend(int16_t port, int16_t channel, uint16_t pb14bit)
    {
        lastPBByChannel[channel] = pb14bit - 8192;
        for (auto &vi : voiceInfo)
        {
            if (vi.matches(port, channel, -1, -1)) // all keys and notes on a channel for midi PB
            {
                responder.setVoiceMIDIPitchBend(vi.activeVoiceCookie, pb14bit);
            }
        }
    }

    void scanForPolyLimit() {}

    void routeNoteExpression(int16_t port, int16_t channel, int16_t key, int32_t noteid,
                             int32_t expression, double value)
    {
        for (auto &vi : voiceInfo)
        {
            if (vi.matches(port, channel, key,
                           noteid)) // all keys and notes on a channel for midi PB
            {
                responder.setNoteExpression(vi.activeVoiceCookie, expression, value);
            }
        }
    }

    void routePolyphonicParameterModulation(int16_t port, int16_t channel, int16_t key,
                                            int32_t noteid, uint32_t parameter, double value)
    {
        for (auto &vi : voiceInfo)
        {
            if (vi.matches(port, channel, key,
                           noteid)) // all keys and notes on a channel for midi PB
            {
                responder.setVoicePolyphonicParameterModulation(vi.activeVoiceCookie, parameter,
                                                                value);
            }
        }
    }
};
} // namespace sst::voicemanager::managers

#endif // SST_VOICEMANAGER_POLYMANAGER_H
