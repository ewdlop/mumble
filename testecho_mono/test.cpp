/**************************************************
Author   : ZL Guo
Email    : 1799580752@qq.com
TechBlog : https://www.jianshu.com/u/b971a1d12a6f
Description:
This program is mainly used for mono echo cancellation testing using speex.
It mainly includes two parts:
1. Echo cancellation for multiple resolutions: 8k 16k 32k 48k
2. Convergence time testing
**************************************************/


#include "targetver.h"
#include <stdio.h>
#include <tchar.h>
#include <windows.h>
#include <direct.h>  // For _mkdir

#ifdef __cplusplus  
extern "C" {    // If referenced by C++ files
#endif

#include "speex/speex_echo.h"
#include "speex/speex_preprocess.h"

#ifdef __cplusplus
}
#endif

enum EPxSampleRateFreq
{
	kePxSampleRateFreq_Invalid = -1,
	kePxSampleRateFreq_8k = 8000,
	kePxSampleRateFreq_16k = 16000,
	kePxSampleRateFreq_32k = 32000,
	kePxSampleRateFreq_48k = 48000,
	kePxSampleRateFreq_Cnt
};

int main(int argc, char** argv)
{
	FILE* echo_fd, * ref_fd, * e_fd;
	SpeexEchoState* st;
	SpeexPreprocessState* den;
	int sampleRate = 48000; // Provided test samples: 8000 16000 32000 48000

	int nFrameSizeInMS = 10;  // How many milliseconds of data to process each time
	int nFilterLenInMS = 120; // Filter length in milliseconds

	SYSTEMTIME sysTime;
	::GetLocalTime(&sysTime);

	int frame_size = (nFrameSizeInMS * sampleRate * 1.0) / 1000;
	int filter_length = (nFilterLenInMS * sampleRate * 1.0) / 1000;

	char szEchoFileName[512] = { 0 };
	char szRefFileName[512] = { 0 };

#if 1
	// test 1: Multi-sampling rate testing
	// The micin and speaker data provided here are completely aligned
	if (kePxSampleRateFreq_8k == sampleRate)
	{
		sprintf(szEchoFileName, "%s", "micin_8k_s16_mono.pcm");
		sprintf(szRefFileName, "%s", "speaker_8k_s16_mono.pcm");
	}
	else if (kePxSampleRateFreq_16k == sampleRate)
	{
		sprintf(szEchoFileName, "%s", "micin_16k_s16_mono.pcm");
		sprintf(szRefFileName, "%s", "speaker_16k_s16_mono.pcm");
	}
	else if (kePxSampleRateFreq_32k == sampleRate)
	{
		sprintf(szEchoFileName, "%s", "micin_32k_s16_mono.pcm");
		sprintf(szRefFileName, "%s", "speaker_32k_s16_mono.pcm");
	}
	else if (kePxSampleRateFreq_48k == sampleRate)
	{
		sprintf(szEchoFileName, "%s", "micin_48k_s16_mono.pcm");
		sprintf(szRefFileName, "%s", "speaker_48k_s16_mono.pcm");
	}

#else 
	// test 2: Test convergence speed
	// Use the same file for testing to see when continuous silence starts outputting, indicating convergence
	if (kePxSampleRateFreq_8k == sampleRate)
	{
		sprintf(szEchoFileName, "%s", "speaker_8k_s16_mono.pcm");
		sprintf(szRefFileName, "%s", "speaker_8k_s16_mono.pcm");
	}
	else if (kePxSampleRateFreq_16k == sampleRate)
	{
		sprintf(szEchoFileName, "%s", "speaker_16k_s16_mono.pcm");
		sprintf(szRefFileName, "%s", "speaker_16k_s16_mono.pcm");
	}
	else if (kePxSampleRateFreq_32k == sampleRate)
	{
		sprintf(szEchoFileName, "%s", "speaker_32k_s16_mono.pcm");
		sprintf(szRefFileName, "%s", "speaker_32k_s16_mono.pcm");
	}
	else if (kePxSampleRateFreq_48k == sampleRate)
	{
		sprintf(szEchoFileName, "%s", "speaker_48k_s16_mono.pcm");
		sprintf(szRefFileName, "%s", "speaker_48k_s16_mono.pcm");
	}
#endif 

	// Create output directory if it doesn't exist
	_mkdir(".\\output");

	char szOutputFileName[512];
	sprintf(szOutputFileName, ".\\output\\mono_%s_%s_%04d%02d%02d_%02d%02d%02d.pcm",
		szEchoFileName, szRefFileName,
		sysTime.wYear, sysTime.wMonth, sysTime.wDay,
		sysTime.wHour, sysTime.wMinute, sysTime.wSecond);
	printf("%s\n", szOutputFileName);

    // Open input files with error checking (echo=mic, ref=speaker)
    echo_fd = fopen(szEchoFileName, "rb");
    if (!echo_fd) {
        printf("Error: Cannot open input file %s\n", szEchoFileName);
        return -1;
    }

    ref_fd = fopen(szRefFileName, "rb");
    if (!ref_fd) {
        printf("Error: Cannot open input file %s\n", szRefFileName);
        fclose(echo_fd);
        return -1;
    }

	// Open output file with error checking
	e_fd = fopen(szOutputFileName, "wb");
	if (!e_fd) {
		printf("Error: Cannot create output file %s\n", szOutputFileName);
		fclose(ref_fd);
		fclose(echo_fd);
		return -1;
	}

	short* echo_buf;
	short* ref_buf;
	short* e_buf;

	echo_buf = new short[frame_size];
	ref_buf = new short[frame_size];
	e_buf = new short[frame_size];

	// Very important function that directly affects convergence speed
	// Smaller TAIL leads to faster convergence, but requires higher synchronization between near-end and far-end data
	// For example, if TAIL is set to 100ms samples, echo cannot be eliminated if near and far data differ by more than 100ms
	// Too large TAIL slows convergence and may cause filter instability
	st = speex_echo_state_init(frame_size, filter_length);

	den = speex_preprocess_state_init(frame_size, sampleRate);

	speex_echo_ctl(st, SPEEX_ECHO_SET_SAMPLING_RATE, &sampleRate);
	speex_preprocess_ctl(den, SPEEX_PREPROCESS_SET_ECHO_STATE, st);

	LARGE_INTEGER timeStartCount;
	LARGE_INTEGER timeEndCount;
	LARGE_INTEGER timeFreq;
	QueryPerformanceFrequency(&timeFreq);
	QueryPerformanceCounter(&timeStartCount);

	while (!feof(ref_fd) && !feof(echo_fd))
	{
		fread(ref_buf, sizeof(short), frame_size, ref_fd);
		fread(echo_buf, sizeof(short), frame_size, echo_fd);

        // ref_buf  : Data obtained from speaker  
        // echo_buf : Data collected from microphone 
        // e_buf    : Data after echo cancellation  
        // speex_echo_cancellation expects (mic, speaker, out)
        speex_echo_cancellation(st, echo_buf, ref_buf, e_buf);
		speex_preprocess_run(den, e_buf);
		fwrite(e_buf, sizeof(short), frame_size, e_fd);
	}

	QueryPerformanceCounter(&timeEndCount);
	double elapsed = (((double)(timeEndCount.QuadPart - timeStartCount.QuadPart) * 1000 / timeFreq.QuadPart));

	printf("AEC Done. TimeDuration: %.2f ms\n", elapsed);

	speex_echo_state_destroy(st);
	speex_preprocess_state_destroy(den);

	fclose(e_fd);
	fclose(echo_fd);
	fclose(ref_fd);

	if (echo_buf)
	{
		delete[] echo_buf;
		echo_buf = NULL;
	}

	if (ref_buf)
	{
		delete[] ref_buf;
		ref_buf = NULL;
	}

	if (e_buf)
	{
		delete[] e_buf;
		e_buf = NULL;
	}

	getchar();

	return 0;
}
