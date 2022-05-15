//Example program scanning a line from top to bottom on the Helios
#include <cmath>
#include <tuple>
#include "HeliosDac.h"
#include <chrono>
#include <thread>
#include <bits/stdc++.h>

using namespace std;

#define PTS_SQ 400
#define PTS_MID (PTS_SQ/2)
#define FRAMES 2000
#define CYCLES 20
#define PAUSE 20
#define SCALE .8

#include <fftw3.h>

#define NUM_POINTS 64


/* Never mind this bit */

#include <stdio.h>
#include <math.h>

#define REAL 0
#define IMAG 1

tuple<int, int> rescaleXY (int x, int y){
  x = (float)(x - 0xFFF/2) * SCALE + 0xFFF/2;
  y = (float)(y - 0xFFF/2) * SCALE + 0xFFF/2;
  return make_tuple(x,y);
}

void acquire_from_somewhere(fftw_complex* signal) {
    /* Generate two sine waves of different frequencies and
     * amplitudes.
     */

    int i;
    for (i = 0; i < NUM_POINTS; ++i) {
        double theta = (double)i / (double)NUM_POINTS * M_PI;

        signal[i][REAL] = 1.0 * cos(10.0 * theta) +
                          0.5 * cos(25.0 * theta);

        signal[i][IMAG] = 1.0 * sin(10.0 * theta) +
                          0.5 * sin(25.0 * theta);
    }
}

void do_something_with(fftw_complex* result, float* freq_arr) {
    int i;
    for (i = 0; i < NUM_POINTS; ++i) {
        freq_arr[i] = sqrt(result[i][REAL] * result[i][REAL] +
                          result[i][IMAG] * result[i][IMAG]);
    }
}


/* Resume reading here */

void getFreqArr(float* freq_arr) {
    fftw_complex signal[NUM_POINTS];
    fftw_complex result[NUM_POINTS];

    fftw_plan plan = fftw_plan_dft_1d(NUM_POINTS,
                                      signal,
                                      result,
                                      FFTW_FORWARD,
                                      FFTW_ESTIMATE);

    acquire_from_somewhere(signal);
    fftw_execute(plan);
    do_something_with(result, freq_arr);

    fftw_destroy_plan(plan);
}

tuple<int, int, int> HSVtoRGB(float H, float S,float V){
  if(H>360 || H<0 || S>100 || S<0 || V>100 || V<0){
      cout<<"The givem HSV values are not in valid range"<<endl;
      exit(1);
  }
  float s = S/100;
  float v = V/100;
  float C = s*v*0xFF;
  float X = C*(1-abs(fmod(H/60.0, 2)-1));
  float m = v-C;
  int r,g,b;
  if(H >= 0 && H < 60){
      r = C,g = X,b = 0;
  }
  else if(H >= 60 && H < 120){
      r = X,g = C,b = 0;
  }
  else if(H >= 120 && H < 180){
      r = 0,g = C,b = X;
  }
  else if(H >= 180 && H < 240){
      r = 0,g = X,b = C;
  }
  else if(H >= 240 && H < 300){
      r = X,g = 0,b = C;
  }
  else{
      r = C,g = 0,b = X;
  }

	return make_tuple(r,g,b);
}

tuple<int, int> genSin(int j){
	int x;
	int y;
	if (j < PTS_MID){
		y = 0xFFF *.5* ( sin (2 * 3.14159 * 10 * (float)j/(float)PTS_MID + 0)/2.+.5);
		x = j * 0xFFF / PTS_MID;
	} else {
		y = 0xFFF *.5* ( sin (2 * 3.14159 * 10 * (float)(j % PTS_MID)/(float)PTS_MID + 3.14159)/2.+.5);
		x = 0xFFF - ((j - PTS_MID) * 0xFFF / PTS_MID);
	}
	return make_tuple(x,y);
}

