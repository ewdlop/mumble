#include <portaudio.h>
#include <pa_win_wasapi.h>
#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>
#include <vector>
#include <deque>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Realtime AEC using: mic input + WASAPI loopback (speaker mix) as far-end
// - mic + processed-out on full-duplex stream
// - loopback captured on a separate input-only stream (WASAPI-specific)
// - 48kHz mono, 10ms frames, 100ms filter

class RealtimeAECWithLoopback {
private:
    static const int SAMPLE_RATE = 48000;
    static const int FRAME_SIZE = 480;     // 10ms @ 48k
    static const int FILTER_LENGTH = 4800; // 100ms @ 48k

    // Streams
    PaStream* duplexStream;   // mic in + processed out
    PaStream* loopbackStream; // speaker mix as far-end

    // SpeexDSP
    SpeexEchoState* echoState;
    SpeexPreprocessState* preprocessState;

    // Buffers
    std::vector<short> micFrame;
    std::vector<short> farFrame;
    std::vector<short> outFrame;

    // Loopback ring buffer (store a few frames)
    std::deque<std::vector<short>> loopbackQueue;
    std::mutex loopbackMutex;
    static const size_t MAX_LOOPBACK_QUEUE = 8; // ~80ms

    // Control
    std::atomic<bool> running;
    std::atomic<unsigned long> processedBlocks;

public:
    RealtimeAECWithLoopback()
        : duplexStream(nullptr), loopbackStream(nullptr),
          echoState(nullptr), preprocessState(nullptr),
          running(false), processedBlocks(0) {
        micFrame.resize(FRAME_SIZE);
        farFrame.resize(FRAME_SIZE);
        outFrame.resize(FRAME_SIZE);
    }

    ~RealtimeAECWithLoopback() { cleanup(); }

