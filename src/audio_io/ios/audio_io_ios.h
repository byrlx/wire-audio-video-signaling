/*
 * Wire
 * Copyright (C) 2016 Wire Swiss GmbH
 *
 * The Wire Software is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License,
 * or (at your option) any later version.
 *
 * The Wire Software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with the Wire Software. If not, see <http://www.gnu.org/licenses/>.
 *
 * This module of the Wire Software uses software code from
 * WebRTC (https://chromium.googlesource.com/external/webrtc)
 *
 * *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 * *
 * *  Use of the WebRTC source code on a stand-alone basis is governed by a
 * *  BSD-style license that can be found in the LICENSE file in the root of
 * *  the source tree.
 * *  An additional intellectual property rights grant can be found
 * *  in the file PATENTS.  All contributing project authors to Web RTC may
 * *  be found in the AUTHORS file in the root of the source tree.
 */

#include <AudioUnit/AudioUnit.h>

#include "webrtc/modules/audio_device/include/audio_device.h"
#include <pthread.h>

#define FRAME_LEN_MS 10
#define FS_KHZ 16
#define FRAME_LEN (FRAME_LEN_MS*FS_KHZ)

#define FS_REC_HZ                       16000
#define FS_PLAY_HZ                      16000

#define REC_CHANNELS                    1
#define PLAY_CHANNELS                   2

#define REC_BUF_SIZE_IN_SAMPLES         (FS_REC_HZ / 100)
#define PLAY_BUF_SIZE_IN_SAMPLES        (FS_PLAY_HZ / 100)

#define REC_BUFFERS                     20

namespace webrtc {
    class audio_io_ios : public AudioDeviceModule {
    public:
        audio_io_ios();
        ~audio_io_ios();
        int32_t AddRef() const { return 0; }
        int32_t Release() const { return 0; }
        int32_t RegisterEventObserver(AudioDeviceObserver* eventCallback) {
            return 0;
        }
        int32_t RegisterAudioCallback(AudioTransport* audioCallback);
        int32_t Init();
        int32_t InitSpeaker() { return 0; }
        int32_t SetPlayoutDevice(uint16_t index) { return -1; }
        int32_t SetPlayoutDevice(WindowsDeviceType device) { return -1; }
        int32_t SetStereoPlayout(bool enable) { return 0; }
        int32_t StopPlayout();
        int32_t InitMicrophone() { return 0; }
        int32_t SetRecordingDevice(uint16_t index) { return -1; }
        int32_t SetRecordingDevice(WindowsDeviceType device) { return -1; }
        int32_t SetStereoRecording(bool enable) { return 0; }
        int32_t SetAGC(bool enable) { return -1; }
        int32_t StopRecording();
        int64_t TimeUntilNextProcess() { return 0; }
        void Process() { return; }
        int32_t Terminate();
        
        int32_t ActiveAudioLayer(AudioLayer* audioLayer) const { return -1; }
        ErrorCode LastError() const { return kAdmErrNone; }
        bool Initialized() const { return true; }
        int16_t PlayoutDevices() { return -1; }
        int16_t RecordingDevices() { return -1; }
        int32_t PlayoutDeviceName(uint16_t index,
                                  char name[kAdmMaxDeviceNameSize],
                                  char guid[kAdmMaxGuidSize]);
        int32_t RecordingDeviceName(uint16_t index,
                                            char name[kAdmMaxDeviceNameSize],
                                            char guid[kAdmMaxGuidSize]) {
            return -1;
        }
        int32_t PlayoutIsAvailable(bool* available) { return 0; }
        int32_t InitPlayout();
        bool PlayoutIsInitialized() const;
        int32_t RecordingIsAvailable(bool* available) { return 0; }
        int32_t InitRecording();
        bool RecordingIsInitialized() const;
        int32_t StartPlayout();
        bool Playing() const;
        int32_t StartRecording();
        bool Recording() const;
        bool AGC() const { return true; }
        int32_t SetWaveOutVolume(uint16_t volumeLeft,
                                         uint16_t volumeRight) {
            return -1;
        }
        int32_t WaveOutVolume(uint16_t* volumeLeft,
                                      uint16_t* volumeRight) const {
            return -1;
        }
        bool SpeakerIsInitialized() const { return true; }
        bool MicrophoneIsInitialized() const { return true; }
        int32_t SpeakerVolumeIsAvailable(bool* available) { return 0; }
        int32_t SetSpeakerVolume(uint32_t volume) { return 0; }
        int32_t SpeakerVolume(uint32_t* volume) const { return 0; }
        int32_t MaxSpeakerVolume(uint32_t* maxVolume) const { return 0; }
        int32_t MinSpeakerVolume(uint32_t* minVolume) const { return 0; }
        int32_t SpeakerVolumeStepSize(uint16_t* stepSize) const { return 0; }
        int32_t MicrophoneVolumeIsAvailable(bool* available) { return 0; }
        int32_t SetMicrophoneVolume(uint32_t volume) { return 0; }
        int32_t MicrophoneVolume(uint32_t* volume) const { return 0; }
        int32_t MaxMicrophoneVolume(uint32_t* maxVolume) const { return 0; }
        int32_t MinMicrophoneVolume(uint32_t* minVolume) const { return 0; }
        int32_t MicrophoneVolumeStepSize(uint16_t* stepSize) const {
            return -1;
        }
        int32_t SpeakerMuteIsAvailable(bool* available) { return 0; }
        int32_t SetSpeakerMute(bool enable) { return 0; }
        int32_t SpeakerMute(bool* enabled) const { return 0; }
        int32_t MicrophoneMuteIsAvailable(bool* available) { return 0; }
        int32_t SetMicrophoneMute(bool enable) { return 0; }
        int32_t MicrophoneMute(bool* enabled) const { return 0; }
        int32_t MicrophoneBoostIsAvailable(bool* available) { return 0; }
        int32_t SetMicrophoneBoost(bool enable) { return 0; }
        int32_t MicrophoneBoost(bool* enabled) const { return 0; }
        int32_t StereoPlayoutIsAvailable(bool* available) const;
        int32_t StereoPlayout(bool* enabled) const { return 0; }
        int32_t StereoRecordingIsAvailable(bool* available) const {
            *available = false;
            return 0;
        }
        int32_t StereoRecording(bool* enabled) const { return 0; }
        int32_t SetRecordingChannel(const ChannelType channel) { return 0; }
        int32_t RecordingChannel(ChannelType* channel) const { return 0; }
        int32_t SetPlayoutBuffer(const BufferType type,
                                         uint16_t sizeMS = 0) {
            return 0;
        }
        int32_t PlayoutBuffer(BufferType* type, uint16_t* sizeMS) const {
            return 0;
        }
        int32_t PlayoutDelay(uint16_t* delayMS) const { return 0; }
        int32_t RecordingDelay(uint16_t* delayMS) const { return 0; }
        int32_t CPULoad(uint16_t* load) const { return 0; }
        int32_t StartRawOutputFileRecording(
                                                    const char pcmFileNameUTF8[kAdmMaxFileNameSize]) {
            return 0;
        }
        int32_t StopRawOutputFileRecording() { return 0; }
        int32_t StartRawInputFileRecording(
                                                   const char pcmFileNameUTF8[kAdmMaxFileNameSize]) {
            return 0;
        }
        int32_t StopRawInputFileRecording() { return 0; }
        int32_t SetRecordingSampleRate(const uint32_t samplesPerSec) {
            return 0;
        }
        int32_t RecordingSampleRate(uint32_t* samplesPerSec) const {
            return 0;
        }
        int32_t SetPlayoutSampleRate(const uint32_t samplesPerSec) {
            return 0;
        }
        int32_t PlayoutSampleRate(uint32_t* samplesPerSec) const { return 0; }
        int32_t ResetAudioDevice();
        int32_t SetLoudspeakerStatus(bool enable) { return 0; }
        int32_t GetLoudspeakerStatus(bool* enabled) const { return 0; }
        bool BuiltInAECIsAvailable() const { return false; }
        int32_t EnableBuiltInAEC(bool enable) { return -1; }
        bool BuiltInAECIsEnabled() const { return false; }
        
