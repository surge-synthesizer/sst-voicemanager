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
#ifndef INCLUDE_SST_VOICEMANAGER_VOICEMANAGER_H
#define INCLUDE_SST_VOICEMANAGER_VOICEMANAGER_H

#include <array>

#include "managers/polymanager.h"

namespace sst::voicemanager
{
template <typename Cfg, typename Responder> struct VoiceManager
{
    enum VoiceMode
    {
        POLY
    } voiceMode{POLY};

    Responder &responder;
    VoiceManager(Responder &r) : responder(r), polyManager(r)
    {
        polyManager.registerVoiceEndCallback();
    }

    void setVoiceMode(VoiceMode m)
    {
        // reset current, replace callback function on responder, etc...
        responder.stopAllVoices();
        voiceMode = m;
        switch (voiceMode)
        {
        case POLY:
            polyManager.registerVoiceEndCallback();
        }
    }

    bool processNoteOnEvent(int16_t port, int16_t channel, int16_t key, int32_t noteid,
                            float velocity, float retune)
    {
        switch (voiceMode)
        {
        case POLY:
            return polyManager.processNoteOnEvent(port, channel, key, noteid, velocity, retune);
        }

        return false;
    }

    void processNoteOffEvent(int16_t port, int16_t channel, int16_t key, int32_t noteid,
                             float velocity)
    {
        switch (voiceMode)
        {
        case POLY:
            polyManager.processNoteOffEvent(port, channel, key, noteid, velocity);
        }
    }

    void updateSustainPedal(int16_t port, int16_t channel, int8_t level)
    {
        switch(voiceMode)
        {
        case POLY:
            polyManager.updateSustainPedal(port, channel, level);
        }
    }

    void routeMIDIPitchBend(int16_t port, int16_t channel, int16_t pb14bit)
    {
        switch (voiceMode)
        {
        case POLY:
            polyManager.routeMIDIPitchBend(port, channel, pb14bit);
        }
    }

    void routeNoteExpression(int16_t port, int16_t channel, int16_t key, int32_t noteid,
                             int32_t expression, double value)
    {
        switch (voiceMode)
        {
        case POLY:
            polyManager.routeNoteExpression(port, channel, key, noteid, expression, value);
        }
    }

    void routePolyphonicParameterModulation(int16_t port, int16_t channel, int16_t key,
                                            int32_t noteid, uint32_t parameter, double value)
    {
        switch (voiceMode)
        {
        case POLY:
            polyManager.routePolyphonicParameterModulation(port, channel, key, noteid, parameter,
                                                           value);
        }
    }

    size_t getVoiceCount()
    {
        switch (voiceMode)
        {
        case POLY:
            return polyManager.getVoiceCount();
        }
        assert(false);
        return 0;
    }

    size_t getGatedVoiceCount()
    {
        switch (voiceMode)
        {
        case POLY:
            return polyManager.getGatedVoiceCount();
        }
        assert(false);
        return 0;
    }

    static float midiToFloatVelocity(uint8_t vel) { return 1.f * vel / 127.f; }

    using polymanager_t = sst::voicemanager::managers::PolyManager<Cfg, Responder>;
    polymanager_t polyManager;
};
} // namespace sst::voicemanager

#endif // INCLUDE_SST_VOICEMANAGER_VOICEMANAGER_H