tuple<int, int> genCircle(int i, int j){
	int x;
	int y;
	y = 0xFFF * ( sin (2 * 3.14159 * 1 * (float)j/(float)PTS_SQ + 3.14159)/2.*(abs((float)(FRAMES/2-i))/(float)(FRAMES/2))+.5);
	x = 0xFFF * ( cos (2 * 3.14159 * 1 * (float)j/(float)PTS_SQ + 3.14159)/2.*(abs((float)(FRAMES/2-i))/(float)(FRAMES/2))+.5);
	return make_tuple(x,y);
}

tuple<int, int> offset(int i, int j){
	int x;
	int y;
	y = 0xFFF * ( sin (2. * 3.14159 * 5. * (float)j/(float)PTS_SQ + 3.14159)/2.+.5);
	x = 0xFFF * ( cos (2. * 3.14159 * (5.*(1.5-abs((float)(i-FRAMES/2)/(float)(FRAMES/2)))) * (float)j/(float)PTS_SQ + 3.14159)/2.+.5);
	return make_tuple(x,y);
}

int main(void)
{
	//make frames
	HeliosPoint frame[FRAMES][PTS_SQ];
	int x = 0;
	int y = 0;
	int r = 0;
	int g = 0;
	int b = 0;
	for (int i = 0; i < FRAMES; i++)
	{

		//y = i * 0xFFF / 30;

    // Spectrum Analysis
    float freq_arr[NUM_POINTS];
    getFreqArr(freq_arr);
    int arrIdx;

		for (int j = 0; j < PTS_SQ; j++)
		{
			//tie(x,y) = genSin(j);
			//tie(x,y) = genCircle(i, j);
			//tie(x,y) = offset(i, j);

      // Spectrum Analysis
      if (j < PTS_MID){
        tie(r,g,b) = HSVtoRGB((float)j/(float)PTS_MID*360., 100., 100.);
        arrIdx = (int)(float)j/(float)PTS_MID*((float)NUM_POINTS);
        y = 0xFFF * ((float)freq_arr[arrIdx]/100.);
        x = j * 0xFFF / PTS_MID;
      } else {
        tie(r,g,b) = HSVtoRGB(360 - (float)(j-PTS_MID)/(float)PTS_MID*360., 100., 100.);
        arrIdx = NUM_POINTS - (int)(float)(j-PTS_MID)/(float)PTS_MID*((float)NUM_POINTS);
        y = 0xFFF * ((float)freq_arr[arrIdx]/100.);
        x = 0xFFF - ((j - PTS_MID) * 0xFFF / PTS_MID);
      }

      //tie(r,g,b) = HSVtoRGB((float)j/(float)PTS_SQ*360., 100., pow(abs((j%(int)((float)PTS_SQ/4.))-(int)((float)PTS_SQ/4.))/(float)(PTS_SQ/4.),.5)*100.);

      tie(x, y) = rescaleXY(x, y);
			frame[i][j].x = x;
			frame[i][j].y = y;
			frame[i][j].r = r;
			frame[i][j].g = g;
			frame[i][j].b = b;
			// frame[i][j].r = 0xD0;
			// frame[i][j].g = 0xFF;
			// frame[i][j].b = 0xD0;
			frame[i][j].i = 0xFF;
		}
	}

	//connect to DACs and output frames
	HeliosDac helios;
	int numDevs = helios.OpenDevices();

	int i = 0;
	while (1)
	{
		i++;
		if (i > FRAMES * CYCLES) //cancel after 5 cycles, 30 frames each
			break;

		for (int j = 0; j < numDevs; j++)
		{
			//wait for ready status
			for (unsigned int k = 0; k < 512; k++)
			{
				if (helios.GetStatus(j) == 1)
					break;
			}
			//std::this_thread::sleep_for(std::chrono::milliseconds(PAUSE));

			helios.WriteFrame(j, 20000, HELIOS_FLAGS_DEFAULT, &frame[i % FRAMES][0], PTS_SQ); //send the next frame
		}
	}

	//freeing connection
	helios.CloseDevices();
}
