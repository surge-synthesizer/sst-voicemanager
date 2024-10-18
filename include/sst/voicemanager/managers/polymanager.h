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
#include <cassert>
#include <iostream>
#include <limits>

namespace sst::voicemanager::managers
{
template <typename Cfg, typename VoiceResponder, typename MonoResponder> struct PolyManager
{
    int64_t mostRecentNoteCounter{1};

    struct VoiceInfo
    {
        int16_t port{0}, channel{0}, key{0};
        int32_t noteId{-1};

        int64_t noteCounter{0};

        bool gated{false};
        bool gatedDueToSustain{false};

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
    uint32_t usedVoices{0};

    std::array<typename Cfg::voice_t *, Cfg::maxVoiceCount> voiceInitWorkingBuffer;

    VoiceResponder &responder;
    MonoResponder &monoResponder;
    PolyManager(VoiceResponder &r, MonoResponder &m) : responder(r), monoResponder(m)
    {
        std::fill(lastPBByChannel.begin(), lastPBByChannel.end(), 0);
    }

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
        if (repeatedKeyMode == PIANO)
        {
            bool didAnyRetrigger{false};
            // First look for a matching PCK
            for (auto &vi : voiceInfo)
            {
                // FIXME this is wrong in the multi-voice partial steal case
                if (vi.matches(port, channel, key, -1)) // dont match noteid
                {
                    responder.retriggerVoiceWithNewNoteID(vi.activeVoiceCookie, noteid, velocity);
                    didAnyRetrigger = true;
                }
            }

            if (didAnyRetrigger)
            {
                return true;
            }
        }

        auto voicesToBeLaunched =
            responder.beginVoiceCreationTransaction(port, channel, key, noteid, velocity);

        if (voicesToBeLaunched == 0)
        {
            responder.endVoiceCreationTransaction(port, channel, key, noteid, velocity);
            return true;
        }

        auto voiceLimit{Cfg::maxVoiceCount};
        if (voicesToBeLaunched + usedVoices > voiceLimit)
        {
            if (voicesToBeLaunched > (int32_t)Cfg::maxVoiceCount)
            {
                // We can't steal enough. So just bail on this edge case for now
                responder.endVoiceCreationTransaction(port, channel, key, noteid, velocity);
                return false;
            }

            auto usedVoicesCopy{usedVoices};
            while (voicesToBeLaunched + usedVoicesCopy > voiceLimit)
            {
                // We really want strategies here but for now just steal
                // oldest non-gated and if thats not there oldest gated
                typename Cfg::voice_t *oldestGated{nullptr}, *oldestNonGated{nullptr};
                int64_t gi{std::numeric_limits<int64_t>::max()}, ngi{gi};
                for (const auto &v : voiceInfo)
                {
                    if (!v.activeVoiceCookie)
                        continue;
                    if (v.gated || v.gatedDueToSustain)
                    {
                        if (v.noteCounter < gi)
                        {
                            gi = v.noteCounter;
                            oldestGated = v.activeVoiceCookie;
                        }
                    }
                    else
                    {
                        if (v.noteCounter < ngi)
                        {
                            ngi = v.noteCounter;
                            oldestNonGated = v.activeVoiceCookie;
                        }
                    }
                }
                if (oldestNonGated)
                {
                    responder.terminateVoice(oldestNonGated);
                }
                else
                {
                    responder.terminateVoice(oldestGated);
                }
                usedVoicesCopy--;
            }
        }

        if (lastPBByChannel[channel] != 0)
        {
            monoResponder.setMIDIPitchBend(channel, lastPBByChannel[channel] + 8192);
        }

        int cid{0};
        for (auto &mcc : midiCCCache[channel])
        {
            if (mcc != 0)
            {
                monoResponder.setMIDI1CC(channel, cid, mcc);
            }
            cid++;
        }

        auto voicesLaunched = responder.initializeMultipleVoices(
            voiceInitWorkingBuffer, port, channel, key, noteid, velocity, retune);

        if (voicesLaunched != voicesToBeLaunched)
        {
            // This is probably OK
        }

        if (voicesLaunched == 0)
        {
            responder.endVoiceCreationTransaction(port, channel, key, noteid, velocity);

            return false;
        }

        auto voicesLeft = voicesLaunched;
        for (auto &vi : voiceInfo)
        {
            if (!vi.activeVoiceCookie)
            {
                usedVoices++;
                vi.noteCounter = mostRecentNoteCounter++;
                vi.port = port;
                vi.channel = channel;
                vi.key = key;
                vi.noteId = noteid;

                vi.gated = true;
                vi.gatedDueToSustain = false;
                vi.activeVoiceCookie = voiceInitWorkingBuffer[voicesLeft - 1];

                voicesLeft--;
                if (voicesLeft == 0)
                {
                    responder.endVoiceCreationTransaction(port, channel, key, noteid, velocity);

                    return true;
                }
            }
        }

        responder.endVoiceCreationTransaction(port, channel, key, noteid, velocity);

        return false;
    }

    void processNoteOffEvent(int16_t port, int16_t channel, int16_t key, int32_t noteid,
                             float velocity)
    {
        for (auto &vi : voiceInfo)
        {
            if (vi.matches(port, channel, key, noteid))
            {
                if (sustainOn)
                {
                    vi.gatedDueToSustain = true;
                }
                else
                {
                    if (vi.gated)
                    {
                        responder.releaseVoice(vi.activeVoiceCookie, velocity);
                        vi.gated = false;
                    }
                }
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
                usedVoices--;
            }
        }
    }

    std::array<int16_t, 16> lastPBByChannel{};
    void routeMIDIPitchBend(int16_t port, int16_t channel, int16_t pb14bit)
    {
        if (channel >= 0 && channel < lastPBByChannel.size())
            lastPBByChannel[channel] = pb14bit - 8192;

        monoResponder.setMIDIPitchBend(channel, pb14bit);
    }

    void routeMIDIMPEChannelPitchBend(int16_t port, int16_t channel, uint16_t pb14bit)
    {
        for (auto &vi : voiceInfo)
        {
            if (vi.matches(port, channel, -1, -1)) // all keys and notes on a channel for midi PB
            {
                responder.setVoiceMIDIMPEChannelPitchBend(vi.activeVoiceCookie, pb14bit);
            }
        }
    }

    bool sustainOn{false};
    void updateSustainPedal(int16_t port, int16_t channel, int8_t level)
    {
        auto sop = sustainOn;
        sustainOn = level > 64;
        if (sop != sustainOn)
        {
            if (!sustainOn)
            {
                // release all voices with sustain gates
                for (auto vi : voiceInfo)
                {
                    if (vi.gatedDueToSustain && vi.matches(port, channel, -1, -1))
                    {
                        responder.releaseVoice(vi.activeVoiceCookie, 0);
                        vi.gated = false;
                        vi.gatedDueToSustain = false;
                    }
                }
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

    std::array<std::array<uint16_t, 128>, 16> midiCCCache{};

    void routeMIDI1CC(int16_t port, int16_t channel, int8_t cc, int8_t val)
    {
        midiCCCache[channel][cc] = val;
        monoResponder.setMIDI1CC(channel, cc, val);
    }

    void routePolyphonicAftertouch(int16_t port, int16_t channel, int16_t key, int8_t pat)
    {
        for (auto &vi : voiceInfo)
        {
            if (vi.matches(port, channel, key, -1)) // all keys and notes on a channel for midi PB
            {
                responder.setPolyphonicAftertouch(vi.activeVoiceCookie, pat);
            }
        }
    }

    void routeChannelPressure(int16_t port, int16_t channel, int8_t pat)
    {
        monoResponder.setMIDIChannelPressure(channel, pat);
    }

    void routeMIDIMPETimbreToVoice(int16_t port, int16_t channel, int8_t val)
    {
        for (auto &vi : voiceInfo)
        {
            if (vi.port == port && vi.channel == channel && vi.gated)
            {
                responder.setVoiceMIDIMPETimbre(vi.activeVoiceCookie, val);
            }
        }
    }

    void routeMIDIMPEChannelPressureToVoice(int16_t port, int16_t channel, int8_t val)
    {
        for (auto &vi : voiceInfo)
        {
            if (vi.port == port && vi.channel == channel && vi.gated)
            {
                responder.setVoiceMIDIMPEChannelPressure(vi.activeVoiceCookie, val);
            }
        }
    }
};
} // namespace sst::voicemanager::managers

#endif // SST_VOICEMANAGER_POLYMANAGER_H
