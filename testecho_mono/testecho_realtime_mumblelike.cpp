//#include <portaudio.h>
//#include <speex/speex_echo.h>
//#include <speex/speex_preprocess.h>
//#include <cmath>
//#include <vector>
//#include <atomic>
//#include <iostream>
//
//
//#ifndef M_PI
//#	define M_PI 3.14159265358979323846
//#endif
//
//// Mumble-like realtime echo cancellation example (full-duplex PortAudio + SpeexDSP)
//// - 48kHz mono, 10ms frames, 100ms filter
//// - Generates a far-end test tone as echo reference (can be mixed to speakers)
//// - Processes mic input with Speex echo canceller + preprocessor, outputs processed audio
//
//class MumbleLikeRealtimeAEC {
//private:
//    static const int SAMPLE_RATE = 48000;
//    static const int FRAME_SIZE = 480;     // 10ms at 48kHz
//    static const int FILTER_LENGTH = 4800; // 100ms at 48kHz
//
//    // PortAudio full-duplex stream
//    PaStream* duplexStream;
//
//    // Speex states
//    SpeexEchoState* echoState;
//    SpeexPreprocessState* preprocessState;
//
//    // Buffers reused in callback
//    std::vector<short> micFrame;
//    std::vector<short> echoRefFrame;
//    std::vector<short> processedFrame;
//
//    // Control and stats
//    std::atomic<bool> running;
//    std::atomic<unsigned long> processedBlocks;
//
//    // Far-end tone generator (echo reference)
//    std::atomic<double> tonePhase;
//    std::atomic<double> toneFrequency;  // Hz
//    std::atomic<double> toneAmplitude;  // 0.0 - 1.0
//    std::atomic<bool>   mixToneToSpeakers; // whether the far-end tone is audible
//
//public:
//    MumbleLikeRealtimeAEC()
//        : duplexStream(nullptr),
//          echoState(nullptr),
//          preprocessState(nullptr),
//          running(false),
//          processedBlocks(0),
//          tonePhase(0.0),
//          toneFrequency(1000.0), // 1 kHz
//          toneAmplitude(0.25),   // 25% to avoid feedback
//          mixToneToSpeakers(true) {
//        micFrame.resize(FRAME_SIZE);
//        echoRefFrame.resize(FRAME_SIZE);
//        processedFrame.resize(FRAME_SIZE);
//    }
//
//    ~MumbleLikeRealtimeAEC() {
//        cleanup();
//    }
//
//    bool initialize() {
//        PaError err = Pa_Initialize();
//        if (err != paNoError) {
//            std::cerr << "PortAudio init failed: " << Pa_GetErrorText(err) << std::endl;
//            return false;
//        }
//
//        // Initialize Speex echo canceller and preprocessor (Mumble-like settings)
//        echoState = speex_echo_state_init(FRAME_SIZE, FILTER_LENGTH);
//        if (!echoState) {
//            std::cerr << "Failed to init Speex echo state" << std::endl;
//            return false;
//        }
//
//        int sr = SAMPLE_RATE;
//        speex_echo_ctl(echoState, SPEEX_ECHO_SET_SAMPLING_RATE, &sr);
//
//        preprocessState = speex_preprocess_state_init(FRAME_SIZE, SAMPLE_RATE);
//        if (!preprocessState) {
//            std::cerr << "Failed to init Speex preprocess state" << std::endl;
//            return false;
//        }
//
//        // Link echo canceller to preprocessor and configure
//        int enable = 1;
//        int vad = 0; // disable VAD to avoid gating for demo
//        int noiseSuppressDb = -25; // dB
//        speex_preprocess_ctl(preprocessState, SPEEX_PREPROCESS_SET_ECHO_STATE, echoState);
//        speex_preprocess_ctl(preprocessState, SPEEX_PREPROCESS_SET_DENOISE, &enable);
//        speex_preprocess_ctl(preprocessState, SPEEX_PREPROCESS_SET_AGC, &enable);
//        speex_preprocess_ctl(preprocessState, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &noiseSuppressDb);
//        speex_preprocess_ctl(preprocessState, SPEEX_PREPROCESS_SET_VAD, &vad);
//
//        // Prepare PortAudio full-duplex stream
//        PaStreamParameters inParams{};
//        inParams.device = Pa_GetDefaultInputDevice();
//        if (inParams.device == paNoDevice) {
//            std::cerr << "No default input device" << std::endl;
//            return false;
//        }
//        inParams.channelCount = 1;
//        inParams.sampleFormat = paInt16;
//        inParams.suggestedLatency = Pa_GetDeviceInfo(inParams.device)->defaultLowInputLatency;
//        inParams.hostApiSpecificStreamInfo = nullptr;
//
//        PaStreamParameters outParams{};
//        outParams.device = Pa_GetDefaultOutputDevice();
//        if (outParams.device == paNoDevice) {
//            std::cerr << "No default output device" << std::endl;
//            return false;
//        }
//        outParams.channelCount = 1;
//        outParams.sampleFormat = paInt16;
//        outParams.suggestedLatency = Pa_GetDeviceInfo(outParams.device)->defaultLowOutputLatency;
//        outParams.hostApiSpecificStreamInfo = nullptr;
//
//        err = Pa_OpenStream(&duplexStream,
//                            &inParams,
//                            &outParams,
//                            SAMPLE_RATE,
//                            FRAME_SIZE,
//                            paClipOff,
//                            &MumbleLikeRealtimeAEC::paCallback,
//                            this);
//        if (err != paNoError) {
//            std::cerr << "OpenStream failed: " << Pa_GetErrorText(err) << std::endl;
//            return false;
//        }
//
//        return true;
//    }
//
//    bool start() {
//        if (running) return true;
//        PaError err = Pa_StartStream(duplexStream);
//        if (err != paNoError) {
//            std::cerr << "StartStream failed: " << Pa_GetErrorText(err) << std::endl;
//            return false;
//        }
//        running = true;
//        std::cout << "Realtime AEC started (48kHz mono, 10ms)\n"
//                  << "- Far-end tone: " << toneFrequency.load() << " Hz at " << (toneAmplitude.load()*100) << "%\n"
//                  << "- Mix tone to speakers: " << (mixToneToSpeakers.load() ? "yes" : "no") << std::endl;
//        return true;
//    }
//
//    void stop() {
//        if (!running) return;
//        running = false;
//        if (duplexStream) Pa_StopStream(duplexStream);
//        std::cout << "Processed blocks: " << processedBlocks.load() << std::endl;
//    }
//
//    void cleanup() {
//        stop();
//        if (duplexStream) {
//            Pa_CloseStream(duplexStream);
//            duplexStream = nullptr;
//        }
//        if (echoState) {
//            speex_echo_state_destroy(echoState);
//            echoState = nullptr;
//        }
//        if (preprocessState) {
//            speex_preprocess_state_destroy(preprocessState);
//            preprocessState = nullptr;
//        }
//        Pa_Terminate();
//    }
//
//    void setMixToneToSpeakers(bool enable) { mixToneToSpeakers = enable; }
//    void setToneFrequency(double hz) { toneFrequency = hz; }
//    void setToneAmplitude(double amp01) { toneAmplitude = amp01; }
//
//private:
//    static int paCallback(const void* inputBuffer,
//                          void* outputBuffer,
//                          unsigned long framesPerBuffer,
//                          const PaStreamCallbackTimeInfo* /*timeInfo*/,
//                          PaStreamCallbackFlags /*statusFlags*/,
//                          void* userData) {
//        auto* self = static_cast<MumbleLikeRealtimeAEC*>(userData);
//        const short* in = static_cast<const short*>(inputBuffer);
//        short* out = static_cast<short*>(outputBuffer);
//
//        // Guard: ensure buffers sized as expected
//        if (framesPerBuffer != FRAME_SIZE || !out) {
//            return paContinue;
//        }
//
//        // Copy mic input (or zeros if null)
//        if (in) {
//            for (unsigned long i = 0; i < framesPerBuffer; ++i) self->micFrame[i] = in[i];
//        } else {
//            std::fill(self->micFrame.begin(), self->micFrame.end(), 0);
//        }
//
//        // Generate far-end reference tone (echoRefFrame)
//        double freq = self->toneFrequency.load();
//        double amp = self->toneAmplitude.load();
//        double phase = self->tonePhase.load();
//        for (unsigned long i = 0; i < framesPerBuffer; ++i) {
//            double s = amp * 32767.0 * std::sin(2.0 * M_PI * freq * phase / SAMPLE_RATE);
//            self->echoRefFrame[i] = static_cast<short>(s);
//            phase += 1.0;
//            if (phase >= SAMPLE_RATE) phase -= SAMPLE_RATE;
//        }
//        self->tonePhase.store(phase);
//
//        // Echo cancellation: mic (near-end), echoRef (far-end), -> processed
//        speex_echo_cancellation(self->echoState,
//                                self->micFrame.data(),
//                                self->echoRefFrame.data(),
//                                self->processedFrame.data());
//
//        // Preprocess (denoise, agc, etc.)
//        speex_preprocess_run(self->preprocessState, self->processedFrame.data());
//
//        // Mix output: processed mic +/- optional far-end tone for audible reference
//        if (self->mixToneToSpeakers.load()) {
//            for (unsigned long i = 0; i < framesPerBuffer; ++i) {
//                int v = static_cast<int>(self->processedFrame[i]) + static_cast<int>(self->echoRefFrame[i]);
//                if (v > 32767) v = 32767; else if (v < -32768) v = -32768;
//                out[i] = static_cast<short>(v);
//            }
//        } else {
//            for (unsigned long i = 0; i < framesPerBuffer; ++i) out[i] = self->processedFrame[i];
//        }
//
//        self->processedBlocks.fetch_add(1);
//        return paContinue;
//    }
//};
//
//int main() {
//    std::cout << "==========================================\n";
//    std::cout << "Mumble-like Realtime AEC (PortAudio + SpeexDSP)\n";
//    std::cout << "- 48kHz mono, 10ms frames, 100ms filter\n";
//    std::cout << "- Far-end test tone as echo reference (toggle audible mix)\n";
//    std::cout << "Press Enter to stop...\n";
//
//    MumbleLikeRealtimeAEC aec;
//    if (!aec.initialize()) return -1;
//
//    // Optional: adjust tone/mix
//    aec.setMixToneToSpeakers(false); // set false if you don't want to hear the test tone
//    // aec.setToneFrequency(750.0);
//    // aec.setToneAmplitude(0.2);
//
//    if (!aec.start()) return -1;
//
//    std::cin.get();
//    aec.stop();
//    return 0;
//}
//
//
