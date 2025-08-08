//#include <portaudio.h>
//#include <speex/speex_echo.h>
//#include <speex/speex_preprocess.h>
//#include <iostream>
//#include <vector>
//#include <thread>
//#include <atomic>
//#include <chrono>
//#include <string>
//#include <cmath>
//
//// Simple diagnostic echo cancellation test
//class DiagnosticEchoTest {
//private:
//    // Audio parameters
//    static const int SAMPLE_RATE = 48000;
//    static const int FRAME_SIZE = 480;  // 10ms at 48kHz
//    static const int FILTER_LENGTH = 4800;  // 100ms at 48kHz
//    
//    // PortAudio streams
//    PaStream* micStream; // full-duplex stream (input+output)
//    
//    // Speex components
//    SpeexEchoState* echoState;
//    SpeexPreprocessState* preprocessState;
//    
//    // Control
//    std::atomic<bool> running;
//    
//    // Statistics
//    std::atomic<unsigned int> micFrames;
//    std::atomic<unsigned int> processedFrames;
//    
//    // Test tone generator
//    std::atomic<double> testTonePhase;
//    std::atomic<bool> generateTestTone;
//    
//public:
//    DiagnosticEchoTest() : 
//        micStream(nullptr),
//        echoState(nullptr),
//        preprocessState(nullptr),
//        running(false),
//        micFrames(0),
//        processedFrames(0),
//        testTonePhase(0.0),
//        generateTestTone(true) {
//    }
//    
//    ~DiagnosticEchoTest() {
//        cleanup();
//    }
//    
//    bool initialize() {
//        std::cout << "=== Diagnostic Echo Cancellation Test ===" << std::endl;
//        
//        // Test 1: PortAudio initialization
//        std::cout << "1. Testing PortAudio initialization..." << std::endl;
//        PaError err = Pa_Initialize();
//        if (err != paNoError) {
//            std::cerr << "   FAILED: " << Pa_GetErrorText(err) << std::endl;
//            return false;
//        }
//        std::cout << "   SUCCESS: PortAudio initialized" << std::endl;
//        
//        // Test 2: Check audio devices
//        std::cout << "2. Checking audio devices..." << std::endl;
//        int numDevices = Pa_GetDeviceCount();
//        std::cout << "   Found " << numDevices << " audio devices" << std::endl;
//        
//        int defaultInput = Pa_GetDefaultInputDevice();
//        int defaultOutput = Pa_GetDefaultOutputDevice();
//        
//        if (defaultInput == paNoDevice) {
//            std::cerr << "   FAILED: No default input device" << std::endl;
//            return false;
//        }
//        if (defaultOutput == paNoDevice) {
//            std::cerr << "   FAILED: No default output device" << std::endl;
//            return false;
//        }
//        
//        std::cout << "   SUCCESS: Default input device: " << defaultInput << std::endl;
//        std::cout << "   SUCCESS: Default output device: " << defaultOutput << std::endl;
//        
//        // Test 3: Speex initialization
//        std::cout << "3. Testing Speex initialization..." << std::endl;
//        if (!initializeSpeex()) {
//            return false;
//        }
//        std::cout << "   SUCCESS: Speex components initialized" << std::endl;
//        
//        // Test 4: Audio stream setup
//        std::cout << "4. Testing audio stream setup..." << std::endl;
//        if (!setupAudioStreams()) {
//            return false;
//        }
//        std::cout << "   SUCCESS: Audio streams configured" << std::endl;
//        
//        std::cout << "=== All tests passed! ===" << std::endl;
//        return true;
//    }
//    
//private:
//    bool initializeSpeex() {
//        // Initialize echo cancellation
//        echoState = speex_echo_state_init(FRAME_SIZE, FILTER_LENGTH);
//        if (!echoState) {
//            std::cerr << "   FAILED: Could not initialize Speex echo state" << std::endl;
//            return false;
//        }
//        
//        // Initialize preprocessor
//        preprocessState = speex_preprocess_state_init(FRAME_SIZE, SAMPLE_RATE);
//        if (!preprocessState) {
//            std::cerr << "   FAILED: Could not initialize Speex preprocessor" << std::endl;
//            return false;
//        }
//        
//        // Configure preprocessor (like Mumble)
//        int enable = 1;
//        speex_preprocess_ctl(preprocessState, SPEEX_PREPROCESS_SET_ECHO_STATE, echoState);
//        speex_preprocess_ctl(preprocessState, SPEEX_PREPROCESS_SET_AGC, &enable);
//        speex_preprocess_ctl(preprocessState, SPEEX_PREPROCESS_SET_AGC_LEVEL, &enable);
//        
//        int noise_suppress = -25;
//        speex_preprocess_ctl(preprocessState, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &noise_suppress);
//        
//        int vad = 0;  // Disable VAD
//        speex_preprocess_ctl(preprocessState, SPEEX_PREPROCESS_SET_VAD, &vad);
//
//        // Ensure echo state uses correct sampling rate
//        int sr = SAMPLE_RATE;
//        speex_echo_ctl(echoState, SPEEX_ECHO_SET_SAMPLING_RATE, &sr);
//        
//        return true;
//    }
//    
//    bool setupAudioStreams() {
//        // Setup microphone input
//        PaStreamParameters micParams;
//        micParams.device = Pa_GetDefaultInputDevice();
//        micParams.channelCount = 1;
//        micParams.sampleFormat = paInt16;
//        micParams.suggestedLatency = Pa_GetDeviceInfo(micParams.device)->defaultLowInputLatency;
//        micParams.hostApiSpecificStreamInfo = nullptr;
//        
//        // Setup output
//        PaStreamParameters outputParams;
//        outputParams.device = Pa_GetDefaultOutputDevice();
//        outputParams.channelCount = 1;
//        outputParams.sampleFormat = paInt16;
//        outputParams.suggestedLatency = Pa_GetDeviceInfo(outputParams.device)->defaultLowOutputLatency;
//        outputParams.hostApiSpecificStreamInfo = nullptr;
//        
//        // Open full-duplex stream
//        PaError err = Pa_OpenStream(&micStream,
//            &micParams,
//            &outputParams,
//            SAMPLE_RATE,
//            FRAME_SIZE,
//            paClipOff,
//            micCallback,
//            this);
//            
//        if (err != paNoError) {
//            std::cerr << "   FAILED: Could not open mic stream: " << Pa_GetErrorText(err) << std::endl;
//            return false;
//        }
//        
//        return true;
//    }
//    
//public:
//    // Microphone callback
//    static int micCallback(const void* inputBuffer, void* outputBuffer,
//        unsigned long framesPerBuffer,
//        const PaStreamCallbackTimeInfo* timeInfo,
//        PaStreamCallbackFlags statusFlags,
//        void* userData) {
//        
//        DiagnosticEchoTest* test = static_cast<DiagnosticEchoTest*>(userData);
//        const short* input = static_cast<const short*>(inputBuffer);
//        short* out = static_cast<short*>(outputBuffer);
//        
//        if (input && out && test->echoState && test->preprocessState) {
//            // Use short arrays for Speex (not float)
//            std::vector<short> micShort(framesPerBuffer);
//            std::vector<short> echoShort(framesPerBuffer);
//            std::vector<short> outputShort(framesPerBuffer);
//            
//            // Copy input data
//            for (unsigned long i = 0; i < framesPerBuffer; ++i) {
//                micShort[i] = input[i];
//            }
//            
//            // Generate test tone for echo reference
//            if (test->generateTestTone) {
//                for (unsigned long i = 0; i < framesPerBuffer; ++i) {
//                    double phase = test->testTonePhase.load();
//                    echoShort[i] = static_cast<short>(0.3 * 32767.0 * sin(2.0 * 3.14159265358979323846 * 1000.0 * phase / SAMPLE_RATE));
//                    phase += 1.0;
//                    if (phase >= SAMPLE_RATE) phase -= SAMPLE_RATE;
//                    test->testTonePhase.store(phase);
//                }
//            }
//            
//            // Process echo cancellation (mic, speaker, out)
//            speex_echo_cancellation(test->echoState, micShort.data(), echoShort.data(), outputShort.data());
//            
//            // Apply preprocessor (using short array)
//            speex_preprocess_run(test->preprocessState, outputShort.data());
//            
//            // Copy to output buffer (play processed audio)
//            for (unsigned long i = 0; i < framesPerBuffer; ++i) {
//                out[i] = outputShort[i];
//            }
//            
//            test->micFrames++;
//            test->processedFrames++;
//        }
//        
//        return paContinue;
//    }
//    
//    // Output handled by full-duplex micCallback
//    
//    bool start() {
//        std::cout << "Starting diagnostic test..." << std::endl;
//        
//        running = true;
//        
//        // Start microphone stream
//        PaError err = Pa_StartStream(micStream);
//        if (err != paNoError) {
//            std::cerr << "Failed to start mic stream: " << Pa_GetErrorText(err) << std::endl;
//            return false;
//        }
//        
//        std::cout << "Diagnostic test running!" << std::endl;
//        std::cout << "Speak into your microphone to test echo cancellation." << std::endl;
//        std::cout << "Press Enter to stop..." << std::endl;
//        
//        return true;
//    }
//    
//    void stop() {
//        std::cout << "Stopping diagnostic test..." << std::endl;
//        
//        running = false;
//        
//        if (micStream) Pa_StopStream(micStream);
//        
//        std::cout << "Statistics:" << std::endl;
//        std::cout << "  Mic frames: " << micFrames << std::endl;
//        std::cout << "  Processed frames: " << processedFrames << std::endl;
//    }
//    
//    void cleanup() {
//        stop();
//        
//        if (micStream) {
//            Pa_CloseStream(micStream);
//            micStream = nullptr;
//        }
//        // full-duplex single stream only
//        
//        if (echoState) {
//            speex_echo_state_destroy(echoState);
//            echoState = nullptr;
//        }
//        if (preprocessState) {
//            speex_preprocess_state_destroy(preprocessState);
//            preprocessState = nullptr;
//        }
//        
//        Pa_Terminate();
//    }
//};
//
//int main() {
//    DiagnosticEchoTest test;
//    
//    if (!test.initialize()) {
//        std::cerr << "Diagnostic test failed to initialize!" << std::endl;
//        return -1;
//    }
//    
//    if (!test.start()) {
//        std::cerr << "Diagnostic test failed to start!" << std::endl;
//        return -1;
//    }
//    
//    // Wait for user input
//    std::string input;
//    std::getline(std::cin, input);
//    
//    test.stop();
//    return 0;
//} 