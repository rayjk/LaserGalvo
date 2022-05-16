
// Class that can be used to record, playback and save audio data 
// to a file.  It is designed to be a producer/consumer with the
// portaudio library.
//
// This class expects mono audio in INT16 (short) format.
//
// Copyright 2007 by Keith Vertanen.

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
		int PlaybackCallback(const void* pInputBuffer, 
							void* pOutputBuffer, 
							unsigned long iFramesPerBuffer, 
							const PaStreamCallbackTimeInfo* timeInfo, 
							PaStreamCallbackFlags statusFlags);
		void Clear();
		void WriteToFile(const string& strFilename);
		void ResetPlayback();

	private:
		VECTOR_SHORT		m_vectorSamples;					// Holds the 16-bit mono samples
		VECTOR_SHORT_ITER	m_iPlaybackIter;					// Tracks where we are during playback

};

#endif

