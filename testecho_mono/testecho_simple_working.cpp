#include <portaudio.h>
#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <cmath>
#include <string>

// Simple working echo cancellation test
class SimpleWorkingEchoTest {
private:
    // Audio parameters
    static const int SAMPLE_RATE = 48000;
    static const int FRAME_SIZE = 480;  // 10ms at 48kHz
    static const int FILTER_LENGTH = 4800;  // 100ms at 48kHz
    
    // PortAudio stream (full-duplex)
    PaStream* micStream;
    
    // Speex components
    SpeexEchoState* echoState;
    SpeexPreprocessState* preprocessState;
    
    // Control
    std::atomic<bool> running;
    
    // Statistics
    std::atomic<unsigned int> micFrames;
    std::atomic<unsigned int> processedFrames;
    
    // Test tone generator
    std::atomic<double> testTonePhase;
    std::atomic<bool> generateTestTone;
    
public:
    SimpleWorkingEchoTest() : 
        micStream(nullptr),
        echoState(nullptr),
        preprocessState(nullptr),
        running(false),
        micFrames(0),
        processedFrames(0),
        testTonePhase(0.0),
        generateTestTone(true) {
    }
    
    ~SimpleWorkingEchoTest() {
        cleanup();
    }
    
    bool initialize() {
        std::cout << "=== Simple Working Echo Cancellation Test ===" << std::endl;
        
        // Initialize PortAudio
        PaError err = Pa_Initialize();
        if (err != paNoError) {
            std::cerr << "PortAudio initialization failed: " << Pa_GetErrorText(err) << std::endl;
            return false;
        }
        
        // Check audio devices
        int numDevices = Pa_GetDeviceCount();
        std::cout << "Found " << numDevices << " audio devices" << std::endl;
        
        int defaultInput = Pa_GetDefaultInputDevice();
        int defaultOutput = Pa_GetDefaultOutputDevice();
        
        if (defaultInput == paNoDevice) {
            std::cerr << "No default input device found" << std::endl;
            return false;
        }
        if (defaultOutput == paNoDevice) {
            std::cerr << "No default output device found" << std::endl;
            return false;
        }
        
        std::cout << "Using input device: " << defaultInput << std::endl;
        std::cout << "Using output device: " << defaultOutput << std::endl;
        
        // Initialize Speex
        if (!initializeSpeex()) {
            return false;
        }
        
        // Setup audio streams
        if (!setupAudioStreams()) {
            return false;
        }
        
        std::cout << "Initialization completed successfully!" << std::endl;
        return true;
    }
    
private:
    bool initializeSpeex() {
        // Initialize echo cancellation
        echoState = speex_echo_state_init(FRAME_SIZE, FILTER_LENGTH);
        if (!echoState) {
            std::cerr << "Failed to initialize Speex echo state" << std::endl;
            return false;
        }
        
        // Initialize preprocessor
        preprocessState = speex_preprocess_state_init(FRAME_SIZE, SAMPLE_RATE);
        if (!preprocessState) {
            std::cerr << "Failed to initialize Speex preprocessor" << std::endl;
            return false;
        }
        
        // Configure preprocessor
        int enable = 1;
        speex_preprocess_ctl(preprocessState, SPEEX_PREPROCESS_SET_ECHO_STATE, echoState);
        speex_preprocess_ctl(preprocessState, SPEEX_PREPROCESS_SET_AGC, &enable);
        
        int noise_suppress = -25;
        speex_preprocess_ctl(preprocessState, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &noise_suppress);
        
        int vad = 0;  // Disable VAD
        speex_preprocess_ctl(preprocessState, SPEEX_PREPROCESS_SET_VAD, &vad);
        
        // Ensure echo state uses correct sampling rate
        int sampleRate = SAMPLE_RATE;
        speex_echo_ctl(echoState, SPEEX_ECHO_SET_SAMPLING_RATE, &sampleRate);

        return true;
    }
    
