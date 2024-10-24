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

#ifndef INCLUDE_SST_VOICEMANAGER_VOICEMANAGER_IMPL_H
#define INCLUDE_SST_VOICEMANAGER_VOICEMANAGER_IMPL_H

#include <type_traits>

#include "voicemanager_constraints.h"

#include <iostream>
#include <unordered_map>

namespace sst::voicemanager
{

static constexpr bool vmLog{false};
#define VML(...)                                                                                   \
    {                                                                                              \
        if constexpr (vmLog)                                                                       \
        {                                                                                          \
            std::cout << "include/sst/voicemanager/voicemanager_impl.h:" << __LINE__ << " "        \
                      << __VA_ARGS__ << std::endl;                                                 \
        }                                                                                          \
    }

template <typename Cfg, typename Responder, typename MonoResponder>
struct VoiceManager<Cfg, Responder, MonoResponder>::Details
{
    VoiceManager<Cfg, Responder, MonoResponder> &vm;
    Details(VoiceManager<Cfg, Responder, MonoResponder> &in) : vm(in)
    {
        std::fill(lastPBByChannel.begin(), lastPBByChannel.end(), 0);
        usedVoices[0] = 0;
        polyLimits[0] = Cfg::maxVoiceCount;
        stealingPriorityMode[0] = OLDEST;
    }

    int64_t mostRecentNoteCounter{1};
    int64_t mostRecentTransactionID{1};

    struct VoiceInfo
    {
        int16_t port{0}, channel{0}, key{0};
        int32_t noteId{-1};

        int64_t noteCounter{0}, transactionId{0};

        bool gated{false};
        bool gatedDueToSustain{false};

        uint64_t polyGroup{0};

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
    std::unordered_map<uint64_t, int32_t> polyLimits;
    std::unordered_map<uint64_t, int32_t> usedVoices;
    std::unordered_map<uint64_t, StealingPriorityMode> stealingPriorityMode;
    int32_t totalUsedVoices{0};

    typename VoiceBeginBufferEntry<Cfg>::buffer_t voiceBeginWorkingBuffer;
    typename VoiceInitBufferEntry<Cfg>::buffer_t voiceInitWorkingBuffer;
    std::array<std::array<uint16_t, 128>, 16> midiCCCache{};
    bool sustainOn{false};
    std::array<int16_t, 16> lastPBByChannel{};

    void doMonoPitchBend(int16_t port, int16_t channel, int16_t pb14bit)
    {
        if (channel >= 0 && channel < (int16_t)lastPBByChannel.size())
            lastPBByChannel[channel] = pb14bit - 8192;

        vm.monoResponder.setMIDIPitchBend(channel, pb14bit);
    }

    void doMPEPitchBend(int16_t port, int16_t channel, int16_t pb14bit)
    {
        for (auto &vi : voiceInfo)
        {
            if (vi.matches(port, channel, -1, -1) &&
                vi.gated) // all keys and notes on a channel for midi PB
            {
                vm.responder.setVoiceMIDIMPEChannelPitchBend(vi.activeVoiceCookie, pb14bit);
            }
        }
    }

    void doMonoChannelPressure(int16_t port, int16_t channel, int8_t val)
    {
        vm.monoResponder.setMIDIChannelPressure(channel, val);
    }

    void doMPEChannelPressure(int16_t port, int16_t channel, int8_t val)
    {
        for (auto &vi : voiceInfo)
        {
            if (vi.activeVoiceCookie && vi.port == port && vi.channel == channel && vi.gated)
            {
                vm.responder.setVoiceMIDIMPEChannelPressure(vi.activeVoiceCookie, val);
            }
        }
    }

    void endVoice(typename Cfg::voice_t *v)
    {
        for (auto &vi : voiceInfo)
        {
            if (vi.activeVoiceCookie == v)
            {
                vi.activeVoiceCookie = nullptr;
                --usedVoices.at(vi.polyGroup);
                --totalUsedVoices;
                VML("Ending voice " << vi.activeVoiceCookie << " pg=" << vi.polyGroup
                                    << " used now is " << usedVoices.at(vi.polyGroup) << " ("
                                    << totalUsedVoices << ")");
            }
        }
    }

