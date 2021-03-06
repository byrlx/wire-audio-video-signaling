/*
* Wire
* Copyright (C) 2016 Wire Swiss GmbH
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*/
#include <cerrno>
#include <cstddef>
#include <stdio.h>
#include <string>
#include <pthread.h>

#include <sys/time.h>

#include "webrtc/modules/audio_coding/include/audio_coding_module.h"
#include "webrtc/system_wrappers/include/trace.h"

#include "webrtc/voice_engine/include/voe_base.h"
#include "webrtc/voice_engine/include/voe_network.h"
#include "webrtc/voice_engine/include/voe_codec.h"
#include "webrtc/voice_engine/include/voe_errors.h"
#include "webrtc/voice_engine/include/voe_volume_control.h"
#include "webrtc/voice_engine/include/voe_audio_processing.h"

#include "NwSimulator.h"

#if defined(WEBRTC_ANDROID)
#include <android/log.h>
#endif

/**********************************/
/* RTP help functions             */
/**********************************/

#define RTP_HEADER_IN_BYTES 12
#define MAX_PACKET_SIZE_BYTES 1000

#define DELTA_TIME_MS 10
#define PACKET_SIZE_MS 20
#define NETEQ_STATS_DUMP_MS 2500

#define NUM_LOOPS 10

#if defined(WEBRTC_ANDROID)
#define LOG(...) ((void)__android_log_print(ANDROID_LOG_INFO, "audiotest : start_stop_stress_test", __VA_ARGS__))
#else
#define LOG(...) printf(__VA_ARGS__)
#endif

static void burn_cpu(float ms_target){
    struct timeval now, startTime, res;
    int tmp = 0, tmp1, tmp2;
    float ms_spend = 0;
    
    while( ms_spend < ms_target ){
        gettimeofday(&startTime, NULL);
        for(int i = 0; i < 10; i++ ){
            tmp1 = rand();
            for(int i = 0; i < 10; i++ ){
                tmp2 = rand();
                tmp = tmp + tmp1 * tmp2;
            }
        }
        gettimeofday(&now, NULL);
        timersub(&now, &startTime, &res);
        ms_spend += (float)res.tv_sec*1000.0 + (float)res.tv_usec/1000.0;
    }
}

static int MakeRTPheader( uint8_t* rtpHeader,
                  const uint8_t payloadType,
                  const uint16_t seqNum,
                  const uint32_t timeStamp,
                  const uint32_t ssrc){
    
    rtpHeader[0] = (uint8_t) 0x80;
    rtpHeader[1] = (uint8_t) (payloadType & 0xFF);
    rtpHeader[2] = (uint8_t) ((seqNum >> 8) & 0xFF);
    rtpHeader[3] = (uint8_t) (seqNum & 0xFF);
    rtpHeader[4] = (uint8_t) ((timeStamp >> 24) & 0xFF);
    rtpHeader[5] = (uint8_t) ((timeStamp >> 16) & 0xFF);
    rtpHeader[6] = (uint8_t) ((timeStamp >> 8) & 0xFF);
    rtpHeader[7] = (uint8_t) (timeStamp & 0xFF);
    rtpHeader[8] = (uint8_t) ((ssrc >> 24) & 0xFF);
    rtpHeader[9] = (uint8_t) ((ssrc >> 16) & 0xFF);
    rtpHeader[10] = (uint8_t) ((ssrc >> 8) & 0xFF);
    rtpHeader[11] = (uint8_t) (ssrc & 0xFF);
    
    return(RTP_HEADER_IN_BYTES);
}

namespace stress {
/**************************************************/
/* Transport callback writes to file              */
/**************************************************/
class TransportCallBack : public webrtc::AudioPacketizationCallback {

public:
    TransportCallBack( std::string name ){
        seqNo_ = 0;
        ssrc_ = 0;
        totBytes_ = 0;
        timestamps_per_packet_ = 0;
        prev_timestamp_ = 0;
        packetLenBytes_ = 0;
        timestamp_offset_ = rand();
        fp_ = fopen(name.c_str(),"wb");
        gettimeofday(&startTime_, NULL);
    }
    
