#ifndef AUDIO_H
#define AUDIO_H

#include <alsa/asoundlib.h>

void setHwParams(snd_pcm_t* hWave, int numChannels, int bytesPerSample, int samplePerSec, int period, int numBuffer);

#endif