    int32_t findNextStealableVoiceInfo(uint64_t polygroup, StealingPriorityMode pm,
                                       bool ignorePolygroup = false)
    {
        int32_t oldestGated{-1}, oldestNonGated{-1};
        int64_t gi{std::numeric_limits<int64_t>::max()}, ngi{gi};
        if (pm == HIGHEST)
        {
            gi = std::numeric_limits<int64_t>::min();
            ngi = gi;
        }

        VML("- Finding stealable from " << polygroup << " with ignore " << ignorePolygroup);

        for (int32_t vi = 0; vi < (int32_t)voiceInfo.size(); ++vi)
        {
            const auto &v = voiceInfo[vi];
            if (!v.activeVoiceCookie)
            {
                VML("   - Skipping no-cookie at " << vi);
                continue;
            }
            if (v.polyGroup != polygroup && !ignorePolygroup)
            {
                VML("   - Skipping different group at " << vi);
                continue;
            }

            VML("   - Considering " << vi << " " << v.key << " " << gi << " " << v.noteCounter);
            if (v.gated || v.gatedDueToSustain)
            {
                switch (pm)
                {
                case OLDEST:
                {
                    if (v.noteCounter < gi)
                    {
                        oldestGated = vi;
                        gi = v.noteCounter;
                    }
                }
                break;

                case HIGHEST:
                {
                    if (v.key > gi)
                    {
                        oldestGated = vi;
                        gi = v.key;
                    }
                }
                break;

                case LOWEST:
                {
                    if (v.key < gi)
                    {
                        oldestGated = vi;
                        gi = v.key;
                    }
                }
                break;
                }
            }
            else
            {
                switch (pm)
                {
                case OLDEST:
                {
                    if (v.noteCounter < ngi)
                    {
                        oldestNonGated = vi;
                        ngi = v.noteCounter;
                    }
                }
                break;

                case HIGHEST:
                {
                    if (v.key > ngi)
                    {
                        oldestNonGated = vi;
                        ngi = v.key;
                    }
                }
                break;

                case LOWEST:
                {
                    if (v.key < ngi)
                    {
                        oldestNonGated = vi;
                        ngi = v.key;
                    }
                }
                break;
                }
            }
        }
        if (oldestNonGated >= 0)
        {
            return oldestNonGated;
        }
        if (oldestGated >= 0)
        {
            VML("  - Found " << oldestGated);
            return oldestGated;
        }
        return -1;
    }
};

// ToDo: API Static Asserts

template <typename Cfg, typename Responder, typename MonoResponder>
VoiceManager<Cfg, Responder, MonoResponder>::VoiceManager(Responder &r, MonoResponder &m)
    : responder(r), monoResponder(m), details(*this)
{
    static_assert(constraints::ConstraintsChecker<Cfg, Responder, MonoResponder>::satisfies());
    registerVoiceEndCallback();
}

template <typename Cfg, typename Responder, typename MonoResponder>
void VoiceManager<Cfg, Responder, MonoResponder>::registerVoiceEndCallback()
{
    responder.setVoiceEndCallback([this](typename Cfg::voice_t *t) { details.endVoice(t); });
}

template <typename Cfg, typename Responder, typename MonoResponder>
bool VoiceManager<Cfg, Responder, MonoResponder>::processNoteOnEvent(int16_t port, int16_t channel,
                                                                     int16_t key, int32_t noteid,
                                                                     float velocity, float retune)
{
    if (repeatedKeyMode == PIANO)
    {
        bool didAnyRetrigger{false};
        ++details.mostRecentTransactionID;
        for (auto &vi : details.voiceInfo)
        {
            if (vi.matches(port, channel, key, -1)) // dont match noteid
            {
                responder.retriggerVoiceWithNewNoteID(vi.activeVoiceCookie, noteid, velocity);
                vi.gated = true;
                vi.noteCounter = ++details.mostRecentNoteCounter;
                vi.transactionId = details.mostRecentTransactionID;
                didAnyRetrigger = true;
            }
        }

        if (didAnyRetrigger)
        {
            return true;
        }
    }

    auto voicesToBeLaunched = responder.beginVoiceCreationTransaction(
        details.voiceBeginWorkingBuffer, port, channel, key, noteid, velocity);

    if (voicesToBeLaunched == 0)
    {
        responder.endVoiceCreationTransaction(port, channel, key, noteid, velocity);
        return true;
    }

    std::unordered_map<uint64_t, int32_t> createdByPolyGroup;
    for (int i = 0; i < voicesToBeLaunched; ++i)
    {
        ++createdByPolyGroup[details.voiceBeginWorkingBuffer[i].polyphonyGroup];
    }

    VML("======== LAUNCHING " << voicesToBeLaunched << " @ " << port << "/" << channel << "/" << key
                              << "/" << noteid << " ============");
    for (int i = 0; i < voicesToBeLaunched; ++i)
    {
        const auto &vbb = details.voiceBeginWorkingBuffer[i];
        auto polyGroup = vbb.polyphonyGroup;
        assert(details.polyLimits.find(polyGroup) != details.polyLimits.end());

        VML("Stealing:");
        VML("- Checking polygroup " << polyGroup);
        int32_t voiceLimit{details.polyLimits.at(polyGroup)};
        int32_t voicesUsed{details.usedVoices.at(polyGroup)};
        int32_t groupFreeVoices = std::max(0, voiceLimit - voicesUsed);

        int32_t globalFreeVoices = Cfg::maxVoiceCount - details.totalUsedVoices;
        int32_t voicesFree = std::min(groupFreeVoices, globalFreeVoices);
        VML("- VoicesFree=" << voicesFree << " toBeCreated=" << createdByPolyGroup.at(polyGroup)
                            << " voiceLimit=" << voiceLimit << " voicesUsed=" << voicesUsed
                            << " groupFreeVoices=" << groupFreeVoices
                            << " globalFreeVoices=" << globalFreeVoices);

        auto voicesToSteal = std::max(createdByPolyGroup.at(polyGroup) - voicesFree, 0);
        auto stealFromPolyGroup{polyGroup};

        VML("- Voices to steal is " << voicesToSteal);
        auto lastVoicesToSteal = voicesToSteal + 1;
        while (voicesToSteal > 0 && voicesToSteal != lastVoicesToSteal)
        {
            lastVoicesToSteal = voicesToSteal;
            auto stealVoiceIndex = details.findNextStealableVoiceInfo(
                polyGroup, details.stealingPriorityMode.at(polyGroup),
                groupFreeVoices > 0 && globalFreeVoices == 0);
            VML("- " << voicesToSteal << " from " << stealFromPolyGroup << " stealing voice "
                     << stealVoiceIndex);
            if (stealVoiceIndex >= 0)
            {
                auto &stealVoice = details.voiceInfo[stealVoiceIndex];
                responder.terminateVoice(stealVoice.activeVoiceCookie);
                VML("  - SkipThis found");
                --voicesToSteal;

                /*
                 * This code makes sure if voices were launched from the same
                 * event they are reaped together
                 */
                for (const auto &v : details.voiceInfo)
                {
                    if (v.activeVoiceCookie &&
                        v.activeVoiceCookie != stealVoice.activeVoiceCookie &&
                        v.transactionId == stealVoice.transactionId)
                    {
                        responder.terminateVoice(v.activeVoiceCookie);
                        --voicesToSteal;
                    }
                }
            }
        }
    }

    if (details.lastPBByChannel[channel] != 0)
    {
        monoResponder.setMIDIPitchBend(channel, details.lastPBByChannel[channel] + 8192);
    }

    int cid{0};
    for (auto &mcc : details.midiCCCache[channel])
    {
        if (mcc != 0)
        {
            monoResponder.setMIDI1CC(channel, cid, mcc);
        }
        cid++;
    }

    auto voicesLaunched = responder.initializeMultipleVoices(
        details.voiceInitWorkingBuffer, port, channel, key, noteid, velocity, retune);

    VML("Voices created " << voicesLaunched);

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
    ++details.mostRecentTransactionID;

    for (auto &vi : details.voiceInfo)
    {
        if (!vi.activeVoiceCookie)
        {
            vi.noteCounter = details.mostRecentNoteCounter++;
            vi.transactionId = details.mostRecentTransactionID;
            vi.port = port;
            vi.channel = channel;
            vi.key = key;
            vi.noteId = noteid;

            vi.gated = true;
            vi.gatedDueToSustain = false;
            vi.activeVoiceCookie = details.voiceInitWorkingBuffer[voicesLeft - 1].voice;
            vi.polyGroup = details.voiceBeginWorkingBuffer[voicesLeft - 1].polyphonyGroup;

            VML("New Voice assigned with " << details.mostRecentNoteCounter << " at pckn=" << port
                                           << "/" << channel << "/" << key << "/" << noteid
                                           << " pg=" << vi.polyGroup);

            ++details.usedVoices.at(vi.polyGroup);
            ++details.totalUsedVoices;

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

template <typename Cfg, typename Responder, typename MonoResponder>
void VoiceManager<Cfg, Responder, MonoResponder>::processNoteOffEvent(int16_t port, int16_t channel,
                                                                      int16_t key, int32_t noteid,
                                                                      float velocity)
{
    for (auto &vi : details.voiceInfo)
    {
        if (vi.matches(port, channel, key, noteid))
        {
            if (details.sustainOn)
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

template <typename Cfg, typename Responder, typename MonoResponder>
void VoiceManager<Cfg, Responder, MonoResponder>::updateSustainPedal(int16_t port, int16_t channel,
                                                                     int8_t level)
{
    auto sop = details.sustainOn;
    details.sustainOn = level > 64;
    if (sop != details.sustainOn)
    {
        if (!details.sustainOn)
        {
            // release all voices with sustain gates
            for (auto &vi : details.voiceInfo)
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

template <typename Cfg, typename Responder, typename MonoResponder>
void VoiceManager<Cfg, Responder, MonoResponder>::routeMIDIPitchBend(int16_t port, int16_t channel,
                                                                     int16_t pb14bit)
{
    if (dialect == MIDI1)
    {
        details.doMonoPitchBend(port, channel, pb14bit);
    }
    else if (dialect == MIDI1_MPE)
    {
        if (channel == mpeGlobalChannel)
        {
            details.doMonoPitchBend(port, -1, pb14bit);
        }
        else
        {
            details.doMPEPitchBend(port, channel, pb14bit);
        }
    }
    else
    {
        // Code this dialect! What is it even?
        assert(false);
    }
}

template <typename Cfg, typename Responder, typename MonoResponder>
size_t VoiceManager<Cfg, Responder, MonoResponder>::getVoiceCount() const
{
    size_t res{0};
    for (const auto &vi : details.voiceInfo)
    {
        res += (vi.activeVoiceCookie != nullptr);
    }
    return res;
}

template <typename Cfg, typename Responder, typename MonoResponder>
size_t VoiceManager<Cfg, Responder, MonoResponder>::getGatedVoiceCount() const
{
    size_t res{0};
    for (const auto &vi : details.voiceInfo)
    {
        res += (vi.activeVoiceCookie != nullptr && vi.gated) ? 1 : 0;
    }
    return res;
}

template <typename Cfg, typename Responder, typename MonoResponder>
void VoiceManager<Cfg, Responder, MonoResponder>::routeNoteExpression(int16_t port, int16_t channel,
                                                                      int16_t key, int32_t noteid,
                                                                      int32_t expression,
                                                                      double value)
{
    for (auto &vi : details.voiceInfo)
    {
        if (vi.matches(port, channel, key,
                       noteid)) // all keys and notes on a channel for midi PB
        {
            responder.setNoteExpression(vi.activeVoiceCookie, expression, value);
        }
    }
}

template <typename Cfg, typename Responder, typename MonoResponder>
void VoiceManager<Cfg, Responder, MonoResponder>::routePolyphonicParameterModulation(
    int16_t port, int16_t channel, int16_t key, int32_t noteid, uint32_t parameter, double value)
{
    for (auto &vi : details.voiceInfo)
    {
        if (vi.matches(port, channel, key,
                       noteid)) // all keys and notes on a channel for midi PB
        {
            responder.setVoicePolyphonicParameterModulation(vi.activeVoiceCookie, parameter, value);
        }
    }
}

template <typename Cfg, typename Responder, typename MonoResponder>
void VoiceManager<Cfg, Responder, MonoResponder>::routePolyphonicAftertouch(int16_t port,
                                                                            int16_t channel,
                                                                            int16_t key, int8_t pat)
{
    for (auto &vi : details.voiceInfo)
    {
        if (vi.matches(port, channel, key, -1)) // all keys and notes on a channel for midi PB
        {
            responder.setPolyphonicAftertouch(vi.activeVoiceCookie, pat);
        }
    }
}

template <typename Cfg, typename Responder, typename MonoResponder>
void VoiceManager<Cfg, Responder, MonoResponder>::routeChannelPressure(int16_t port,
                                                                       int16_t channel, int8_t pat)
{
    if (dialect == MIDI1)
    {
        details.doMonoChannelPressure(port, channel, pat);
    }
    else if (dialect == MIDI1_MPE)
    {
        if (channel == mpeGlobalChannel)
        {
            details.doMonoChannelPressure(port, channel, pat);
        }
        else
        {
            details.doMPEChannelPressure(port, channel, pat);
        }
    }
}

template <typename Cfg, typename Responder, typename MonoResponder>
void VoiceManager<Cfg, Responder, MonoResponder>::routeMIDI1CC(int16_t port, int16_t channel,
                                                               int8_t cc, int8_t val)
{
    if (dialect == MIDI1_MPE && channel != mpeGlobalChannel && cc == mpeTimbreCC)
    {
        for (auto &vi : details.voiceInfo)
        {
            if (vi.activeVoiceCookie && vi.port == port && vi.channel == channel && vi.gated)
            {
                responder.setVoiceMIDIMPETimbre(vi.activeVoiceCookie, val);
            }
        }
    }
    else
    {
        details.midiCCCache[channel][cc] = val;
        monoResponder.setMIDI1CC(channel, cc, val);
    }
}

template <typename Cfg, typename Responder, typename MonoResponder>
void VoiceManager<Cfg, Responder, MonoResponder>::allSoundsOff()
{
    for (const auto &v : details.voiceInfo)
    {
        if (v.activeVoiceCookie)
        {
            responder.terminateVoice(v.activeVoiceCookie);
        }
    }
}

template <typename Cfg, typename Responder, typename MonoResponder>
void VoiceManager<Cfg, Responder, MonoResponder>::allNotesOff()
{
    for (auto &v : details.voiceInfo)
    {
        if (v.activeVoiceCookie)
        {
            responder.releaseVoice(v.activeVoiceCookie, 0);
            v.gated = false;
        }
    }
}

template <typename Cfg, typename Responder, typename MonoResponder>
void VoiceManager<Cfg, Responder, MonoResponder>::setPolyphonyGroupVoiceLimit(uint64_t groupId,
                                                                              int32_t limit)
{
    details.polyLimits[groupId] = limit;
    if (details.usedVoices.find(groupId) == details.usedVoices.end())
        details.usedVoices[groupId] = 0;
    if (details.stealingPriorityMode.find(groupId) == details.stealingPriorityMode.end())
        details.stealingPriorityMode[groupId] = OLDEST;
}

template <typename Cfg, typename Responder, typename MonoResponder>
void VoiceManager<Cfg, Responder, MonoResponder>::setPlaymode(uint64_t groupId, PlayMode pm)
{
}

template <typename Cfg, typename Responder, typename MonoResponder>
void VoiceManager<Cfg, Responder, MonoResponder>::setStealingPriorityMode(uint64_t groupId,
                                                                          StealingPriorityMode pm)
{
    details.stealingPriorityMode[groupId] = pm;
}

} // namespace sst::voicemanager
#endif // VOICEMANAGER_IMPL_H
