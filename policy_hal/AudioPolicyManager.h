/*
 * Copyright (c) 2013-2017, The Linux Foundation. All rights reserved.
 * Not a contribution.
 *
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <audiopolicy/managerdefault/AudioPolicyManager.h>
#include <audio_policy_conf.h>
#include <Volume.h>


namespace android {
#ifndef AUDIO_EXTN_FORMATS_ENABLED
#ifndef WMA_OFFLOAD_ENABLED
#define AUDIO_FORMAT_WMA 0x12000000UL
#define AUDIO_FORMAT_WMA_PRO 0x13000000UL
#endif
#define AUDIO_FORMAT_FLAC 0x1B000000UL
#define AUDIO_FORMAT_ALAC 0x1C000000UL
#ifndef APE_OFFLOAD_ENABLED
#define AUDIO_FORMAT_APE 0x1D000000UL
#endif
#endif

#define WMA_STD_NUM_FREQ     7
#define WMA_STD_NUM_CHANNELS 2
static uint32_t wmaStdSampleRateTbl[WMA_STD_NUM_FREQ] =
{
    8000, 11025, 16000, 22050, 32000, 44100, 48000
};

static uint32_t wmaStdMinAvgByteRateTbl[WMA_STD_NUM_FREQ][WMA_STD_NUM_CHANNELS] =
{
    {128, 12000},
    {8016, 8016},
    {10000, 16000},
    {16016, 20008},
    {20000, 24000},
    {20008, 31960},
    {63000, 63000}
};

static uint32_t wmaStdMaxAvgByteRateTbl[WMA_STD_NUM_FREQ][WMA_STD_NUM_CHANNELS] =
{
    {8000, 12000},
    {10168, 10168},
    {16000, 20000},
    {20008, 32048},
    {20000, 48000},
    {48024, 320032},
    {256008, 256008}
};

#ifndef AAC_ADTS_OFFLOAD_ENABLED
#define AUDIO_FORMAT_AAC_ADTS 0x1E000000UL
#endif

#ifndef AUDIO_EXTN_AFE_PROXY_ENABLED
#define AUDIO_DEVICE_OUT_PROXY 0x1000000
#endif

#define MAX_BITRATE_WMA          384000
#define MAX_BITRATE_WMA_PRO      1536000
#define MAX_BITRATE_WMA_LOSSLESS 1152000
// ----------------------------------------------------------------------------

class AudioPolicyManagerCustom: public AudioPolicyManager
{

public:
        AudioPolicyManagerCustom(AudioPolicyClientInterface *clientInterface);

        virtual ~AudioPolicyManagerCustom() {}

        status_t setDeviceConnectionStateInt(audio_devices_t device,
                                          audio_policy_dev_state_t state,
                                          const char *device_address,
                                          const char *device_name);
        virtual void setPhoneState(audio_mode_t state);
        virtual void setForceUse(audio_policy_force_use_t usage,
                                 audio_policy_forced_cfg_t config);

        virtual bool isOffloadSupported(const audio_offload_info_t& offloadInfo);

        virtual status_t getInputForAttr(const audio_attributes_t *attr,
                                         audio_io_handle_t *input,
                                         audio_session_t session,
                                         uid_t uid,
                                         const audio_config_base_t *config,
                                         audio_input_flags_t flags,
                                         audio_port_handle_t selectedDeviceId,
                                         input_type_t *inputType,
                                         audio_port_handle_t *portId);
        // indicates to the audio policy manager that the input starts being used.
        virtual status_t startInput(audio_io_handle_t input,
                                    audio_session_t session,
                                    concurrency_type__mask_t *concurrency);
        // indicates to the audio policy manager that the input stops being used.
        virtual status_t stopInput(audio_io_handle_t input,
                                   audio_session_t session);

        virtual void closeAllInputs();

protected:

         status_t checkAndSetVolume(audio_stream_type_t stream,
                                                   int index,
                                                   const sp<AudioOutputDescriptor>& outputDesc,
                                                   audio_devices_t device,
                                                   int delayMs = 0, bool force = false);

        // avoid invalidation for active music stream on  previous outputs
        // which is supported on the new device.
        bool isInvalidationOfMusicStreamNeeded(routing_strategy strategy);

        // Must be called before updateDevicesAndOutputs()
        void checkOutputForStrategy(routing_strategy strategy);

        // returns true if given output is direct output
        bool isDirectOutput(audio_io_handle_t output);

        // if argument "device" is different from AUDIO_DEVICE_NONE,  startSource() will force
        // the re-evaluation of the output device.
        status_t startSource(const sp<AudioOutputDescriptor>& outputDesc,
                             audio_stream_type_t stream,
                             audio_devices_t device,
                             const char *address,
                             uint32_t *delayMs);
         status_t stopSource(const sp<AudioOutputDescriptor>& outputDesc,
                            audio_stream_type_t stream,
                            bool forceDeviceUpdate);
        // event is one of STARTING_OUTPUT, STARTING_BEACON, STOPPING_OUTPUT, STOPPING_BEACON
        // returns 0 if no mute/unmute event happened, the largest latency of the device where
        //   the mute/unmute happened
        uint32_t handleEventForBeacon(int){return 0;}
        uint32_t setBeaconMute(bool){return 0;}
#ifdef VOICE_CONCURRENCY
        static audio_output_flags_t getFallBackPath();
        int mFallBackflag;
#endif /*VOICE_CONCURRENCY*/
        void moveGlobalEffect();

        // handle special cases for sonification strategy while in call: mute streams or replace by
        // a special tone in the device used for communication
        void handleIncallSonification(audio_stream_type_t stream, bool starting, bool stateChange, audio_io_handle_t output);
        //parameter indicates of HDMI speakers disabled
        bool mHdmiAudioDisabled;
        //parameter indicates if HDMI plug in/out detected
        bool mHdmiAudioEvent;
private:
        // updates device caching and output for streams that can influence the
        //    routing of notifications
        void handleNotificationRoutingForStream(audio_stream_type_t stream);
        // internal method to return the output handle for the given device and format
        audio_io_handle_t getOutputForDevice(
                audio_devices_t device,
                audio_session_t session,
                audio_stream_type_t stream,
                uint32_t samplingRate,
                audio_format_t format,
                audio_channel_mask_t channelMask,
                audio_output_flags_t flags,
                const audio_offload_info_t *offloadInfo);
        // internal method to fill offload info in case of Direct PCM
        status_t getOutputForAttr(const audio_attributes_t *attr,
                audio_io_handle_t *output,
                audio_session_t session,
                audio_stream_type_t *stream,
                uid_t uid,
                const audio_config_t *config,
                audio_output_flags_t flags,
                audio_port_handle_t selectedDeviceId,
                audio_port_handle_t *portId);
        // Used for voip + voice concurrency usecase
        int mPrevPhoneState;
#ifdef VOICE_CONCURRENCY
        int mvoice_call_state;
#endif
#ifdef RECORD_PLAY_CONCURRENCY
        // Used for record + playback concurrency
        bool mIsInputRequestOnProgress;
#endif

#ifdef FM_POWER_OPT
        float mPrevFMVolumeDb;
        bool mFMIsActive;
#endif
};
};
