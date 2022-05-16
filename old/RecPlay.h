#ifndef _REC_PLAY_H_
#define _REC_PLAY_H_

#include <iostream>
#include <cmath>
#include <cassert>
#include <cstddef>
#include <chrono>
#include <thread>
#include "portaudiocpp/PortAudioCpp.hxx"
#include "Sine.h"
#include "AudioBuffer.h"

const int		BEEP_SECONDS		= 1;
const double	SAMPLE_RATE			= 16000.0;
const int		FRAMES_PER_BUFFER	= 64;
const int		TABLE_SIZE			= 200;

using namespace std;

class RecPlay
{
	public:
		RecPlay();
		~RecPlay();
		int AudioSetup(int argc, char* argv[]);
		short* RetSamples();
};
#endif
