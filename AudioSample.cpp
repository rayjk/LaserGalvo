// Example program that uses portaudio to play a tone, record some audio, save
// the audio to a file, then play the audio back.
//
// Uses the C++ bindings.
//
// Adapted from sine_example in the portaudio C++ binding example.
//
// Keith Vertanen

#include <iostream>
#include <cmath>
#include <cassert>
#include <cstddef>
#include "portaudiocpp/PortAudioCpp.hxx"
#include "AudioBuffer.h"

// ---------------------------------------------------------------------
// Some constants:
const double	SAMPLE_RATE			= 8000.0;
const int		FRAMES_PER_BUFFER	= 16;

using namespace std;

// ---------------------------------------------------------------------
// main:
int main(int argc, char* argv[]);
int main(int argc, char* argv[])
{

	try
	{
		char 	chWait;
		int 	iInputDevice = -1;
		int 	iOutputDevice = -1;

		AudioBuffer	objAudioBuffer((int) (SAMPLE_RATE * 1));

		////cout << "Setting up PortAudio..." << endl;

		// Set up the System:
		portaudio::AutoSystem autoSys;
		portaudio::System &sys = portaudio::System::instance();

		if (argc > 1)
		{
			if (isdigit(argv[1][0]))
				iInputDevice 	= atoi(argv[1]);
			else{
				int 	iNumDevices 		= sys.deviceCount();
				int 	iIndex 				= 0;
				string	strDetails			= "";

				for (portaudio::System::DeviceIterator i = sys.devicesBegin(); i != sys.devicesEnd(); ++i)
				{
					strDetails = "";
					if ((*i).isSystemDefaultInputDevice())
						strDetails += ", default input";
					if ((*i).isSystemDefaultOutputDevice())
						strDetails += ", default output";

					cout << (*i).index() << ": " << (*i).name() << ", ";
					cout << "in=" << (*i).maxInputChannels() << " ";
					cout << "out=" << (*i).maxOutputChannels() << ", ";
					cout << (*i).hostApi().name();

					cout << strDetails.c_str() << endl;

					iIndex++;
				}
				return 0;
			}

			//cout << "Using input device index = " << iInputDevice << endl;
			//cout << "Using output device index = " << iOutputDevice << endl;
		}
		else
		{
			//cout << "Using system default input/output devices..." << endl;
			iInputDevice	= sys.defaultInputDevice().index();
		  iOutputDevice	= sys.defaultOutputDevice().index();
		}

		portaudio::DirectionSpecificStreamParameters inParamsRecord(sys.deviceByIndex(iInputDevice), 1, portaudio::INT16, false, sys.deviceByIndex(iInputDevice).defaultLowInputLatency(), NULL);
		portaudio::StreamParameters paramsRecord(inParamsRecord, portaudio::DirectionSpecificStreamParameters::null(), SAMPLE_RATE, FRAMES_PER_BUFFER, paClipOff);
		portaudio::MemFunCallbackStream<AudioBuffer> streamRecord(paramsRecord, objAudioBuffer, &AudioBuffer::RecordCallback);

		streamRecord.start();
		cin.get(chWait);
		streamRecord.stop();

		sys.terminate();

	}
	catch (const portaudio::PaException &e)
	{
		//cout << "A PortAudio error occured: " << e.paErrorText() << endl;
	}
	catch (const portaudio::PaCppException &e)
	{
		//cout << "A PortAudioCpp error occured: " << e.what() << endl;
	}
	catch (const exception &e)
	{
		//cout << "A generic exception occured: " << e.what() << endl;
	}
	catch (...)
	{
		//cout << "An unknown exception occured." << endl;
	}

	return 0;
}
