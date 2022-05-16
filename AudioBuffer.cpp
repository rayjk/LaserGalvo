
#include "AudioBuffer.h"

// Constructor, caller can give us a hint about the number of samples we may need to hold.
AudioBuffer::AudioBuffer(int iSizeHint)
{
	if (iSizeHint > 0)
		m_vectorSamples.reserve(iSizeHint);

	m_iPlaybackIter = m_vectorSamples.begin();
}

AudioBuffer::~AudioBuffer()
{
}

int AudioBuffer::RecordCallback(const void* pInputBuffer, 
								void* pOutputBuffer, 
								unsigned long iFramesPerBuffer, 
								const PaStreamCallbackTimeInfo* timeInfo, 
								PaStreamCallbackFlags statusFlags)
{
	short** pData = (short**) pInputBuffer;

	if (pInputBuffer == NULL)
	{
		cout << "AudioBuffer::RecordCallback, input buffer was NULL!" << endl;
		return paContinue;
	}

	// Copy all the frames over to our internal vector of samples
	for (unsigned long i = 0; i < iFramesPerBuffer; i++)
		m_vectorSamples.push_back(pData[0][i]);

	return paContinue;
}

int AudioBuffer::PlaybackCallback(const void* pInputBuffer, 
									void* pOutputBuffer, 
									unsigned long iFramesPerBuffer, 
									const PaStreamCallbackTimeInfo* timeInfo, 
									PaStreamCallbackFlags statusFlags)
{
	short**			pData	= (short**) pOutputBuffer;
	unsigned long	iOutput = 0;

	if (pOutputBuffer == NULL)
	{
		cout << "AudioBuffer::PlaybackCallback was NULL!" << endl;
		return paComplete;
	}

	// Output samples until we either have satified the caller, or we run out
	while (iOutput < iFramesPerBuffer)
	{
		if (m_iPlaybackIter == m_vectorSamples.end())
		{
			// Fill out buffer with zeros
			while (iOutput < iFramesPerBuffer)
			{
				pData[0][iOutput] = (short) 0;
				iOutput++;
			}
			return paComplete;
		}

		pData[0][iOutput] = (short) *m_iPlaybackIter;

		m_iPlaybackIter++;
		iOutput++;
	}

	return paContinue;
}

// Clear out any data in the buffer and prepare for a new recording.
void AudioBuffer::Clear()
{
	m_vectorSamples.clear();
}

// Dumpt the samples to a raw file 
void AudioBuffer::WriteToFile(const string& strFilename)
{
	fstream fout(strFilename.c_str(), ios::out|ios::binary);
	short iSample;
	for (VECTOR_SHORT_ITER iter = m_vectorSamples.begin(); iter != m_vectorSamples.end(); iter++)
	{
		iSample = (short) *iter;
		fout.write((char *) &iSample, sizeof(short));
	}
	fout.close();
}

// Reset the playback index to the start of the samples
void AudioBuffer::ResetPlayback()
{
	m_iPlaybackIter = m_vectorSamples.begin();
}
