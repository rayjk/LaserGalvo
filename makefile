inc = -I. -I../portaudio/bindings/cpp/include/ -I../portaudio/include/ -I../portaudio/src/common/
lib = -lportaudiocpp -lpthread -lportaudio -lfftw3

#PERF=-pg
#CFLAGS = -g -O3 $(PERF)
CFLAGS = -O3 $(PERF)

CPPFLAGS = $(inc)
LDFLAGS = $(lib)
LLDFLAGS = $(lib)
HOBJS = HeliosDac.o libusb-1.0.so

LaserShow:  AudioSample libHeliosDacAPI.so $(HOBJS)
	g++ LaserShow.cpp $(HOBJS) $(LLDFLAGS) $(PERF) -o bin/LaserShow

libHeliosDacAPI.so: HeliosDacAPI.o $(HOBJS)
	g++ -shared -o libHeliosDacAPI.so HeliosDacAPI.o HeliosDac.o libusb-1.0.so

HeliosDacAPI.o:
	g++ -Wall -std=c++14 -fPIC -O2 -c HeliosDacAPI.cpp

HeliosDac.o:
	g++ -Wall -std=c++14 -fPIC -O2 -c HeliosDac.cpp

AudioSample:  AudioSample.cpp AudioBuffer.o
	g++ AudioSample.cpp AudioBuffer.o $(LDFLAGS) $(PERF) -o bin/AudioSample

AudioBuffer.o: AudioBuffer.cpp AudioBuffer.h
	g++ $(CFLAGS) $(inc) -c AudioBuffer.cpp


clean:
	rm bin/* *.o libHeliosDacAPI.so
#	rm AudioSample
