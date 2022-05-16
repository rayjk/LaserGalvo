
#include "AudioBuffer.h"

// Constructor, caller can give us a hint about the number of samples we may need to hold.
AudioBuffer::AudioBuffer(int iSizeHint)
{
	if (iSizeHint > 0)
		m_vectorSamples.reserve(iSizeHint);
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


// Dumpt the samples to a raw file
short * AudioBuffer::GetSamples(unsigned long)
{
	unsigned short vecSize = m_vectorSamples.size();
	short samples[vecSize];
	cout << vecSize << endl;
	short iSample;
	unsigned short idx = 0;
	for (VECTOR_SHORT_ITER iter = m_vectorSamples.begin(); iter != m_vectorSamples.end(); iter++)
	{
		iSample = (short) *iter;
		samples[idx] = iSample;
		idx ++;
		//fout.write((char *) &iSample, sizeof(short));
	}
	m_vectorSamples.clear();
	return samples;
}
