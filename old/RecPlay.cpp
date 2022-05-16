#include "RecPlay.h"

RecPlay::RecPlay(){}

RecPlay::~RecPlay(){}

int RecPlay::AudioSetup(int argc, char* argv[])
{
	try
	{
		int 	iInputDevice = -1;
		int 	iOutputDevice = -1;
		// Create a object that is used to record, save to file and play the audio.
		AudioBuffer	objAudioBuffer((int) (SAMPLE_RATE * 1));

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
		cout << "Opening recording input stream on " << sys.deviceByIndex(iInputDevice).name() << endl;
		portaudio::DirectionSpecificStreamParameters inParamsRecord(sys.deviceByIndex(iInputDevice), 1, portaudio::INT16, false, sys.deviceByIndex(iInputDevice).defaultLowInputLatency(), NULL);
		portaudio::StreamParameters paramsRecord(inParamsRecord, portaudio::DirectionSpecificStreamParameters::null(), SAMPLE_RATE, FRAMES_PER_BUFFER, paClipOff);
		portaudio::MemFunCallbackStream<AudioBuffer> streamRecord(paramsRecord, objAudioBuffer, &AudioBuffer::RecordCallback);
		}
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

short* RecPlay::RetSamples(){

		streamRecord.start();
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		streamRecord.stop();

		return objAudioBuffer.GetSamples();

}