    ~TransportCallBack(){
        if(fp_){
            fclose(fp_);
        }
    }
    
    int32_t SendData(
                    webrtc::FrameType     frame_type,
                    uint8_t       payload_type,
                    uint32_t      timestamp,
                    const uint8_t* payload_data,
                    size_t      payload_len_bytes,
                    const webrtc::RTPFragmentationHeader* fragmentation){
            
        int headerLenBytes;
        
        timestamp = timestamp + timestamp_offset_;
            
        headerLenBytes = MakeRTPheader(packet_,
                                       payload_type,
                                       seqNo_++,
                                       timestamp,
                                       ssrc_);
            
        if( payload_len_bytes < MAX_PACKET_SIZE_BYTES ){
            memcpy(&packet_[headerLenBytes], payload_data, payload_len_bytes * sizeof(uint8_t));
            packetLenBytes_ = (uint32_t)payload_len_bytes + (uint32_t)headerLenBytes;
        } else {
            packetLenBytes_ = 0;
        }
            
        if((prev_timestamp_ != 0) && (timestamps_per_packet_ == 0)){
            timestamps_per_packet_ = timestamp - prev_timestamp_;
        }
        prev_timestamp_ = timestamp;
            
        struct timeval now, res;
        gettimeofday(&now, NULL);
        timersub(&now, &startTime_, &res);
        int32_t ms = (int32_t)res.tv_sec*1000 + (int32_t)res.tv_usec/1000;
            
        fwrite( &ms, sizeof(int32_t), 1, fp_);
        fwrite( &packetLenBytes_, sizeof(uint32_t), 1, fp_);
        fwrite( packet_, sizeof(uint8_t), packetLenBytes_, fp_);
        
        totBytes_ += payload_len_bytes;
        
        return (int)packetLenBytes_;
    }
        
    int32_t get_total_bytes(){
        return(totBytes_);
    }
        
    int32_t get_timestamps_per_packet(){
        return(timestamps_per_packet_);
    }
    
private:
    uint32_t ssrc_;
    uint32_t seqNo_;
    uint32_t totBytes_;
    uint16_t timestamps_per_packet_;
    uint32_t prev_timestamp_;
    uint32_t timestamp_offset_;
    uint8_t  packet_[RTP_HEADER_IN_BYTES + MAX_PACKET_SIZE_BYTES];
    uint32_t packetLenBytes_;
    FILE* fp_;
    struct timeval startTime_;
};

class DummyTransport : public webrtc::Transport {
public:
    DummyTransport(int packet_size_ms){
        packet_size_ms_ = packet_size_ms;
        prev_snd_time_ms_ = -1;
        gettimeofday(&start_time_, NULL);
    };
    virtual~ DummyTransport() {
    };
    
	virtual bool SendRtp(const uint8_t* packet, size_t length, const webrtc::PacketOptions& options) {
        struct timeval now, res;
        gettimeofday(&now, NULL);
        timersub(&now, &start_time_, &res);
        int32_t now_ms = (int32_t)res.tv_sec*1000 + (int32_t)res.tv_usec/1000;
        if(prev_snd_time_ms_ > 0){
            if((now_ms - prev_snd_time_ms_) > 2*packet_size_ms_){
                LOG("Warning DummyTransport(%p) inter packet Jitter = %d ms \n", this, (now_ms - prev_snd_time_ms_) - packet_size_ms_);
            }
        }
        prev_snd_time_ms_ = now_ms;
        
        return true;
    };
    
    virtual bool SendRtcp(const uint8_t* packet, size_t length) {
        return true;
    };
    
    void deregister()
    {
    }
private:
    int packet_size_ms_;
    int prev_snd_time_ms_;
    struct timeval start_time_;
};

class VoELogCallback : public webrtc::TraceCallback {
public:
    VoELogCallback() {};
    virtual ~VoELogCallback() {};
    
