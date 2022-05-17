//Example program scanning a line from top to bottom on the Helios
#include <cmath>
#include <tuple>
#include <fcntl.h>
#include "HeliosDac.h"
#include <chrono>
#include <thread>
#include <bits/stdc++.h>

using namespace std;

#define PTS_SQ 400
#define PTS_MID (PTS_SQ/2)
//#define FRAMES 2000
#define FRAMES 1
#define CYCLES 20
#define PAUSE 100
#define SCALE .9
#define E 2.71281
#define PI 3.14159

#include <fftw3.h>

#define SAMPLE_RATE 8000
#define NUM_POINTS 2048
#define BUFF_SZE 128
#define BASS_OFFSET  (80.*(float)NUM_POINTS/(float)SAMPLE_RATE)
int ARR_END = 0;
bool ARR_INIT = false;

/* Never mind this bit */

#include <stdio.h>
#include <math.h>

#define REAL 0
#define IMAG 1

HeliosPoint frame[FRAMES][PTS_SQ];
double MAX_MAG = 0;
float PREV_ARR[NUM_POINTS/2];

tuple<int, int> rescaleXY (int x, int y, int loopCount){//, bool graphFlag){
  // float localScale = SCALE + (SCALE / (float)(loopCount%3+1)) / 2.;
  x = -1.*(float)(x - 0xFFF/2) * SCALE + 0xFFF/2;
  // if (graphFlag)
  //   y = -1.*(float)(y * localScale - 0xFFF/2) * SCALE + 0xFFF/2;
  // else
    y = -1.*(float)(y - 0xFFF/2) * SCALE + 0xFFF/2;
  return make_tuple(x,y);
}

tuple<int, int> rotateXY (int x, int y, int i, int rate){
  i = i%rate;
  // if (i<rate/2)
  //   x = (float)x * ((float)i/(float)(rate/2));
  // else
  //   x = (float)x * (1.-(float)(i-rate/2)/(float)(rate/2));
  int xStd = x - 0xFFF/2;
  x = (x - 0xFFF/2) * (sin((float)i/(float)rate*2.*PI)) + 0xFFF/2;
  // if (xStd < 0)
  y = y*(7./9.);
  y = y+(cos((float)i/(float)rate*2.*PI))*0xFFF/9.*((float)xStd/((float)0xFFF/2.));
  y = y + 0xFFF/9.;
  if (y>0xFFF)
    y=0xFFF;
  // else
  //   y = y+(cos((float)i/(float)rate*2.*PI))*0xFFF/2.*((float)x/((float)0xFFF/2.));

  //y = (float)(y - 0xFFF/2) * ((float)xStd/(float)0xFFF) + 0xFFF/2;
  return make_tuple(x,y);
}

// void acquire_from_somewhere(fftw_complex* signal) {
//     /* Generate two sine waves of different frequencies and
//      * amplitudes.
//      */
//
//     int i;
//     for (i = 0; i < NUM_POINTS; ++i) {
//         double theta = (double)i / (double)NUM_POINTS * M_PI;
//
//         signal[i][REAL] = 1.0 * cos(10.0 * theta) +
//                           0.5 * cos(25.0 * theta);
//
//         signal[i][IMAG] = 1.0 * sin(10.0 * theta) +
//                           0.5 * sin(25.0 * theta);
//     }
// }

void get_stdin_audio(int* shift_sig, fftw_complex* signal){
  int pt;
  //int flags = fcntl(0, F_GETFL, 0);
  //for (i = 0; i < NUM_POINTS; ++i) {
  //streamsize chars_left;
  string line;
  // streambuf* cbuf;
  do {
    //fcntl(0, F_SETFL, flags | O_NONBLOCK);
    // getline(cin, line);
    // cbuf = cin.rdbuf();
    // chars_left = cbuf->in_avail();
    // cout << chars_left << endl;
    // while (chars_left > 64) {
    for (int i=0; i< BUFF_SZE; ++i){
        getline(cin, line);
        sscanf(line.c_str(), "%d", &pt);
        shift_sig[ARR_END] = pt;
        ARR_END = (ARR_END + 1) % NUM_POINTS;
        if (!ARR_INIT)
          signal[i][IMAG] = 0;
        if (ARR_END == NUM_POINTS-1)
          ARR_INIT = true;
        //chars_left = cbuf->in_avail();
    }
    //cin.clear();
  } while(!ARR_INIT);
  for(int i=0; i<NUM_POINTS; ++i){
    signal[i][REAL] = shift_sig[(ARR_END + i) % NUM_POINTS];
    //cout << signal[i][REAL] << ',';
  }
  //cout << endl;
}

