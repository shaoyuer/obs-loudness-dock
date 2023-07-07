/*
OBS Loudness Dock
Copyright (C) 2023 Norihiro Kamae <norihiro@nagater.net>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <obs-module.h>
#include <pthread.h>
#include "loudness.h"
#include "deps/libavfilter/ebur128.h"
#include "plugin-macros.generated.h"

typedef DARRAY(double) double_array_t;

struct loudness
{
	int track;
	FFEBUR128State *state;
	pthread_mutex_t mutex;
	double_array_t buf;
	bool paused;
};

void audio_cb(void *param, size_t mix_idx, struct audio_data *data);

static bool init_state(loudness_t *loudness)
{
	struct obs_audio_info oai;
	if (!obs_get_audio_info(&oai))
		return false;

	loudness->state = ff_ebur128_init(get_audio_channels(oai.speakers), oai.samples_per_sec, 0,
					  FF_EBUR128_MODE_M | FF_EBUR128_MODE_S | FF_EBUR128_MODE_I |
						  FF_EBUR128_MODE_LRA | FF_EBUR128_MODE_SAMPLE_PEAK);

	return true;
}

loudness_t *loudness_create(int track)
{
	loudness_t *loudness = bzalloc(sizeof(loudness_t));
	loudness->track = track;

	init_state(loudness);

	pthread_mutex_init(&loudness->mutex, NULL);

	obs_add_raw_audio_callback(track, NULL, audio_cb, loudness);

	return loudness;
}

void loudness_destroy(loudness_t *loudness)
{
	obs_remove_raw_audio_callback(loudness->track, audio_cb, loudness);
	ff_ebur128_destroy(&loudness->state);
	pthread_mutex_destroy(&loudness->mutex);
	da_free(loudness->buf);
	bfree(loudness);
}

void loudness_get(loudness_t *loudness, double results[5])
{
	double peak = 0.0;

	pthread_mutex_lock(&loudness->mutex);

	results[0] = 0.0; // TODO: Implement momentary loudness
	ff_ebur128_loudness_shortterm(loudness->state, &results[1]);
	ff_ebur128_loudness_global(loudness->state, &results[2]);
	ff_ebur128_loudness_range(loudness->state, &results[3]);
	for (size_t ch = 0; ch < loudness->state->channels; ch++) {
		double peak_ch;
		if (ff_ebur128_sample_peak(loudness->state, ch, &peak_ch) == 0) {
			if (peak_ch > peak)
				peak = peak_ch;
		}
	}
	results[4] = obs_mul_to_db(peak);

	pthread_mutex_unlock(&loudness->mutex);
}

void audio_cb(void *param, size_t mix_idx, struct audio_data *data)
{
	UNUSED_PARAMETER(mix_idx);
	loudness_t *loudness = param;

	const size_t nch = loudness->state->channels;
	da_resize(loudness->buf, nch * data->frames);

	double *array = loudness->buf.array;
	const float **data_in = (const float **)data->data;
	for (size_t iframe = 0, k = 0; iframe < data->frames; iframe++) {
		for (size_t ich = 0; ich < nch; ich++)
			array[k++] = data_in[ich][iframe];
	}

	pthread_mutex_lock(&loudness->mutex);

	if (!loudness->paused)
		ff_ebur128_add_frames_double(loudness->state, array, data->frames);

	pthread_mutex_unlock(&loudness->mutex);
}

void loudness_set_pause(loudness_t *loudness, bool paused)
{
	if (paused) {
		obs_remove_raw_audio_callback(loudness->track, audio_cb, loudness);
	}
	else {
		obs_add_raw_audio_callback(loudness->track, NULL, audio_cb, loudness);
	}
}

void loudness_reset(loudness_t *loudness)
{
	pthread_mutex_lock(&loudness->mutex);

	ff_ebur128_destroy(&loudness->state);
	init_state(loudness);

	pthread_mutex_unlock(&loudness->mutex);
}