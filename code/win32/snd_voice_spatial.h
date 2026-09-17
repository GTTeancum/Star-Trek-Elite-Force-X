#ifndef STEFX_SND_VOICE_SPATIAL_H
#define STEFX_SND_VOICE_SPATIAL_H

#include <math.h>

// Dialogue uses spectral-neutral speaker gains, like the PC stereo mixer.
// Coordinates are listener-local: +X right, +Y up, +Z forward.
struct StefxVoiceGains { float left, right, distance; };

static StefxVoiceGains StefxSpatialVoiceGains(float x, float y, float z,
    float referenceDistance, unsigned int listeners)
{
    StefxVoiceGains out;
    out.distance = (float)sqrt(x*x + y*y + z*z);
    const float maximum = referenceDistance > 0.5f ? referenceDistance * 2.0f : 1.0f;
    float attenuation = 1.0f;
    if (out.distance >= maximum) attenuation = 0.0f;
    else if (out.distance > 1.0f) attenuation = (maximum - out.distance) / (maximum - 1.0f);
    // Use the complete distance, so a source directly overhead stays centered.
    const float pan = out.distance > 0.001f ? x / out.distance : 0.0f;
    const float scale = attenuation / (listeners ? (float)listeners : 1.0f);
    out.left = scale * (float)sqrt(0.5f * (1.0f - pan));
    out.right = scale * (float)sqrt(0.5f * (1.0f + pan));
    return out;
}

#endif