    bool setupAudioStreams() {
        // Setup microphone input
        PaStreamParameters micParams;
        micParams.device = Pa_GetDefaultInputDevice();
        micParams.channelCount = 1;
        micParams.sampleFormat = paInt16;
        micParams.suggestedLatency = Pa_GetDeviceInfo(micParams.device)->defaultLowInputLatency;
        micParams.hostApiSpecificStreamInfo = nullptr;
        
        // Setup output (for full-duplex)
        PaStreamParameters outputParams;
        outputParams.device = Pa_GetDefaultOutputDevice();
        outputParams.channelCount = 1;
        outputParams.sampleFormat = paInt16;
        outputParams.suggestedLatency = Pa_GetDeviceInfo(outputParams.device)->defaultLowOutputLatency;
        outputParams.hostApiSpecificStreamInfo = nullptr;

        // Open full-duplex stream
        PaError err = Pa_OpenStream(&micStream,
            &micParams,
            &outputParams,
            SAMPLE_RATE,
            FRAME_SIZE,
            paClipOff,
            micCallback,
            this);
            
        if (err != paNoError) {
            std::cerr << "Failed to open mic stream: " << Pa_GetErrorText(err) << std::endl;
            return false;
        }
        
        return true;
    }
    
public:
    // Microphone callback
    static int micCallback(const void* inputBuffer, void* outputBuffer,
        unsigned long framesPerBuffer,
        const PaStreamCallbackTimeInfo* timeInfo,
        PaStreamCallbackFlags statusFlags,
        void* userData) {
        
        SimpleWorkingEchoTest* test = static_cast<SimpleWorkingEchoTest*>(userData);
        const short* input = static_cast<const short*>(inputBuffer);
        
        if (input && test->echoState && test->preprocessState) {
            // Use short arrays for Speex (not float)
            std::vector<short> micShort(framesPerBuffer);
            std::vector<short> echoShort(framesPerBuffer);
            std::vector<short> outputShort(framesPerBuffer);
            
            // Copy input data
            for (unsigned long i = 0; i < framesPerBuffer; ++i) {
                micShort[i] = input[i];
            }
            
            // Generate test tone for echo reference (but don't play it)
            if (test->generateTestTone) {
                for (unsigned long i = 0; i < framesPerBuffer; ++i) {
                    double phase = test->testTonePhase.load();
                    echoShort[i] = static_cast<short>(0.3 * 32767.0 * sin(2.0 * 3.14159265358979323846 * 1000.0 * phase / SAMPLE_RATE));
                    phase += 1.0;
                    if (phase >= SAMPLE_RATE) phase -= SAMPLE_RATE;
                    test->testTonePhase.store(phase);
                }
            }
            
            // Process echo cancellation (using short arrays)
            speex_echo_cancellation(test->echoState, micShort.data(), echoShort.data(), outputShort.data());
            
            // Apply preprocessor (using short array)
            speex_preprocess_run(test->preprocessState, outputShort.data());
            
            // Copy to output buffer
            short* output = static_cast<short*>(outputBuffer);
            for (unsigned long i = 0; i < framesPerBuffer; ++i) {
                output[i] = outputShort[i];
            }
            
            test->micFrames++;
            test->processedFrames++;
        }
        
        return paContinue;
    }
    
    // Output handled in full-duplex micCallback
    
    bool start() {
        std::cout << "Starting simple working echo test..." << std::endl;
        
        running = true;
        
        // Start microphone stream
        PaError err = Pa_StartStream(micStream);
        if (err != paNoError) {
            std::cerr << "Failed to start mic stream: " << Pa_GetErrorText(err) << std::endl;
            return false;
        }
        
        std::cout << "Simple working echo test running!" << std::endl;
        std::cout << "You should hear your microphone input with echo cancellation applied." << std::endl;
        std::cout << "The echo cancellation should reduce any test tone echo in your mic input." << std::endl;
        std::cout << "Press Enter to stop..." << std::endl;
        
        return true;
    }
    
    void stop() {
        std::cout << "Stopping simple working echo test..." << std::endl;
        
        running = false;
        
        if (micStream) Pa_StopStream(micStream);
        
        std::cout << "Statistics:" << std::endl;
        std::cout << "  Mic frames: " << micFrames << std::endl;
        std::cout << "  Processed frames: " << processedFrames << std::endl;
    }
    
    void cleanup() {
        stop();
        
        if (micStream) {
            Pa_CloseStream(micStream);
            micStream = nullptr;
        }
        if (outputStream) {
            Pa_CloseStream(outputStream);
            outputStream = nullptr;
        }
        
        if (echoState) {
            speex_echo_state_destroy(echoState);
            echoState = nullptr;
        }
        if (preprocessState) {
            speex_preprocess_state_destroy(preprocessState);
            preprocessState = nullptr;
        }
        
        Pa_Terminate();
    }
    
    // Control test tone
    void setTestTone(bool enable) {
        generateTestTone = enable;
        if (enable) {
            std::cout << "Test tone enabled (1kHz sine wave)" << std::endl;
        } else {
            std::cout << "Test tone disabled" << std::endl;
        }
    }
};

int main() {
    SimpleWorkingEchoTest test;
    
    if (!test.initialize()) {
        std::cerr << "Failed to initialize simple working echo test!" << std::endl;
        return -1;
    }
    
    if (!test.start()) {
        std::cerr << "Failed to start simple working echo test!" << std::endl;
        return -1;
    }
    
    // Interactive control
    std::cout << "\nControls:" << std::endl;
    std::cout << "  't' + Enter: Enable test tone" << std::endl;
    std::cout << "  'f' + Enter: Disable test tone" << std::endl;
    std::cout << "  Enter: Stop and exit" << std::endl;
    
    std::string input;
    while (std::getline(std::cin, input)) {
        if (input.empty()) {
            break; // Exit
        } else if (input == "t") {
            test.setTestTone(true);
        } else if (input == "f") {
            test.setTestTone(false);
        }
    }
    
    test.stop();
    return 0;
} 