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

#ifndef INCLUDE_SST_VOICEMANAGER_VOICEMANAGER_H
#define INCLUDE_SST_VOICEMANAGER_VOICEMANAGER_H

#include <array>
#include <cassert>

/**
 * \mainpage SST Voice Manager
 *
 * The SST Voice Manager abstracts the concept of voice management into a header-only class
 * which collaborates with your synth using 'responders', or objects which can undertake
 * activities around a single voice or single monophonic value.
 */

namespace sst::voicemanager
{

/**
 * VoiceInitBufferEntry is the object which the responder needs to populate
 * in the voice intiaition creation lifecycle.
 *
 * @tparam Cfg the voice manager configuration trait
 */
template <typename Cfg> struct VoiceInitBufferEntry
{
    typename Cfg::voice_t *voice; ///< A pointer to the voice, owned by the responder

    /**
     * buffer_t is the typedef for the working group array you will receive
     * in init voices. Use `buffer_t &` rather than the array expansions.
     */
    using buffer_t = std::array<VoiceInitBufferEntry<Cfg>, Cfg::maxVoiceCount>;
};

/**
 * VoiceBeginBufferEntry is the object which the responder needs to populate
 * in the voice begin creation lifecycle.
 *
 * @tparam Cfg the voice manager configuration trait
 */
template <typename Cfg> struct VoiceBeginBufferEntry
{
    uint64_t polyphonyGroup; ///< The polyphony group in which this voice participates

    /**
     * buffer_t is the typedef for the working group array you will receive
     * in init voices. Use `buffer_t &` rather than the array expansions.
     */
    using buffer_t = std::array<VoiceBeginBufferEntry<Cfg>, Cfg::maxVoiceCount>;
};

/**
 * VoiceManager is the main class used for voice management, and the sole public API.
 * The voice manager has the following features
 * - blah
 * - blah
 * - blah
 *
 * The VoiceManager collaborates with your symth through three
 *
 * @tparam Cfg A configuration trait, setting the hard (physical) voice limit and voice type
 * @tparam Responder A class responsible for responding to voice-level activities such as creation,
 * termination, setting of voice level properties, and so forth
 * @tparam MonoResponder A class responsible for responding to monophoni activities such as MIDI CC
 * and Channel PitchBend (which are monopnonic in non-MPE mode)
 */
template <typename Cfg, typename Responder, typename MonoResponder> struct VoiceManager
{
    enum MIDI1Dialect
    {
        MIDI1,
        MIDI1_MPE
    } dialect{MIDI1};

    /*
     * If a key is struck twice while still gated or sustained, do we start a new voice
     * or do we re-use the voice (and move the note id)
     */
    enum RepeatedKeyMode
    {
        MULTI_VOICE,
        PIANO
    } repeatedKeyMode{MULTI_VOICE};

    enum PlayMode
    {
        POLY,
        MONO,
        MONO_LEGATO
    };

    enum StealingPriorityMode
    {
        OLDEST,
        HIGHEST,
        LOWEST
    };

    int8_t mpeGlobalChannel{0};
    int8_t mpeTimbreCC{74};

    Responder &responder;
    MonoResponder &monoResponder;
    VoiceManager(Responder &r, MonoResponder &m);

    void registerVoiceEndCallback();

    bool processNoteOnEvent(int16_t port, int16_t channel, int16_t key, int32_t noteid,
                            float velocity, float retune);

    void processNoteOffEvent(int16_t port, int16_t channel, int16_t key, int32_t noteid,
                             float velocity);

    void updateSustainPedal(int16_t port, int16_t channel, int8_t level);

    void routeMIDIPitchBend(int16_t port, int16_t channel, int16_t pb14bit);

    void routeMIDI1CC(int16_t port, int16_t channel, int8_t cc, int8_t val);

    void routePolyphonicAftertouch(int16_t port, int16_t channel, int16_t key, int8_t pat);

    void routeChannelPressure(int16_t port, int16_t channel, int8_t pat);
    void routeNoteExpression(int16_t port, int16_t channel, int16_t key, int32_t noteid,
                             int32_t expression, double value);

    void routePolyphonicParameterModulation(int16_t port, int16_t channel, int16_t key,
                                            int32_t noteid, uint32_t parameter, double value);

    size_t getVoiceCount() const;
    size_t getGatedVoiceCount() const;
    void allNotesOff();
    void allSoundsOff();

    void setPolyphonyGroupVoiceLimit(uint64_t groupId, int32_t limit);
    void setPlaymode(uint64_t groupId, PlayMode pm);
    void setStealingPriorityMode(uint64_t groupId, StealingPriorityMode pm);

  private:
    struct Details;
    Details details;
};
} // namespace sst::voicemanager

#include "voicemanager_impl.h"

#endif // INCLUDE_SST_VOICEMANAGER_VOICEMANAGER_H