    bool initialize() {
        PaError err = Pa_Initialize();
        if (err != paNoError) {
            std::cerr << "PortAudio init failed: " << Pa_GetErrorText(err) << std::endl;
            return false;
        }

        // Check WASAPI host availability
        int wasapiIndex = -1;
        int hostCount = Pa_GetHostApiCount();
        for (int i = 0; i < hostCount; ++i) {
            const PaHostApiInfo* info = Pa_GetHostApiInfo(i);
            if (info && info->type == paWASAPI) { wasapiIndex = i; break; }
        }
        if (wasapiIndex < 0) {
            std::cerr << "WASAPI not available. Loopback capture requires WASAPI host." << std::endl;
            return false;
        }

        // Init Speex
        echoState = speex_echo_state_init(FRAME_SIZE, FILTER_LENGTH);
        if (!echoState) { std::cerr << "Failed to init echo state" << std::endl; return false; }
        int sr = SAMPLE_RATE;
        speex_echo_ctl(echoState, SPEEX_ECHO_SET_SAMPLING_RATE, &sr);

        preprocessState = speex_preprocess_state_init(FRAME_SIZE, SAMPLE_RATE);
        if (!preprocessState) { std::cerr << "Failed to init preprocess state" << std::endl; return false; }
        int enable = 1, vad = 0, ns = -25;
        speex_preprocess_ctl(preprocessState, SPEEX_PREPROCESS_SET_ECHO_STATE, echoState);
        speex_preprocess_ctl(preprocessState, SPEEX_PREPROCESS_SET_DENOISE, &enable);
        speex_preprocess_ctl(preprocessState, SPEEX_PREPROCESS_SET_AGC, &enable);
        speex_preprocess_ctl(preprocessState, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &ns);
        speex_preprocess_ctl(preprocessState, SPEEX_PREPROCESS_SET_VAD, &vad);

        // Full-duplex (mic + out)
        PaStreamParameters inParams{};
        inParams.device = Pa_GetDefaultInputDevice();
        if (inParams.device == paNoDevice) { std::cerr << "No default input device" << std::endl; return false; }
        inParams.channelCount = 1;
        inParams.sampleFormat = paInt16;
        inParams.suggestedLatency = Pa_GetDeviceInfo(inParams.device)->defaultLowInputLatency;
        inParams.hostApiSpecificStreamInfo = nullptr;

        PaStreamParameters outParams{};
        outParams.device = Pa_GetDefaultOutputDevice();
        if (outParams.device == paNoDevice) { std::cerr << "No default output device" << std::endl; return false; }
        outParams.channelCount = 1;
        outParams.sampleFormat = paInt16;
        outParams.suggestedLatency = Pa_GetDeviceInfo(outParams.device)->defaultLowOutputLatency;
        outParams.hostApiSpecificStreamInfo = nullptr;

        err = Pa_OpenStream(&duplexStream,
                            &inParams,
                            &outParams,
                            SAMPLE_RATE,
                            FRAME_SIZE,
                            paClipOff,
                            &RealtimeAECWithLoopback::duplexCallback,
                            this);
        if (err != paNoError) {
            std::cerr << "OpenStream (duplex) failed: " << Pa_GetErrorText(err) << std::endl; return false;
        }

        // Try to find a loopback-capable input device (common names: "loopback", "Stereo Mix", "What U Hear")
        PaDeviceIndex loopbackDevice = paNoDevice;
        int deviceCount = Pa_GetDeviceCount();
        for (int i = 0; i < deviceCount; ++i) {
            const PaDeviceInfo* di = Pa_GetDeviceInfo(i);
            if (!di) continue;
            const PaHostApiInfo* hai = Pa_GetHostApiInfo(di->hostApi);
            if (!hai || hai->type != paWASAPI) continue;
            if (di->maxInputChannels < 1) continue;
			loopbackDevice = i;

            //std::string name = di->name ? di->name : "";
            //std::string lower;
            //lower.resize(name.size());
            //std::transform(name.begin(), name.end(), lower.begin(), [](unsigned char c){ return (char)std::tolower(c); });
            //if (lower.find("loopback") != std::string::npos ||
            //    lower.find("stereo mix") != std::string::npos ||
            //    lower.find("what u hear") != std::string::npos) {
            //    loopbackDevice = i;
            //    break;
            //}
        }

        if (loopbackDevice == paNoDevice) {
            std::cerr << "Could not find a WASAPI loopback capture device (e.g., 'Speakers (loopback)' or 'Stereo Mix').\n"
                      << "Enable 'Stereo Mix' in Recording devices or use the Mumble-like test tone example instead." << std::endl;
            return false;
        }

        PaStreamParameters loopIn{};
        loopIn.device = loopbackDevice;
        loopIn.channelCount = 1;
        loopIn.sampleFormat = paInt16;
        loopIn.suggestedLatency = Pa_GetDeviceInfo(loopIn.device)->defaultLowInputLatency;
        loopIn.hostApiSpecificStreamInfo = nullptr; // no special flags

        err = Pa_OpenStream(&loopbackStream,
                            &loopIn,
                            nullptr,
                            SAMPLE_RATE,
                            FRAME_SIZE,
                            paClipOff,
                            &RealtimeAECWithLoopback::loopbackCallback,
                            this);
        if (err != paNoError) {
            std::cerr << "OpenStream (loopback) failed: " << Pa_GetErrorText(err) << std::endl; return false;
        }

        return true;
    }

    bool start() {
        if (running) return true;
        PaError err = Pa_StartStream(loopbackStream);
        if (err != paNoError) { std::cerr << "Start loopback failed: " << Pa_GetErrorText(err) << std::endl; return false; }
        err = Pa_StartStream(duplexStream);
        if (err != paNoError) { std::cerr << "Start duplex failed: " << Pa_GetErrorText(err) << std::endl; return false; }
        running = true;
        std::cout << "Realtime AEC (loopback) started. Speak into mic and play audio on speakers.\n";
        return true;
    }

    void stop() {
        if (!running) return;
        running = false;
        if (duplexStream) Pa_StopStream(duplexStream);
        if (loopbackStream) Pa_StopStream(loopbackStream);
        std::cout << "Processed blocks: " << processedBlocks.load() << std::endl;
    }

