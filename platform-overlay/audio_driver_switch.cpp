#include "audio_driver_switch.h"

#include "core/os/os.h"
#include "core/config/project_settings.h"

#include <malloc.h>
#include <string.h>

#ifdef AUDREN_ENABLED
static constexpr AudioRendererConfig EngineConfig = {
	.output_rate = AudioRendererOutputRate_48kHz,
	.num_voices = 24,
	.num_effects = 0,
	.num_sinks = 1,
	.num_mix_objs = 1,
	.num_mix_buffers = 2,
};
#endif

Error AudioDriverSwitch::_init_device() {
#ifdef AUDREN_ENABLED
	Result res = audrenInitialize(&EngineConfig);
#else
	Result res = audoutInitialize();
#endif
	ERR_FAIL_COND_V_MSG(R_FAILED(res), ERR_CANT_CREATE, vformat("Initialize failed (0x%x)", res));

	const size_t single_buf_bytes = BUFFER_FRAMES * channels * sizeof(int16_t);

#ifdef AUDREN_ENABLED
	res = audrvCreate(&drv, &EngineConfig, channels);
	if (R_FAILED(res)) {
		audrenExit();
		ERR_FAIL_V_MSG(ERR_CANT_CREATE, vformat("audrvCreate failed (0x%x)", res));
	}

	const size_t total_raw_bytes = single_buf_bytes * BUFFER_COUNT;
	mempool_size = (total_raw_bytes + 0xFFF) & ~0xFFFULL;
#else
	res = audoutStartAudioOut();
	if (R_FAILED(res)) {
		audoutExit();
		ERR_FAIL_V_MSG(ERR_CANT_CREATE, vformat("audoutStartAudioOut failed (0x%x)", res));
	}

	const size_t aligned_buf_size = (single_buf_bytes + 0xFFF) & ~0xFFFULL;
	mempool_size = aligned_buf_size * BUFFER_COUNT;
#endif

	mempool_ptr = memalign(0x1000, mempool_size);
	if (!mempool_ptr) {
#ifdef AUDREN_ENABLED
		audrvClose(&drv);
		audrenExit();
#else
		audoutStopAudioOut();
		audoutExit();
#endif
		ERR_FAIL_V_MSG(ERR_OUT_OF_MEMORY, "memalign failed");
	}
	memset(mempool_ptr, 0, mempool_size);

#ifdef AUDREN_ENABLED
	int16_t *pcm_base = static_cast<int16_t *>(mempool_ptr);
	for (int i = 0; i < BUFFER_COUNT; i++) {
		buffers[i] = pcm_base + (i * BUFFER_FRAMES * channels);

		memset(&wavebufs[i], 0, sizeof(AudioDriverWaveBuf));
		wavebufs[i].data_raw = mempool_ptr;
		wavebufs[i].size = total_raw_bytes;
		wavebufs[i].start_sample_offset = i * BUFFER_FRAMES;
		wavebufs[i].end_sample_offset = wavebufs[i].start_sample_offset + BUFFER_FRAMES;
		wavebufs[i].is_looping = false;
	}

	int mpid = audrvMemPoolAdd(&drv, mempool_ptr, mempool_size);
	if (mpid < 0) {
		free(mempool_ptr);
		mempool_ptr = nullptr;
		audrvClose(&drv);
		audrenExit();
		ERR_FAIL_V_MSG(ERR_CANT_CREATE, "audrvMemPoolAdd failed");
	}
	audrvMemPoolAttach(&drv, mpid);

	static const u8 sink_channels[] = { 0, 1 };
	audrvDeviceSinkAdd(&drv, AUDREN_DEFAULT_DEVICE_NAME, channels, sink_channels);

	audrvUpdate(&drv);

	res = audrenStartAudioRenderer();
	if (R_FAILED(res)) {
		free(mempool_ptr);
		mempool_ptr = nullptr;
		audrvClose(&drv);
		audrenExit();
		ERR_FAIL_V_MSG(ERR_CANT_CREATE, vformat("audrenStartAudioRenderer failed (0x%x)", res));
	}

	bool voice_ok = audrvVoiceInit(&drv, 0, channels, PcmFormat_Int16, SAMPLE_RATE);
	if (!voice_ok) {
		free(mempool_ptr);
		mempool_ptr = nullptr;
		audrvClose(&drv);
		audrenExit();
		ERR_FAIL_V_MSG(ERR_CANT_CREATE, "audrvVoiceInit failed");
	}

	audrvVoiceSetDestinationMix(&drv, 0, AUDREN_FINAL_MIX_ID);
	if (channels == 1) {
		audrvVoiceSetMixFactor(&drv, 0, 1.0f, 0, 0);
		audrvVoiceSetMixFactor(&drv, 0, 1.0f, 0, 1);
	} else {
		audrvVoiceSetMixFactor(&drv, 0, 1.0f, 0, 0);
		audrvVoiceSetMixFactor(&drv, 0, 0.0f, 0, 1);
		audrvVoiceSetMixFactor(&drv, 0, 0.0f, 1, 0);
		audrvVoiceSetMixFactor(&drv, 0, 1.0f, 1, 1);
	}
#else
	uint8_t *base_ptr = static_cast<uint8_t *>(mempool_ptr);
	for (int i = 0; i < BUFFER_COUNT; i++) {
		buffers[i] = reinterpret_cast<int16_t *>(base_ptr + (i * aligned_buf_size));

		memset(&audout_bufs[i], 0, sizeof(AudioOutBuffer));
		audout_bufs[i].next = nullptr;
		audout_bufs[i].buffer = buffers[i];
		audout_bufs[i].buffer_size = aligned_buf_size;
		audout_bufs[i].data_size = single_buf_bytes;
		audout_bufs[i].data_offset = 0;
	}
#endif

	mixdown_buffer = memnew_arr(int32_t, BUFFER_FRAMES * channels);
	memset(mixdown_buffer, 0, BUFFER_FRAMES * channels * sizeof(int32_t));

	initialized = true;
#ifdef AUDREN_ENABLED
	TRACE("Init AudRen (libnx) audio driver OK");
#else
	TRACE("Init audioout driver OK");
#endif
	return OK;
}

