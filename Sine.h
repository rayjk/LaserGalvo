
#ifndef _SINE_GENERATOR_H_
#define _SINE_GENERATOR_H_

#include <math.h>
#include <assert.h>

#include "portaudiocpp/PortAudioCpp.hxx"

// SineGenerator class:
class SineGenerator
{
 public:
	SineGenerator(int tableSize);
	~SineGenerator();

	int generate(const void *inputBuffer, 
		     void *outputBuffer, 
		     unsigned long framesPerBuffer, 
		     const PaStreamCallbackTimeInfo *timeInfo, 
		     PaStreamCallbackFlags statusFlags);

 private:
	float *table_;
	int tableSize_;
	int leftPhase_;
	int rightPhase_;
};

#endif

