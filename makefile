inc = -I. -I../portaudio/bindings/cpp/include/ -I../portaudio/include/ -I../portaudio/src/common/
lib = -lportaudiocpp -lpthread -lportaudio -lfftw3

#PERF=-pg
#CFLAGS = -O3 $(PERF)
CFLAGS = -O3

CPPFLAGS = $(inc)
LDFLAGS = $(lib)
LLDFLAGS = $(lib)
HOBJS = HeliosDac.o libusb-1.0.so
HOBJSR = HeliosDac.o libusb-1.0-rpi.so

laser:  AudioSample libHeliosDacAPI.so $(HOBJS) | bin
	g++ -o bin/LaserShow $(HOBJS) $(LLDFLAGS) $(PERF) LaserShow.cpp

laser-rpi:  AudioSample-rpi libHeliosDacAPI-rpi.so $(HOBJSR) | bin
	g++ -o bin/LaserShow-rpi $(HOBJSR) $(LLDFLAGS) $(PERF) LaserShow-rpi.cpp

libHeliosDacAPI.so: HeliosDacAPI.o $(HOBJS)
	g++ -shared -o libHeliosDacAPI.so $(HOBJS) HeliosDacAPI.o

libHeliosDacAPI-rpi.so: HeliosDacAPI.o $(HOBJSR)
	g++ -shared -o libHeliosDacAPI-rpi.so $(HOBJSR) HeliosDacAPI.o

HeliosDacAPI.o:
	g++ -Wall -std=c++14 -fPIC -O2 -c HeliosDacAPI.cpp

HeliosDac.o:
	g++ -Wall -std=c++14 -fPIC -O2 -c HeliosDac.cpp

AudioSample:  AudioSample.cpp AudioBuffer.o | bin
	g++ $(LDFLAGS) $(PERF) -o bin/AudioSample AudioSample.cpp AudioBuffer.o

AudioSample-rpi:  AudioSample-rpi.cpp AudioBuffer-rpi.o | bin
	g++ $(LDFLAGS) $(PERF) -o bin/AudioSample-rpi AudioSample-rpi.cpp AudioBuffer-rpi.o

bin:
	mkdir bin

AudioBuffer.o: AudioBuffer.cpp AudioBuffer.h
	g++ $(CFLAGS) $(inc) -c AudioBuffer.cpp

AudioBuffer-rpi.o: AudioBuffer-rpi.cpp AudioBuffer-rpi.h
	g++ $(CFLAGS) $(inc) -o AudioBuffer-rpi.o -c AudioBuffer-rpi.cpp

clean:
	rm -r bin *.o libHeliosDacAPI.so libHeliosDacAPI-rpi.so
