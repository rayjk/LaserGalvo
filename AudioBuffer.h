// Adapted from code by Keith Vertanen.

#ifndef _AUDIO_BUFFER_H_
#define _AUDIO_BUFFER_H_

#include <vector>
#include <iostream>
#include <fstream>

#include "portaudiocpp/PortAudioCpp.hxx"

using namespace std;

typedef vector<short> VECTOR_SHORT;
typedef vector<short>::iterator VECTOR_SHORT_ITER;

class AudioBuffer
{
	public:
		AudioBuffer(int iSizeHint);
		~AudioBuffer();

		int RecordCallback(const void* pInputBuffer,
							void* pOutputBuffer,
							unsigned long iFramesPerBuffer,
							const PaStreamCallbackTimeInfo* timeInfo,
							PaStreamCallbackFlags statusFlags);
	private:
		VECTOR_SHORT		m_vectorSamples;					// Holds the 16-bit mono samples
};

#endif
