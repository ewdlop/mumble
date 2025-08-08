//#include "MumbleEchoProcessor.h"
//#include <portaudio.h>
//#include <iostream>
//#include <vector>
//#include <thread>
//#include <atomic>
//#include <chrono>
//#include <string>
//
//// Define M_PI if not defined
//#ifndef M_PI
//#define M_PI 3.14159265358979323846
//#endif
//
//// Realistic echo cancellation test using actual speaker output
//class RealisticMumbleEchoTest {
//private:
//    // PortAudio streams
//    PaStream* micStream;
//    PaStream* speakerStream;  // Loopback stream to capture speaker output
//    PaStream* outputStream;   // Stream to play processed audio
//
//    // Mumble's exact echo processor
//    MumbleEchoProcessor* echoProcessor;
//
//    // Audio buffers
//    std::vector<short> micBuffer;
//    std::vector<short> speakerBuffer;
//    std::vector<short> outputBuffer;
//
//    // Control
//    std::atomic<bool> running;
//
//    // Statistics
//    std::atomic<unsigned int> processedFrames;
//    std::atomic<unsigned int> droppedFrames;
//    std::atomic<unsigned int> micFrames;
//    std::atomic<unsigned int> speakerFrames;
//
//    // Test tone generator for realistic testing
//    std::atomic<double> testTonePhase;
//    std::atomic<bool> generateTestTone;
//
//public:
//    RealisticMumbleEchoTest() : 
//        micStream(nullptr), 
//        speakerStream(nullptr),
//        outputStream(nullptr),
//        echoProcessor(nullptr),
//        running(false), 
//        processedFrames(0), 
//        droppedFrames(0),
//        micFrames(0),
//        speakerFrames(0),
//        testTonePhase(0.0),
//        generateTestTone(true) {
//        
//        micBuffer.resize(FRAME_SIZE);
//        speakerBuffer.resize(FRAME_SIZE);
//        outputBuffer.resize(FRAME_SIZE);
//    }
//
//    ~RealisticMumbleEchoTest() {
//        cleanup();
//    }
//
//    bool initialize() {
//        std::cout << "Initializing Realistic Mumble Echo Test..." << std::endl;
//
//        // Initialize PortAudio
//        PaError err = Pa_Initialize();
//        if (err != paNoError) {
//            std::cerr << "PortAudio initialization failed: " << Pa_GetErrorText(err) << std::endl;
//            return false;
//        }
//
//        // Create Mumble's exact echo processor
//        echoProcessor = new MumbleEchoProcessor();
//        echoProcessor->setDebugOutput(true);
//
//        if (!echoProcessor->initialize()) {
//            std::cerr << "Failed to initialize Mumble echo processor" << std::endl;
//            return false;
//        }
//
//        // Setup audio streams
//        if (!setupAudioStreams()) {
//            return false;
//        }
//
//        std::cout << "Realistic Mumble Echo Test initialized successfully!" << std::endl;
//        std::cout << "This will generate a test tone and capture it for echo cancellation" << std::endl;
//        return true;
//    }
//
//private:
//    bool setupAudioStreams() {
//        // Setup microphone input stream
//        PaStreamParameters micParams;
//        micParams.device = Pa_GetDefaultInputDevice();
//        if (micParams.device == paNoDevice) {
//            std::cerr << "No default input device found" << std::endl;
//            return false;
//        }
//
//        micParams.channelCount = 1;  // Mono like Mumble
//        micParams.sampleFormat = paInt16;
//        micParams.suggestedLatency = Pa_GetDeviceInfo(micParams.device)->defaultLowInputLatency;
//        micParams.hostApiSpecificStreamInfo = nullptr;
//
//        // Setup speaker loopback stream (to capture what's being played)
//        PaStreamParameters speakerParams;
//        speakerParams.device = Pa_GetDefaultOutputDevice();
//        if (speakerParams.device == paNoDevice) {
//            std::cerr << "No default output device found" << std::endl;
//            return false;
//        }
//
//        speakerParams.channelCount = 1;  // Mono like Mumble
//        speakerParams.sampleFormat = paInt16;
//        speakerParams.suggestedLatency = Pa_GetDeviceInfo(speakerParams.device)->defaultLowOutputLatency;
//        speakerParams.hostApiSpecificStreamInfo = nullptr;
//
//        // Setup output stream for processed audio
//        PaStreamParameters outputParams;
//        outputParams.device = Pa_GetDefaultOutputDevice();
//        if (outputParams.device == paNoDevice) {
//            std::cerr << "No default output device found" << std::endl;
//            return false;
//        }
//
//        outputParams.channelCount = 1;  // Mono like Mumble
//        outputParams.sampleFormat = paInt16;
//        outputParams.suggestedLatency = Pa_GetDeviceInfo(outputParams.device)->defaultLowOutputLatency;
//        outputParams.hostApiSpecificStreamInfo = nullptr;
//
//        // Open microphone stream
//        PaError err = Pa_OpenStream(&micStream,
//            &micParams,
//            nullptr,
//            SAMPLE_RATE,
//            FRAME_SIZE,
//            paClipOff,
//            micCallback,
//            this);
//
//        if (err != paNoError) {
//            std::cerr << "Failed to open mic stream: " << Pa_GetErrorText(err) << std::endl;
//            return false;
//        }
//
//        // Try to open speaker loopback stream
//        err = Pa_OpenStream(&speakerStream,
//            nullptr,
//            &speakerParams,
//            SAMPLE_RATE,
//            FRAME_SIZE,
//            paClipOff,
//            speakerCallback,
//            this);
//
//        if (err != paNoError) {
//            std::cerr << "Failed to open speaker loopback stream: " << Pa_GetErrorText(err) << std::endl;
//            std::cerr << "Falling back to dummy speaker data..." << std::endl;
//            speakerStream = nullptr;
//        } else {
//            std::cout << "Speaker loopback stream opened successfully!" << std::endl;
//        }
//
//        // Open output stream for processed audio
//        err = Pa_OpenStream(&outputStream,
//            nullptr,
//            &outputParams,
//            SAMPLE_RATE,
//            FRAME_SIZE,
//            paClipOff,
//            outputCallback,
//            this);
//
//        if (err != paNoError) {
//            std::cerr << "Failed to open output stream: " << Pa_GetErrorText(err) << std::endl;
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
//        RealisticMumbleEchoTest* test = static_cast<RealisticMumbleEchoTest*>(userData);
//        const short* input = static_cast<const short*>(inputBuffer);
//
//        if (input && test->echoProcessor) {
//            // Use Mumble's exact addMic method
//            test->echoProcessor->addMic(input, framesPerBuffer);
//            test->micFrames++;
//        }
//
//        return paContinue;
//    }
//
//    // Speaker loopback callback
//    static int speakerCallback(const void* inputBuffer, void* outputBuffer,
//        unsigned long framesPerBuffer,
//        const PaStreamCallbackTimeInfo* timeInfo,
//        PaStreamCallbackFlags statusFlags,
//        void* userData) {
//
//        RealisticMumbleEchoTest* test = static_cast<RealisticMumbleEchoTest*>(userData);
//        short* output = static_cast<short*>(outputBuffer);
//
//        // Generate test tone (1kHz sine wave)
//        if (test->generateTestTone) {
//            const double frequency = 1000.0; // 1kHz test tone
//            const double amplitude = 0.3;    // 30% volume to avoid feedback
//            const double sampleRate = SAMPLE_RATE;
//            
//            // Generate test tone data for echo cancellation (but don't play it)
//            std::vector<short> testToneData(framesPerBuffer);
//            for (unsigned long i = 0; i < framesPerBuffer; ++i) {
//                double phase = test->testTonePhase.load();
//                testToneData[i] = static_cast<short>(amplitude * 32767.0 * sin(2.0 * M_PI * frequency * phase / sampleRate));
//                phase += 1.0;
//                if (phase >= sampleRate) phase -= sampleRate;
//                test->testTonePhase.store(phase);
//            }
//
//            // Send the test tone to echo processor for cancellation
//            if (test->echoProcessor) {
//                test->echoProcessor->addEcho(testToneData.data(), framesPerBuffer);
//                test->speakerFrames++;
//            }
//
//            // Output silence instead of test tone (commented out speaker output)
//            std::fill(output, output + framesPerBuffer, 0);
//        } else {
//            // Output silence
//            std::fill(output, output + framesPerBuffer, 0);
//        }
//
//        return paContinue;
//    }
//
//    // Output callback for processed audio
//    static int outputCallback(const void* inputBuffer, void* outputBuffer,
//        unsigned long framesPerBuffer,
//        const PaStreamCallbackTimeInfo* timeInfo,
//        PaStreamCallbackFlags statusFlags,
//        void* userData) {
//
//        RealisticMumbleEchoTest* test = static_cast<RealisticMumbleEchoTest*>(userData);
//        short* output = static_cast<short*>(outputBuffer);
//
//        if (test->echoProcessor) {
//            // Get processed audio from Mumble's processor
//            if (test->echoProcessor->getProcessedAudio(output, framesPerBuffer)) {
//                test->processedFrames++;
//            } else {
//                // Output silence if no processed audio available
//                std::fill(output, output + framesPerBuffer, 0);
//            }
//        } else {
//            // Output silence
//            std::fill(output, output + framesPerBuffer, 0);
//        }
//
//        return paContinue;
//    }
//
//    bool start() {
//        std::cout << "Starting realistic Mumble echo test..." << std::endl;
//
//        running = true;
//
//        // Start microphone stream
//        PaError err = Pa_StartStream(micStream);
//        if (err != paNoError) {
//            std::cerr << "Failed to start mic stream: " << Pa_GetErrorText(err) << std::endl;
//            running = false;
//            return false;
//        }
//
//        // Start speaker loopback stream if available
//        if (speakerStream) {
//            err = Pa_StartStream(speakerStream);
//            if (err != paNoError) {
//                std::cerr << "Failed to start speaker stream: " << Pa_GetErrorText(err) << std::endl;
//                running = false;
//                Pa_StopStream(micStream);
//                return false;
//            }
//            std::cout << "Speaker loopback started - generating 1kHz test tone" << std::endl;
//        } else {
//            std::cout << "No speaker loopback - using dummy speaker data" << std::endl;
//            // Start a thread to generate dummy speaker data
//            std::thread([this]() {
//                while (running) {
//                    std::vector<short> dummySpeaker(FRAME_SIZE, 0);
//                    // Generate some dummy speaker data (silence for now)
//                    if (echoProcessor) {
//                        echoProcessor->addEcho(dummySpeaker.data(), FRAME_SIZE);
//                    }
//                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
//                }
//            }).detach();
//        }
//
//        // Start output stream
//        err = Pa_StartStream(outputStream);
//        if (err != paNoError) {
//            std::cerr << "Failed to start output stream: " << Pa_GetErrorText(err) << std::endl;
//            running = false;
//            Pa_StopStream(micStream);
//            if (speakerStream) Pa_StopStream(speakerStream);
//            return false;
//        }
//
//        std::cout << "Realistic Mumble echo test started successfully!" << std::endl;
//        std::cout << "You should hear:" << std::endl;
//        std::cout << "1. Your microphone input with echo cancellation applied" << std::endl;
//        std::cout << "2. The echo cancellation should reduce any test tone echo in your mic input" << std::endl;
//        std::cout << "Press Enter to stop..." << std::endl;
//
//        return true;
//    }
//
//    void stop() {
//        std::cout << "Stopping realistic Mumble echo test..." << std::endl;
//
//        running = false;
//
//        if (micStream) Pa_StopStream(micStream);
//        if (speakerStream) Pa_StopStream(speakerStream);
//        if (outputStream) Pa_StopStream(outputStream);
//
//        std::cout << "Realistic Mumble echo test stopped." << std::endl;
//        std::cout << "Statistics:" << std::endl;
//        std::cout << "  Mic frames: " << micFrames << std::endl;
//        std::cout << "  Speaker frames: " << speakerFrames << std::endl;
//        std::cout << "  Processed frames: " << processedFrames << std::endl;
//        std::cout << "  Dropped frames: " << droppedFrames << std::endl;
//    }
//
//    void cleanup() {
//        stop();
//
//        // Close streams
//        if (micStream) {
//            Pa_CloseStream(micStream);
//            micStream = nullptr;
//        }
//        if (speakerStream) {
//            Pa_CloseStream(speakerStream);
//            speakerStream = nullptr;
//        }
//        if (outputStream) {
//            Pa_CloseStream(outputStream);
//            outputStream = nullptr;
//        }
//
//        // Cleanup echo processor
//        if (echoProcessor) {
//            delete echoProcessor;
//            echoProcessor = nullptr;
//        }
//
//        // Terminate PortAudio
//        Pa_Terminate();
//    }
//
//    // Control test tone
//    void setTestTone(bool enable) {
//        generateTestTone = enable;
//        if (enable) {
//            std::cout << "Test tone enabled (1kHz sine wave)" << std::endl;
//        } else {
//            std::cout << "Test tone disabled" << std::endl;
//        }
//    }
//};
//
//int main() {
//    std::cout << "=========================================" << std::endl;
//    std::cout << "Realistic Mumble Echo Cancellation Test" << std::endl;
//    std::cout << "Using Actual Speaker Output for Echo Cancellation" << std::endl;
//    std::cout << "=========================================" << std::endl;
//
//    RealisticMumbleEchoTest test;
//
//    if (!test.initialize()) {
//        std::cerr << "Failed to initialize realistic Mumble echo test" << std::endl;
//        return -1;
//    }
//
//    if (!test.start()) {
//        std::cerr << "Failed to start realistic Mumble echo test" << std::endl;
//        return -1;
//    }
//
//    // Interactive control
//    std::cout << "\nControls:" << std::endl;
//    std::cout << "  't' + Enter: Toggle test tone" << std::endl;
//    std::cout << "  Enter: Stop and exit" << std::endl;
//
//    std::string input;
//    while (std::getline(std::cin, input)) {
//        if (input.empty()) {
//            break; // Exit
//        } else if (input == "t") {
//            test.setTestTone(true);
//        } else if (input == "f") {
//            test.setTestTone(false);
//        }
//    }
//
//    test.stop();
//    return 0;
//} 