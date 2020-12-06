#include "audio.h"

void setHwParams(snd_pcm_t* hWave, int numChannels, int bytesPerSample, int samplePerSec, int period, int numBuffer) {
    snd_pcm_hw_params_t* hwparams;
    int result;

    snd_pcm_hw_params_alloca(&hwparams);

    result = snd_pcm_hw_params_any(hWave, hwparams);
    if (result < 0) {
        printf("snd_pcm_hw_params_any failed: %s\n", snd_strerror(result));
        return;
    }

    result = snd_pcm_hw_params_set_format(hWave, hwparams, SND_PCM_FORMAT_S16_LE);
    if (result < 0) {
        printf("snd_pcm_hw_params_set_format failed: %s\n", snd_strerror(result));
        return;
    }

    result = snd_pcm_hw_params_set_rate(hWave, hwparams, samplePerSec, 0);
    if (result < 0) {
        printf("snd_pcm_hw_params_set_rate failed: %s\n", snd_strerror(result));
        return;
    }

    result = snd_pcm_hw_params_set_channels(hWave, hwparams, numChannels);
    if (result < 0) {
        printf("snd_pcm_hw_params_set_channels failed: %s\n", snd_strerror(result));
        return;
    }

    result = snd_pcm_hw_params_set_access(hWave, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (result < 0) {
        printf("snd_pcm_hw_params_set_access failed: %s\n", snd_strerror(result));
        return;
    }

    snd_pcm_uframes_t period_size = period;
    int dir = 0;
    result = snd_pcm_hw_params_set_period_size_near(hWave, hwparams, &period_size, &dir);
    if (result < 0) {
        printf("snd_pcm_hw_params_set_period_size_near failed: %s\n", snd_strerror(result));
        return;
    }

    snd_pcm_uframes_t target_buffer_size = period_size * numBuffer;
    result = snd_pcm_hw_params_set_buffer_size_near(hWave, hwparams, &target_buffer_size);
    if (result < 0) {
        printf("snd_pcm_hw_params_set_buffer_size_near failed: %s\n", snd_strerror(result));
        return;
    }

    result = snd_pcm_hw_params(hWave, hwparams);
    if (result < 0) {
        printf("snd_pcm_hw_params failed: %s\n", snd_strerror(result));
        return;
    }
}