    void cleanup() {
        stop();
        if (duplexStream) { Pa_CloseStream(duplexStream); duplexStream = nullptr; }
        if (loopbackStream) { Pa_CloseStream(loopbackStream); loopbackStream = nullptr; }
        if (echoState) { speex_echo_state_destroy(echoState); echoState = nullptr; }
        if (preprocessState) { speex_preprocess_state_destroy(preprocessState); preprocessState = nullptr; }
        Pa_Terminate();
    }

private:
    // Capture speaker mix frames into a small queue
    static int loopbackCallback(const void* inputBuffer,
                                void* /*outputBuffer*/,
                                unsigned long framesPerBuffer,
                                const PaStreamCallbackTimeInfo* /*timeInfo*/,
                                PaStreamCallbackFlags /*statusFlags*/,
                                void* userData) {
        auto* self = static_cast<RealtimeAECWithLoopback*>(userData);
        const short* in = static_cast<const short*>(inputBuffer);
        if (!in || framesPerBuffer != FRAME_SIZE) return paContinue;

        std::vector<short> frame(FRAME_SIZE);
        for (unsigned long i = 0; i < framesPerBuffer; ++i) frame[i] = in[i];

        {
            std::lock_guard<std::mutex> lock(self->loopbackMutex);
            self->loopbackQueue.push_back(std::move(frame));
            while (self->loopbackQueue.size() > MAX_LOOPBACK_QUEUE) self->loopbackQueue.pop_front();
        }
        return paContinue;
    }

    // Process mic with latest loopback frame
    static int duplexCallback(const void* inputBuffer,
                              void* outputBuffer,
                              unsigned long framesPerBuffer,
                              const PaStreamCallbackTimeInfo* /*timeInfo*/,
                              PaStreamCallbackFlags /*statusFlags*/,
                              void* userData) {
        auto* self = static_cast<RealtimeAECWithLoopback*>(userData);
        const short* in = static_cast<const short*>(inputBuffer);
        short* out = static_cast<short*>(outputBuffer);
        if (!out || framesPerBuffer != FRAME_SIZE) return paContinue;

        // Mic
        if (in) {
            for (unsigned long i = 0; i < framesPerBuffer; ++i) self->micFrame[i] = in[i];
        } else {
            std::fill(self->micFrame.begin(), self->micFrame.end(), 0);
        }

        // Far-end from loopback (latest frame)
        bool haveFar = false;
        {
            std::lock_guard<std::mutex> lock(self->loopbackMutex);
            if (!self->loopbackQueue.empty()) {
                self->farFrame = self->loopbackQueue.back();
                self->loopbackQueue.clear(); // keep latest only
                haveFar = true;
            }
        }
        if (!haveFar) {
            std::fill(self->farFrame.begin(), self->farFrame.end(), 0);
        }

        // AEC: mic, far, out
        speex_echo_cancellation(self->echoState,
                                self->micFrame.data(),
                                self->farFrame.data(),
                                self->outFrame.data());

        // Preprocess
        speex_preprocess_run(self->preprocessState, self->outFrame.data());

        // Output processed
        for (unsigned long i = 0; i < framesPerBuffer; ++i) out[i] = self->outFrame[i];
        self->processedBlocks.fetch_add(1);
        return paContinue;
    }
};

int main() {
    std::cout << "==========================================\n";
    std::cout << "Realtime AEC with WASAPI Loopback (PortAudio + SpeexDSP)\n";
    std::cout << "- Captures speaker mix as far-end via WASAPI loopback\n";
    std::cout << "- 48kHz mono, 10ms frame, 100ms filter\n";
    std::cout << "Speak while playing audio on your speakers. Press Enter to stop.\n";

    RealtimeAECWithLoopback aec;
    if (!aec.initialize()) return -1;
    if (!aec.start()) return -1;
    std::cin.get();
    aec.stop();
    return 0;
}


