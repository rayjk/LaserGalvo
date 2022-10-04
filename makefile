inc = -I. -I../portaudio/bindings/cpp/include/ -I../portaudio/include/ -I../portaudio/src/common/ -I/usr/local/include -L/usr/local/lib
lib = -lportaudiocpp -lpthread -lportaudio -lfftw3
dmx = -lola -lolacommon -lprotobuf -pthread

#PERF=-pg
#CFLAGS = -O3 $(PERF)
CFLAGS = -O3

CPPFLAGS = $(inc)
LDFLAGS = $(lib)
LLDFLAGS = $(lib)
DMXFLGS = $(dmx)
HOBJS = HeliosDac.o libusb-1.0.so
HOBJSR = HeliosDac.o libusb-1.0-rpi.so

# PKG_CONFIG_PATH=/usr/local/lib/pkgconfig

laser:  AudioSample libHeliosDacAPI.so $(HOBJS) | bin
	g++ -o bin/LaserShow $(HOBJS) $(LLDFLAGS) $(PERF) LaserShow.cpp

beam:  AudioSample libHeliosDacAPI.so $(HOBJS) | bin
	g++ -o bin/LaserShow $(HOBJS) $(LLDFLAGS) $(DMXFLGS) $(PERF) LaserShow_Beam.cpp

beam-rpi:  AudioSample-rpi libHeliosDacAPI-rpi.so $(HOBJSR) | bin
	g++ LaserShow_Beam-rpi.cpp $(HOBJSR) $(LLDFLAGS) $(DMXFLGS) $(PERF) -o bin/LaserShow-rpi

laser-rpi:  AudioSample-rpi libHeliosDacAPI-rpi.so $(HOBJSR) | bin
	g++ LaserShow-rpi.cpp $(HOBJSR) $(LLDFLAGS) $(PERF) -o bin/LaserShow-rpi

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