    virtual void Print(webrtc::TraceLevel lvl, const char* message,
                       int len) override
    {        
        LOG("%s \n", message);
    };
};

static VoELogCallback logCb;

class MyObserver : public webrtc::VoiceEngineObserver {
public:
    virtual void CallbackOnError(int channel, int err_code);
};
void MyObserver::CallbackOnError(int channel, int err_code)
{
    std::string msg;
    
    // Add printf for other error codes here
    if (err_code == VE_RECEIVE_PACKET_TIMEOUT) {
        msg = "VE_RECEIVE_PACKET_TIMEOUT\n";
    } else if (err_code == VE_PACKET_RECEIPT_RESTARTED) {
        msg = "VE_PACKET_RECEIPT_RESTARTED\n";
    } else if (err_code == VE_RUNTIME_PLAY_WARNING) {
        msg = "VE_RUNTIME_PLAY_WARNING\n";
    } else if (err_code == VE_RUNTIME_REC_WARNING) {
        msg = "VE_RUNTIME_REC_WARNING\n";
    } else if (err_code == VE_SATURATION_WARNING) {
        msg = "VE_SATURATION_WARNING\n";
    } else if (err_code == VE_RUNTIME_PLAY_ERROR) {
        msg = "VE_RUNTIME_PLAY_ERROR\n";
    } else if (err_code == VE_RUNTIME_REC_ERROR) {
        msg = "VE_RUNTIME_REC_ERROR\n";
    } else if (err_code == VE_REC_DEVICE_REMOVED) {
        msg = "VE_REC_DEVICE_REMOVED\n";
    }
    LOG("CallbackOnError msg = %s \n", msg.c_str());
}
    
    bool is_running = false;
}

using namespace stress;
static MyObserver my_observer;

#define VOE_THREAD_LOAD_PCT 100
#define MAIN_THREAD_LOAD_PCT 100