        void* record_thread();
        
        static OSStatus rec_process(void *inRefCon,
                                      AudioUnitRenderActionFlags *ioActionFlags,
                                      const AudioTimeStamp *timeStamp,
                                      UInt32 inBusNumber,
                                      UInt32 inNumberFrames,
                                      AudioBufferList *ioData);
        
        static OSStatus play_process(void *inRefCon,
                                            AudioUnitRenderActionFlags *ioActionFlags,
                                            const AudioTimeStamp *inTimeStamp,
                                            UInt32 inBusNumber,
                                            UInt32 inNumberFrames,
                              AudioBufferList *ioData);
        
        OSStatus rec_process_impl(AudioUnitRenderActionFlags *ioActionFlags,
                                   const AudioTimeStamp *timeStamp,
                                   uint32_t inBusNumber,
                                   uint32_t inNumberFrames);
        
        OSStatus play_process_impl(uint32_t inNumberFrames, AudioBufferList* ioData);
        
    private:
        int32_t init_play_or_record();
        int32_t shutdown_play_or_record();
        
        void update_rec_delay();
                
        AudioUnit au_;
        
        uint32_t rec_fs_hz_;
        uint32_t play_fs_hz_;
        
        // Delay calculation
        uint32_t rec_delay_;
        
        uint32_t rec_latency_ms_;
        uint32_t prev_rec_latency_ms_;
        
        uint16_t rec_delay_warning_;
        uint16_t play_delay_warning_;
                
        // Recording buffers
        int16_t rec_buffer_[REC_BUFFERS][REC_BUF_SIZE_IN_SAMPLES];
        uint32_t rec_length_[REC_BUFFERS];
        uint32_t rec_seq_[REC_BUFFERS];
        uint32_t rec_current_seq_;
        
        // Playout buffer
        int16_t play_buffer_[2*PLAY_BUF_SIZE_IN_SAMPLES];
        uint32_t play_buffer_used_;  // How much is filled
        
        // Current total size all data in buffers, used for delay estimate
        uint32_t rec_buffer_total_size_;
        
        uint32_t _capture_latency_ms;
        uint32_t _render_latency_ms;
        uint32_t _prev_capture_latency_ms;
        uint32_t _prev_render_latency_ms;
        
        bool input_device_specified_;
        bool output_device_specified_;
        
        bool initialized_;
        bool is_shut_down_;
        bool rec_is_initialized_;
        bool play_is_initialized_;
        
        AudioTransport* audioCallback_;
        pthread_t rec_tid_ = 0;
        volatile bool is_recording_;
        volatile bool is_playing_;
        volatile bool is_recording_initialized_;
        volatile bool is_playing_initialized_;

        pthread_mutex_t mutex_;
        pthread_mutex_t cond_mutex_;
        pthread_cond_t cond_;
        bool is_running_;
        
        Float64 used_sample_rate_;
        
        int32_t tot_rec_delivered_;
        int32_t num_capture_worker_calls_;
    };
}
