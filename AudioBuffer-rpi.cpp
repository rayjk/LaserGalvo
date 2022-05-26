
#include "AudioBuffer-rpi.h"

// Constructor, caller can give us a hint about the number of samples we may need to hold.
AudioBuffer::AudioBuffer(int iSizeHint, int divisor, int buffSze)
{
//	if (iSizeHint > 0)
//		m_vectorSamples.reserve(iSizeHint);
		this->divisor = divisor;
		this->buffSze = buffSze;
		sampIdx = 0;
		frameIdx = 0;
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

	// Copy all the frames over to our internal array of samples
	for (unsigned long i = 0; i < iFramesPerBuffer; i++){
		//m_vectorSamples.push_back(pData[0][i]);
		if (frameIdx % divisor == 0){
			buff[sampIdx] = pData[0][i];
			sampIdx++;
		}
		frameIdx++;
	}
	if (sampIdx >= buffSze){
		for (int i = 0; i < sampIdx; i++)
			cout << buff[i] << endl;
		sampIdx = 0;
	}
	//return 1;
	return paContinue;
}