static void *main_function(void *arg)
{
    webrtc::VoiceEngine* ve = webrtc::VoiceEngine::Create();
    
    webrtc::VoEBase* base = webrtc::VoEBase::GetInterface(ve);
    if (!base) {
        LOG("VoEBase::GetInterface failed \n");
    }
    webrtc::VoENetwork *nw = webrtc::VoENetwork::GetInterface(ve);
    if (!nw) {
        LOG("VoENetwork::GetInterface failed \n");
    }
    webrtc::VoECodec *codec = webrtc::VoECodec::GetInterface(ve);
    if (!codec) {
        LOG("VoECodec::GetInterface failed \n");
    }
    webrtc::VoEVolumeControl *volume = webrtc::VoEVolumeControl::GetInterface(ve);
    if(!volume){
        LOG("VoEVolumeControl::GetInterface failed \n");
    }
    webrtc::VoEAudioProcessing *proc = webrtc::VoEAudioProcessing::GetInterface(ve);
    if(!proc){
        LOG("VoEAudioProcessing::GetInterface failed \n");
    }
    
    int numChannels, i;
    int ret, num_frames = 0;
    size_t read_count;
    
    webrtc::AudioFrame audioframe;
    
#if TARGET_OS_IPHONE || defined(WEBRTC_ANDROID)
    std::string file_path = (const char *)arg;
#else
    std::string file_path = "../../../test/audio_test/files/";
#endif
    
    if(file_path.length()){
        std::string slash = "/";
        if(file_path[file_path.length()-1] != slash[slash.length()-1]){
            file_path = file_path + slash;
        }
    }else{
        LOG("Error: No file path specified \n");
    }
    std::string file_name = file_path + "far32.pcm";
    
    webrtc::CodecInst c;
    
    int numberOfCodecs = codec->NumOfCodecs();
    bool codec_found = false;
    for( int i = 0; i < numberOfCodecs; i++ ){
        ret = codec->GetCodec( i, c);
        if(strcmp(c.plname,"opus") == 0){
            codec_found = true;
            break;
        }
    }
    c.rate = 40000;
    c.channels = 1;
    c.pacsize = (c.plfreq * PACKET_SIZE_MS) / 1000;
    
    /* Encode audio files to RTP stream files */
    std::unique_ptr<webrtc::AudioCodingModule> acm(webrtc::AudioCodingModule::Create(0));
    ret = acm->RegisterSendCodec(c);
    if( ret < 0 ){
        LOG("acm->SetSendCodec returned %d \n", ret);
    }
        
    std::string rtp_file_name;
#if TARGET_OS_IPHONE || defined(WEBRTC_ANDROID)
    rtp_file_name = file_path;
    rtp_file_name.insert(rtp_file_name.length(),"/rtp_ch0");
#else
    rtp_file_name = "rtp_ch0";
#endif
    rtp_file_name.insert(rtp_file_name.size(),".dat");
        
    TransportCallBack tCB( rtp_file_name.c_str() );
    
    if(acm->RegisterTransportCallback((webrtc::AudioPacketizationCallback*) &tCB) < 0){
        LOG("Register Transport Callback failed \n");
    }
        
    FILE *in_file = fopen(file_name.c_str(),"rb");
    if(in_file == NULL){
        LOG("Cannot open %s for reading \n", file_name.c_str());
    }
    LOG("Encoding file %s \n", file_name.c_str());
        
    audioframe.sample_rate_hz_ = 32000;
    audioframe.num_channels_ = 1;
    audioframe.samples_per_channel_ = audioframe.sample_rate_hz_/100;
    size_t size = audioframe.samples_per_channel_ * audioframe.num_channels_;
    for(num_frames = 0; num_frames < 500; num_frames++ ) {
        read_count = fread(audioframe.data_,
                            sizeof(int16_t),
                            size,
                            in_file);
            
        if(read_count < size){
            break;
        }
            
        audioframe.timestamp_ = num_frames * audioframe.samples_per_channel_;
            
        ret = acm->Add10MsData(audioframe);
        if( ret < 0 ){
            LOG("acm->Add10msData returned %d \n", ret);
        }
    }
    fclose(in_file);
    
    webrtc::Trace::CreateTrace();
    webrtc::Trace::SetTraceCallback(&logCb);
    webrtc::Trace::set_level_filter(webrtc::kTraceWarning | webrtc::kTraceError | webrtc::kTraceCritical | webrtc::kTracePersist);
    
    base->Init();
    
    /* SetUp HP filter */
    proc->EnableHighPassFilter( true );
    
    /* SetUp AGC */
    proc->SetAgcStatus( true, webrtc::kAgcAdaptiveDigital);
    
    /* Setup Noise Supression */
    proc->SetNsStatus( true, webrtc::kNsHighSuppression );
    
    /* setup AEC */
#if defined(WEBRTC_ANDROID)
	proc->SetEcStatus( true, webrtc::kEcAecm);
    proc->SetAecmMode( webrtc::kAecmLoudSpeakerphone, false );
#else
    
#endif
    
#if defined(WEBRTC_ANDROID)
    proc->StartDebugRecording("/sdcard/proc.aecdump");
#endif
    
    base->RegisterVoiceEngineObserver(my_observer);
    
    DummyTransport* transport = new DummyTransport(PACKET_SIZE_MS);
    
    NwSimulator* nw_sim = new NwSimulator();
    nw_sim->Init(PACKET_SIZE_MS, 0, 1.0f, NW_type_clean, file_path);
    
    struct timeval now, res, start_time;
    
    int32_t rcv_time_ms, next_ms = DELTA_TIME_MS;
    uint32_t bytesIn;
    uint8_t RTPpacketBuf[MAX_PACKET_SIZE_BYTES];
    size_t read = 0;
    
    FILE *fp = NULL;
    for(int i = 0; i < NUM_LOOPS; i++){
        /* Setup File pointers */
        fp = fopen(rtp_file_name.c_str(),"rb");
    
        int channel_id = base->CreateChannel();
        nw->RegisterExternalTransport(channel_id, *transport);
        
        base->StartReceive(channel_id);
        base->StartPlayout(channel_id);
        
        base->StartSend(channel_id);
    
        codec->SetSendCodec(channel_id, c);
        
        gettimeofday(&start_time, NULL);
        next_ms = DELTA_TIME_MS;
        
        while(1){
            if( next_ms % PACKET_SIZE_MS == 0){
                /* Read a packet */
                read = fread(&rcv_time_ms, sizeof(int32_t), 1, fp);
                if(read > 0){
                    fread(&bytesIn, sizeof(uint32_t), 1, fp);
                    fread(RTPpacketBuf, sizeof(uint8_t), bytesIn, fp);
                    
                    nw_sim->Add_Packet(RTPpacketBuf, bytesIn, next_ms);
                } else {
                    break;
                }
            }
            // Get arrived packets from Network Queue
            int bytesIn = nw_sim->Get_Packet(RTPpacketBuf, next_ms);
            while(bytesIn > 0){
              nw->ReceivedRTPPacket(channel_id, (const void*)RTPpacketBuf, bytesIn);
                    
              bytesIn = nw_sim->Get_Packet(RTPpacketBuf, next_ms);
            }
            gettimeofday(&now, NULL);
            timersub(&now, &start_time, &res);
            int32_t now_ms = (int32_t)res.tv_sec*1000 + (int32_t)res.tv_usec/1000;
            int32_t sleep_ms = next_ms - now_ms;
            if(sleep_ms < 0){
                LOG("Warning sleep_ms = %d not reading fast enough !! \n", sleep_ms);
                sleep_ms = 0;
            }
            next_ms += DELTA_TIME_MS;
            
            burn_cpu((VOE_THREAD_LOAD_PCT*sleep_ms/100));
            timespec t;
            t.tv_sec = 0;
            t.tv_nsec = ((100 - VOE_THREAD_LOAD_PCT)*sleep_ms/100)*1000*1000;
            nanosleep(&t, NULL);
        }
        base->StopSend(channel_id);

        base->StopReceive(channel_id);
        base->StopPlayout(channel_id);

        // Close down the transport
        nw->DeRegisterExternalTransport(channel_id);
        
        base->DeleteChannel(channel_id);
        
        fclose(fp);
    }
    
    
    delete transport;
    delete nw_sim;
    
#if defined(WEBRTC_ANDROID)
    proc->StopDebugRecording();
#endif

    if(proc){
        proc->Release();
        proc = NULL;
    }
    if(volume){
        volume->Release();
        volume = NULL;
    }
    if(codec){
        codec->Release();
        codec = NULL;
    }
    if(nw){
        nw->Release();
        nw = NULL;
    }
    if (base) {
        base->Terminate();
        base->Release();
        base = NULL;
    }
    
    webrtc::VoiceEngine::Delete(ve);
    
    is_running = false;
    
    return NULL;
}

#if TARGET_OS_IPHONE || defined(WEBRTC_ANDROID)
int start_stop_stress_test(int argc, char *argv[], const char *path)
#else
int main(int argc, char *argv[])
#endif
{
    pthread_t tid;
    void* thread_ret;
    is_running = true;
#if TARGET_OS_IPHONE || defined(WEBRTC_ANDROID)
    std::string tmp_path = path;
    pthread_create(&tid, NULL, main_function, (void*)path);
#else
    pthread_create(&tid, NULL, main_function, NULL);
#endif
    while(is_running){
        burn_cpu(MAIN_THREAD_LOAD_PCT);
        timespec t;
        t.tv_sec = 0;
        t.tv_nsec = (100 - MAIN_THREAD_LOAD_PCT)*1000*1000;
        nanosleep(&t, NULL);
    }
    
    pthread_join(tid, &thread_ret);

    return 0;
}