#ifdef AUDREN_ENABLED
void AudioDriverSwitch::_thread_func_audren(void *p_udata) {
	AudioDriverSwitch *ad = static_cast<AudioDriverSwitch *>(p_udata);
	const size_t single_buf_bytes = BUFFER_FRAMES * ad->channels * sizeof(int16_t);
	const int total_samples = BUFFER_FRAMES * ad->channels;

	svcSetThreadPriority(CUR_THREAD_HANDLE, 0x2B);

	while (!ad->exit_thread) {
		int free_buffer = -1;
		for (int i = 0; i < BUFFER_COUNT; i++) {
			if (ad->wavebufs[i].state == AudioDriverWaveBufState_Free ||
			    ad->wavebufs[i].state == AudioDriverWaveBufState_Done) {
				free_buffer = i;
				break;
			}
		}

		if (free_buffer >= 0) {
			if (!ad->active || ad->pause) {
				memset(ad->buffers[free_buffer], 0, single_buf_bytes);
			} else {
				ad->lock();
				ad->start_counting_ticks();
				ad->audio_server_process(BUFFER_FRAMES, ad->mixdown_buffer);
				ad->stop_counting_ticks();
				ad->unlock();

				for (int s = 0; s < total_samples; s++) {
					ad->buffers[free_buffer][s] = static_cast<int16_t>(ad->mixdown_buffer[s] >> 16);
				}
			}

			armDCacheFlush(ad->buffers[free_buffer], single_buf_bytes);
			audrvVoiceAddWaveBuf(&ad->drv, 0, &ad->wavebufs[free_buffer]);

			if (!audrvVoiceIsPlaying(&ad->drv, 0)) {
				audrvVoiceStart(&ad->drv, 0);
			}

			audrvUpdate(&ad->drv);
			audrenWaitFrame();
		} else {
			audrvUpdate(&ad->drv);
			audrenWaitFrame();
		}
	}

	audrvVoiceStop(&ad->drv, 0);
	audrvUpdate(&ad->drv);
}
#else
void AudioDriverSwitch::_thread_func_audout(void *p_udata) {
	AudioDriverSwitch *ad = static_cast<AudioDriverSwitch *>(p_udata);
	const size_t single_buf_bytes = BUFFER_FRAMES * ad->channels * sizeof(int16_t);
	const int total_samples = BUFFER_FRAMES * ad->channels;

	svcSetThreadPriority(CUR_THREAD_HANDLE, 0x2B);

	for (int i = 0; i < BUFFER_COUNT; i++) {
		memset(ad->buffers[i], 0, single_buf_bytes);
		armDCacheFlush(ad->buffers[i], single_buf_bytes);
		audoutAppendAudioOutBuffer(&ad->audout_bufs[i]);
	}

	while (!ad->exit_thread) {
		AudioOutBuffer *released_buf = nullptr;
		u32 released_count = 0;

		Result res = audoutWaitPlayFinish(&released_buf, &released_count, 100000000ULL);
		if (R_FAILED(res) || !released_buf) {
			continue;
		}

		int16_t *dst = static_cast<int16_t *>(released_buf->buffer);
		if (!ad->active || ad->pause) {
			memset(dst, 0, single_buf_bytes);
		} else {
			ad->lock();
			ad->start_counting_ticks();
			ad->audio_server_process(BUFFER_FRAMES, ad->mixdown_buffer);
			ad->stop_counting_ticks();
			ad->unlock();

			for (int s = 0; s < total_samples; s++) {
				dst[s] = static_cast<int16_t>(ad->mixdown_buffer[s] >> 16);
			}
		}

		armDCacheFlush(dst, single_buf_bytes);
		audoutAppendAudioOutBuffer(released_buf);
	}
}
#endif

Error AudioDriverSwitch::init() {
	active = false;
	exit_thread = false;
	pause = false;

	Error err = _init_device();
	if (err == OK) {
#ifdef AUDREN_ENABLED
		thread.start(AudioDriverSwitch::_thread_func_audren, this);
#else
		thread.start(AudioDriverSwitch::_thread_func_audout, this);
#endif
	}

	return err;
}

void AudioDriverSwitch::finish() {
	exit_thread = true;
	if (thread.is_started()) {
		thread.wait_to_finish();
	}

	active = false;

	if (initialized) {
#ifdef AUDREN_ENABLED
		audrvClose(&drv);
		audrenExit();
#else
		audoutStopAudioOut();
		audoutExit();
#endif
		initialized = false;
	}

	if (mixdown_buffer) {
		memdelete_arr(mixdown_buffer);
		mixdown_buffer = nullptr;
	}

	if (mempool_ptr) {
		free(mempool_ptr);
		mempool_ptr = nullptr;
	}
}

AudioDriverSwitch::AudioDriverSwitch() {
	// Does nothing.
}

AudioDriverSwitch::~AudioDriverSwitch() {
	finish();
}
