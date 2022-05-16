inc = -I. -I../portaudio/bindings/cpp/include/ -I../portaudio/include/ -I../portaudio/src/common/
lib = -lportaudiocpp -lpthread -lportaudio -lfftw3
llib = -lfftw3

#PERF=-pg
#CFLAGS = -g -O3 $(PERF)
CFLAGS = -O3 $(PERF)

CPPFLAGS = $(inc)
LDFLAGS = $(lib)
LLDFLAGS = $(llib)
HOBJS = HeliosDac.o libusb-1.0.so
POBJS  = Sine.o AudioBuffer.o

LaserShow:  PortAudioRecPlay $(HOBJS)
	g++ LaserShow.cpp $(HOBJS) $(LLDFLAGS) $(PERF) -o bin/LaserShow

PortAudioRecPlay:  PortAudioRecPlay.cpp $(POBJS)
	g++ PortAudioRecPlay.cpp $(POBJS) $(LDFLAGS) $(PERF) -o bin/PortAudioRecPlay

Sine.o: Sine.cpp Sine.h
	g++ $(CFLAGS) $(inc) -c Sine.cpp

AudioBuffer.o: AudioBuffer.cpp AudioBuffer.h
	g++ $(CFLAGS) $(inc) -c AudioBuffer.cpp


clean:
	rm bin/* Sine.o AudioBuffer.o
#	rm PortAudioRecPlay