void do_something_with(fftw_complex* result, float* freq_arr) {
    int i;
    int arrOffset;
    double maxAmp = 0.;
    int domFreq;
    double curAmp;

    for (i = 0; i < NUM_POINTS/2; ++i) {
      //arrOffset = (i+ARR_END) % NUM_POINTS;
      //curAmp = result[arrOffset][REAL];
      curAmp = result[i][REAL];
      if (curAmp > maxAmp){
        maxAmp = curAmp;
        domFreq = i;
      }
    }
    if (maxAmp > MAX_MAG)
      MAX_MAG = maxAmp;
    for (i = 0; i < NUM_POINTS/2; ++i) {
      // arrOffset = (i+ARR_END) % NUM_POINTS;
      //curAmp = result[i][REAL] / maxAmp;
      curAmp = sqrt(result[i][REAL]) / 1400.;
      //curAmp = result[i][REAL] / MAX_MAG;
      if (curAmp >= 0.)
        freq_arr[i] = curAmp;
      else if (curAmp > 1.)
        curAmp = 1.;
      else
        freq_arr[i] = 0.;
    }
    for(int i=0; i<NUM_POINTS/2; ++i){
      if (freq_arr[i] < PREV_ARR[i]){
        float diff = PREV_ARR[i] - freq_arr[i];
        freq_arr[i] = PREV_ARR[i] - diff*.05;
      }
      PREV_ARR[i] = freq_arr[i];
    }
    // domFreq = (float) domFreq * ((float)SAMPLE_RATE / (float)NUM_POINTS);
    // if (domFreq > 60)
    //   cout << domFreq << ',' << maxAmp << endl;

}


/* Resume reading here */

void getFreqArr(float* freq_arr) {
  int shift_sig[NUM_POINTS];
  fftw_complex signal[NUM_POINTS];
  fftw_complex result[NUM_POINTS];

  fftw_plan plan = fftw_plan_dft_1d(NUM_POINTS,
                                    signal,
                                    result,
                                    FFTW_FORWARD,
                                    FFTW_ESTIMATE);

  //acquire_from_somewhere(signal);
  get_stdin_audio(shift_sig, signal);
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

double getAvg(float* freq_arr, int lastIdx, int newIdx)
{
  if (newIdx == 0)
    return 0.;
  double avg=0;
  int count=0;
  for(int i=lastIdx; i<=newIdx; ++i){
    avg += freq_arr[i];
    count++;
  }
  avg = avg / double(count);
  return avg;
}

void getFrames(int loopCount)
{
	//make frames
	int x = 0;
	int y = 0;
	int r = 0;
	int g = 0;
	int b = 0;
  // bool graphFlag;
	for (int i = 0; i < FRAMES; i++)
	{

		//y = i * 0xFFF / 30;

    // Spectrum Analysis
    float freq_arr[NUM_POINTS/2];
    getFreqArr(freq_arr);
    int arrIdx;
    int lastIdx = 0;
    double magVal;

		for (int j = 0; j < PTS_SQ; j++)
		{
			//tie(x,y) = genSin(j);
			//tie(x,y) = genCircle(i, j);
			//tie(x,y) = offset(i, j);

      // Spectrum Analysis

      if (j < PTS_MID){
        tie(r,g,b) = HSVtoRGB((int)((float)j/(float)PTS_MID*360.+loopCount*2.)%360, 100., 100.);
        arrIdx = (int)(pow((float)j/(float)PTS_MID, E)*((float)(NUM_POINTS/2-BASS_OFFSET)))+BASS_OFFSET;
        magVal = getAvg(freq_arr, lastIdx, arrIdx);
        lastIdx = arrIdx;
        y = (double)0xFFF * magVal;
        // graphFlag = true;
        //y = (double)0xFFF * freq_arr[arrIdx];
        //y = 0;
        // if (freq_arr[arrIdx] > 0.1)
        //   cout << y << ','<< freq_arr[arrIdx] << endl;
        x = j * 0xFFF / PTS_MID;
      } else {
        // graphFlag = false;
        tie(r,g,b) = HSVtoRGB(360 - (int)((float)(j-PTS_MID)/(float)PTS_MID*360.+loopCount*2.)%360, 100., 100.);
        // arrIdx = NUM_POINTS - (int)(float)(j-PTS_MID)/(float)PTS_MID*((float)NUM_POINTS);
        // magVal = getAvg(freq_arr, lastIdx, arrIdx);
        // lastIdx = arrIdx;
        //y = (double) 0xFFF * magVal;
        y = 0;
        x = 0xFFF - ((j - PTS_MID) * 0xFFF / PTS_MID);
      }

      //tie(r,g,b) = HSVtoRGB((float)j/(float)PTS_SQ*360., 100., pow(abs((j%(int)((float)PTS_SQ/4.))-(int)((float)PTS_SQ/4.))/(float)(PTS_SQ/4.),.5)*100.);

      tie(x, y) = rotateXY(x, y, loopCount, 800);
      tie(x, y) = rescaleXY(x, y, loopCount);//, graphFlag);
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
}

int main(void)
{

	//connect to DACs and output frames
	HeliosDac helios;
	int numDevs = helios.OpenDevices();

	int i = 0;
  for(int jj=0; jj<NUM_POINTS/2; ++jj)
    PREV_ARR[jj] = 0.;
	while (1)
	{
    if(i%FRAMES==0)
      getFrames(i);
		i++;
		 // if (i > FRAMES * CYCLES) //cancel after 5 cycles, 30 frames each
		 // 	break;

		for (int j = 0; j < numDevs; j++)
		{
			//wait for ready status
			for (unsigned int k = 0; k < 512; k++)
			{
				if (helios.GetStatus(j) == 1)
					break;
			}
			//std::this_thread::sleep_for(std::chrono::milliseconds(PAUSE));

			helios.WriteFrame(j, 30000, HELIOS_FLAGS_DEFAULT, &frame[i % FRAMES][0], PTS_SQ); //send the next frame
		}
	}

	//freeing connection
	helios.CloseDevices();
}
