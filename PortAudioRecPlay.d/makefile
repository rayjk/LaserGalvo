inc = -I. -I../portaudio/bindings/cpp/include/ -I../portaudio/include/ -I../portaudio/src/common/
lib = -lportaudiocpp -lpthread -lportaudio

#PERF=-pg
#CFLAGS = -g -O3 $(PERF)
CFLAGS = -O3 $(PERF)

CPPFLAGS = $(inc)
LDFLAGS = $(lib)
HOBJS = Sine.o AudioBuffer.o

PortAudioRecPlay:  PortAudioRecPlay.cpp $(HOBJS)
	g++ PortAudioRecPlay.cpp $(HOBJS) $(LDFLAGS) $(PERF) -o PortAudioRecPlay

Sine.o: Sine.cpp Sine.h
	g++ $(CFLAGS) $(inc) -c Sine.cpp

AudioBuffer.o: AudioBuffer.cpp AudioBuffer.h
	g++ $(CFLAGS) $(inc) -c AudioBuffer.cpp

clean:
	rm -f *.o
	rm PortAudioRecPlay
