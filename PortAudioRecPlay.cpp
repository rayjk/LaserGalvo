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
#include "Sine.h"
#include "AudioBuffer.h"

// ---------------------------------------------------------------------
// Some constants:
const int		BEEP_SECONDS		= 1;
const double	SAMPLE_RATE			= 16000.0;
const int		FRAMES_PER_BUFFER	= 64;
const int		TABLE_SIZE			= 200;

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

		// Create a SineGenerator object:
		SineGenerator sineGenerator(TABLE_SIZE);

		// Create a object that is used to record, save to file and play the audio.
		AudioBuffer	objAudioBuffer((int) (SAMPLE_RATE * 60));

		cout << "Setting up PortAudio..." << endl;

		// Set up the System:
		portaudio::AutoSystem autoSys;
		portaudio::System &sys = portaudio::System::instance();

		if (argc > 2)
		{	
			iInputDevice 	= atoi(argv[1]); 
			iOutputDevice 	= atoi(argv[2]); 

			cout << "Using input device index = " << iInputDevice << endl;
			cout << "Using output device index = " << iOutputDevice << endl;
		}
		else
		{
			cout << "Using system default input/output devices..." << endl;		  
			iInputDevice	= sys.defaultInputDevice().index();
			iOutputDevice	= sys.defaultOutputDevice().index();
		}


		// List out all the devices we have
		int 	iNumDevices 		= sys.deviceCount();
		int 	iIndex 				= 0;			
		string	strDetails			= "";

		std::cout << "Number of devices = " << iNumDevices << std::endl;		
		if ((iInputDevice >= 0) && (iInputDevice >= iNumDevices))
		{
			cout << "Input device index out of range!" << endl;
			return 0;
		}
		if ((iOutputDevice >= 0) && (iOutputDevice >= iNumDevices))
		{
			cout << "Ouput device index out of range!" << endl;
			return 0;
		}


		//		portaudio::Device inDevice = portaudio::Device(sys.defaultInputDevice());

		//		portaudio::Device& inDevice 	= sys.deviceByIndex(iInputDevice);
		//portaudio::Device& outDevice 	= sys.deviceByIndex(iOutputDevice);

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


		// On linux, it wasn't possible to open two output streams
		// at the same time, so we'll create a sine wave tone 
		// output stream for the start of the recording, then let
		// it scope out so we can create a playback stream.
		{

			// Set up the parameters required to open a (Callback)Stream:
			portaudio::DirectionSpecificStreamParameters outParamsBeep(sys.deviceByIndex(iOutputDevice), 2, portaudio::FLOAT32, false, sys.deviceByIndex(iOutputDevice).defaultLowOutputLatency(), NULL);
			portaudio::StreamParameters paramsBeep(portaudio::DirectionSpecificStreamParameters::null(), outParamsBeep, SAMPLE_RATE, FRAMES_PER_BUFFER, paClipOff);

			// Create (and open) a new Stream, using the SineGenerator::generate function as a callback:		
			cout << "Opening beep output stream on: " << sys.deviceByIndex(iOutputDevice).name() << endl;
			portaudio::MemFunCallbackStream<SineGenerator> streamBeep(paramsBeep, sineGenerator, &SineGenerator::generate);
			
			cout << "Press enter to START recording after the beep.";
			cin.get(chWait);

			// Play the beep before starting the recording
			streamBeep.start();
			sys.sleep(BEEP_SECONDS * 1000);
			streamBeep.stop();
			streamBeep.close();

		} // end scope of sine output stream
	
		cout << "Opening recording input stream on " << sys.deviceByIndex(iInputDevice).name() << endl;
		portaudio::DirectionSpecificStreamParameters inParamsRecord(sys.deviceByIndex(iInputDevice), 1, portaudio::INT16, false, sys.deviceByIndex(iInputDevice).defaultLowInputLatency(), NULL);
		portaudio::StreamParameters paramsRecord(inParamsRecord, portaudio::DirectionSpecificStreamParameters::null(), SAMPLE_RATE, FRAMES_PER_BUFFER, paClipOff);		
		portaudio::MemFunCallbackStream<AudioBuffer> streamRecord(paramsRecord, objAudioBuffer, &AudioBuffer::RecordCallback);
	   
		cout << "Opening playback output stream on " << sys.deviceByIndex(iOutputDevice).name() << endl;
		portaudio::DirectionSpecificStreamParameters outParamsPlayback(sys.deviceByIndex(iOutputDevice), 1, portaudio::INT16, false, sys.deviceByIndex(iOutputDevice).defaultLowOutputLatency(), NULL);
		portaudio::StreamParameters paramsPlayback(portaudio::DirectionSpecificStreamParameters::null(), outParamsPlayback, SAMPLE_RATE, FRAMES_PER_BUFFER, paClipOff);		
		portaudio::MemFunCallbackStream<AudioBuffer> streamPlayback(paramsPlayback, objAudioBuffer, &AudioBuffer::PlaybackCallback);
		
		cout << "Press enter to STOP recording.";
		streamRecord.start();
		cin.get(chWait);
		streamRecord.stop();

		cout << "Writing samples to audio.raw" << endl;
		objAudioBuffer.WriteToFile("audio.raw");

		cout << "Playing back samples." << endl;
		objAudioBuffer.ResetPlayback();
		streamPlayback.start();
		while (streamPlayback.isActive())
			sys.sleep(100);
		streamPlayback.stop();
	   

		// Close the Stream (not strictly needed as terminating the System will also close all open Streams):
		streamRecord.close();
		streamPlayback.close();

		// Terminate the System (not strictly needed as the AutoSystem will also take care of this when it 
		// goes out of scope):
		sys.terminate();

	}
	catch (const portaudio::PaException &e)
	{
		cout << "A PortAudio error occured: " << e.paErrorText() << endl;
	}
	catch (const portaudio::PaCppException &e)
	{
		cout << "A PortAudioCpp error occured: " << e.what() << endl;
	}
	catch (const exception &e)
	{
		cout << "A generic exception occured: " << e.what() << endl;
	}
	catch (...)
	{
		cout << "An unknown exception occured." << endl;
	}

	return 0;
}


