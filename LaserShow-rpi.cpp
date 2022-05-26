 // (c) 2022 Raymond Korte
#include <bits/stdc++.h>
#include <chrono>
#include <cmath>
#include <fcntl.h>
#include <fftw3.h>
#include <math.h>
#include <stdio.h>
#include <thread>
#include <tuple>
#include "HeliosDac.h"

#define SCALE 1.
#define DECAY .08
#define MIN_VOL_PCT .01
#define MIN_FR_MAX 0.1
#define ROTATE_RATE 200
#define MAX_OVER_FRAMES 200

#define SAMPLE_RATE    11025
#define TOT_SAMPLES    1024
#define BUFF_SZE       80
#define BASS_CUTOFF_HZ 80.
#define BASS_OFFSET   (BASS_CUTOFF_HZ * (float)TOT_SAMPLES / (float)SAMPLE_RATE)

#define NUM_FRAMES 1
#define PPS 20000
#define BLANKING 10
//#define GRAPH_SHARP BLANKING
//#define GRAPH_SHARP 5
#define GRAPH_SHARP 0

#define PTS_CLOVER 100
#define PTS_PLOT 150
#define PTS_PLOT_GRAPH (int)((float)PTS_PLOT * .8)
#define PTS_PLOT_BASE (PTS_PLOT - PTS_PLOT_GRAPH)
#define PTS_TOT (PTS_CLOVER + PTS_PLOT * 2)

#define E  2.71281
#define PI 3.14159
#define REAL 0
#define IMAG 1

using namespace std;

HeliosPoint  frame[NUM_FRAMES][PTS_TOT];
fftw_complex SIGNAL[TOT_SAMPLES];
double       PREV_ARR[PTS_PLOT_GRAPH];
double       PREV_MAXS[MAX_OVER_FRAMES];
double       SHIFT_SIG[TOT_SAMPLES];
int          PREV_MAXS_END = 0;
int          ARR_END = 0;
bool         ARR_INIT = false;


 // std::chrono::high_resolution_clock::time_point latencySum;

void rescaleXY (int &x, int &y, int loopCount, float cusScale = SCALE) {
  x = - 1. * (float)(x - 0xFFF / 2) * .8 * cusScale + 0xFFF / 2;
  y = - 1. * (float)(y - 0xFFF / 2) * cusScale + 0xFFF / 2;
  if (x>0xFFF)
    x = 0xFFF;
  if (y>0xFFF)
    y = 0xFFF;
}

void rotateXY (int &x, int &y, int i, int rate, bool off90 = false) {
  i = i % rate;
  int xStd = x - 0xFFF / 2;
  int yStd = y - 0xFFF / 2;
  if (off90) {
    x = (x - 0xFFF / 2)
        * (sin((float)i / (float)rate * 2. * PI))
        + 0xFFF / 2;
    y = y * (7. / 9.);
    y = y + (cos((float)i / (float)rate * 2. * PI))
        * 0xFFF / 9.
        * ((float)xStd / ((float)0xFFF / 2.));
    y = y + 0xFFF / 9.;
  }
  else{
    x = (x - 0xFFF / 2)
        * (cos((float)i / (float)rate * 2. * PI + PI))
        + 0xFFF / 2;
    y = y * (7. / 9.);
    y = y + (sin((float)i / (float)rate * 2. * PI))
        * 0xFFF / 9.
        * ((float)xStd / ((float)0xFFF / 2.));
    y = y + 0xFFF / 9.;
  }
}

void get_stdin_audio() {
  int pt;
  string line;
  do {
    for (int i = 0; i< BUFF_SZE; ++i) {
        getline(cin, line);
        pt = stoi(line.c_str());
        SHIFT_SIG[ARR_END] = pt;
        ARR_END = (ARR_END + 1) % TOT_SAMPLES;
        if (!ARR_INIT)
          SIGNAL[i][IMAG] = 0;
        if (ARR_END == TOT_SAMPLES - 1)
          ARR_INIT = true;
    }
  } while(!ARR_INIT);
  for(int i = 0; i<TOT_SAMPLES; ++i) {
    SIGNAL[i][REAL] = SHIFT_SIG[(ARR_END + i) % TOT_SAMPLES];
  }
}

double getAvg(fftw_complex * result, int lastIdx, int newIdx) {
  if (newIdx == 0)
    return 0.;
  double avg = 0;
  int count = 0;
  for(int i = lastIdx; i <= newIdx; ++i) {
    avg += abs(result[i][REAL]);
    count++;
  }
  avg /= (double)count;
  return avg;
}


void getFreqArr(double * freq_arr) {
  int i;
  int arrOffset;
  double maxAmp = 0.;
  double globalMax = 1.;
  double frMax = 1.;
  int arrIdx;
  int lastIdx = 0;
  double magVal;
  int grMaxCnt;
  fftw_complex result[TOT_SAMPLES];
  fftw_plan plan = fftw_plan_dft_1d(TOT_SAMPLES,
                                    SIGNAL,
                                    result,
                                    FFTW_FORWARD,
                                    FFTW_ESTIMATE);
  fftw_execute(plan);

  for (int i = 0; i<PTS_PLOT_GRAPH; i++ ) {
    arrIdx = (int)(pow((float)i / (float)PTS_PLOT_GRAPH, E)
             * ((float)(TOT_SAMPLES / 2 - BASS_OFFSET))) + BASS_OFFSET;
    magVal = getAvg(result, lastIdx, arrIdx);
    lastIdx = arrIdx;
    magVal = pow(magVal, .8 + (.2 * ((float)i / (float)PTS_PLOT_GRAPH)));
    if (!(magVal >= 0))
      magVal = 1.;
    if (magVal > maxAmp) {
      maxAmp = magVal;
    }
    freq_arr[i] = magVal;
  }

  PREV_MAXS[PREV_MAXS_END] = maxAmp;
  PREV_MAXS_END = (PREV_MAXS_END + 1) % MAX_OVER_FRAMES;
  for (i = 0; i< MAX_OVER_FRAMES; ++i) {
    if (PREV_MAXS[i] > globalMax){
      grMaxCnt = 0;
      for (int j = i; j < MAX_OVER_FRAMES; j++){
        if (PREV_MAXS[j] >= PREV_MAXS[i])
          grMaxCnt++;
      }
      if (grMaxCnt >= (float)MAX_OVER_FRAMES * MIN_FR_MAX)
        globalMax = PREV_MAXS[i];
    }
//      globalMax += PREV_MAXS[i];
  }
//  globalMax /= (double)MAX_OVER_FRAMES;

  for (i = 0; i < PTS_PLOT_GRAPH; i++){
    if (freq_arr[i] > frMax)
      frMax = freq_arr[i];
  }
  globalMax = max(globalMax, frMax);
  if (globalMax < 1)
    globalMax = 1.;
//  cout << globalMax << endl;
  for (i = 0; i < PTS_PLOT_GRAPH; ++i) {
//    freq_arr[i] = min(freq_arr[i] / globalMax, 1.);
    if (frMax > MIN_VOL_PCT * (float)0xFFFF)
      freq_arr[i] = freq_arr[i] / globalMax;
    else
      freq_arr[i] = 0;
    double diff = PREV_ARR[i] - freq_arr[i];

    if (diff > 0) {
      freq_arr[i] = PREV_ARR[i] - diff * DECAY;
    }
    PREV_ARR[i] = freq_arr[i];
  }
  fftw_destroy_plan(plan);
}

void HSVtoRGB(float H, float S, float V, int& r, int& g, int& b) {
  if(H>360 || H<0 || S>100 || S<0 || V>100 || V<0) {
      cout << "The givem HSV values are not in valid range" << endl;
      exit(1);
  }
  float s = S / 100;
  float v = V / 100;
  float C = s * v * 0xFF;
  float X = C * (1 - abs(fmod(H / 60.0, 2) - 1));
  float m = v - C;
  if(H >= 0 && H < 60) {
      r = C, g = X, b = 0;
  }
  else if(H >= 60 && H < 120) {
      r = X, g = C, b = 0;
  }
  else if(H >= 120 && H < 180) {
      r = 0, g = C, b = X;
  }
  else if(H >= 180 && H < 240) {
      r = 0, g = X, b = C;
  }
  else if(H >= 240 && H < 300) {
      r = X, g = 0, b = C;
  }
  else{
      r = C, g = 0, b = X;
  }
}

void genGraph(int i, int loopCount, double * freq_arr, int& lastIdx,
              int& x, int& y, int& r, int& g, int& b) {
  int arrIdx;
  int hue;
  double magVal;
  if (i < PTS_PLOT_GRAPH) {
    magVal = freq_arr[i];
    if (i > PTS_PLOT_GRAPH - GRAPH_SHARP) {
      y = 0;
      x = 0xFFF;
    }else{
      y = (double)0xFFF * magVal;
      x = i * 0xFFF / (PTS_PLOT_GRAPH - 2);
    }

    hue = (int)((float)i / (float)PTS_PLOT_GRAPH * 360. + loopCount * 2.) % 360;
    HSVtoRGB(hue, 100., 100., r, g, b);


  } else {
    hue = (int)((float)(i - PTS_PLOT_GRAPH)
          / (float)(PTS_PLOT_BASE - 1) * 360. + loopCount * 2.) % 360;
    HSVtoRGB(hue, 100., 100., r, g, b);
    y = 0;
    x = 0xFFF - (i - PTS_PLOT_GRAPH) * 0xFFF / (PTS_PLOT_BASE - 1);
  }
}

void genClover(int i, int loopCount, int rate, int& x,
               int& y, int& r, int& g, int& b) {
  int hue;
  float rad = cos(2 * PI * 4 * (float)i / (float)(PTS_CLOVER - 1))
              / 2. * .7 + .3 / 2. + .5;
  y = (sin(PI / - 2. + 2 * PI * 1 * (float)i
      / (float)(PTS_CLOVER - 1) - 2.
      * (float)loopCount / (float)rate * PI)
      / 2. * 0xFFF * rad * .25 + .1 * 0xFFF);
  x = (cos(PI / - 2. + 2 * PI * 1 * (float)i
      / (float)(PTS_CLOVER - 1) - 2.
      * (float)loopCount / (float)rate * PI) / 2.
      * 0xFFF * rad + .5 * 0xFFF);
  hue = (int)((float)i / (float)(PTS_CLOVER - 1) * 360.
      * 2. + loopCount * 2.) % 360;
  HSVtoRGB(hue, 100., 100., r, g, b);
}

int interpBlanking(int i, int maxFrames) {
  if (i == BLANKING + 1)
    return 0;
  if (i > maxFrames - .6 * BLANKING - 1)
    return maxFrames - 1;
  float factor = (float)maxFrames / (float)(maxFrames - 1.6 * BLANKING);
  i = (float)i * factor - BLANKING * factor;
  return i;
}

void getFrames(int loopCount)
{
  int x = 0;
  int y = 0;
  int r = 0;
  int g = 0;
  int b = 0;
  int hue;
  int lastIdx;
  int locIter;
  double freq_arr[PTS_PLOT_GRAPH];
  for (int i = 0; i < NUM_FRAMES; i++ ) {
    loopCount = loopCount + i;
    getFreqArr(freq_arr);
    lastIdx = 0;
    locIter = 0;

    for (int j = 0; j < PTS_TOT; j++ )
    {
      if (j < PTS_PLOT * 2) {
        if (j == PTS_PLOT) {
          locIter = 0;
          lastIdx = 0;
        }
        if (locIter > BLANKING) {
          genGraph(interpBlanking(locIter, PTS_PLOT), loopCount,
                   freq_arr, lastIdx, x, y, r, g, b);
        }
        else{
          HSVtoRGB(90, 100., 0., r, g, b);
          y = 0;
          x = 0;
        }
         if (j < PTS_PLOT)
          x = (float)x * .85 + .075 * (float)0xFFF;
        else
          x = (float)x * .9 + .05 * (float)0xFFF;
        rotateXY(x, y, loopCount, ROTATE_RATE, (j >= PTS_PLOT));
      }
      else{
        if (j == PTS_PLOT * 2)
          locIter = 0;
        if (locIter>BLANKING) {
          genClover(interpBlanking(locIter, PTS_CLOVER), loopCount,
                                   ROTATE_RATE, x, y, r, g, b);
        }
        else{
          genClover(0, loopCount, ROTATE_RATE, x, y, r, g, b);
          HSVtoRGB(90, 100., 0., r, g, b);
        }
      }

      rescaleXY(x, y, loopCount);
      locIter += 1;

      frame[i][j].x = x;
      frame[i][j].y = y;
      frame[i][j].r = r;
      frame[i][j].g = g;
      frame[i][j].b = b;
      frame[i][j].i = 0xFF;
    }
  }
}

int main(void)
{

  // connect to DACs and output frames
  HeliosDac helios;
  int numDevs = helios.OpenDevices();

  int i = 0;
  int loopCount = 0;

  std::chrono::high_resolution_clock::time_point startFrame;
  std::chrono::high_resolution_clock::time_point stopFrame;
  std::chrono::high_resolution_clock::time_point startBuf;
  std::chrono::high_resolution_clock::time_point stopBuf;
  long int latencySum;
  long int lastLatency;
  int latencyCount = 0;
  int readCount = 0;
  int numLatency = 0;
  float avgLatency;
  float fps;
  bool frameSuccess = false;

  for(int jj = 0; jj<TOT_SAMPLES / 2; ++jj)
    PREV_ARR[jj] = 0.;
  for(int kk = 0; kk<MAX_OVER_FRAMES; ++kk)
    PREV_MAXS[kk] = 1.;

  startFrame = chrono::high_resolution_clock::now();
  while (1)
  {
    i++;
    loopCount = loopCount + NUM_FRAMES;

    for (int j = 0; j < numDevs; j++ )
    {
    // Read in audio buffer until DAC gives ready signal
      do {
        startBuf = chrono::high_resolution_clock::now();
        get_stdin_audio();
        stopBuf = chrono::high_resolution_clock::now();
        lastLatency = chrono::duration_cast
                      <std::chrono::microseconds>(stopBuf - startBuf).count();
        latencySum = latencySum + lastLatency;
        latencyCount++;
        readCount++;
        if (latencyCount % 100 == 0) {
          if (!frameSuccess) {
            helios.CloseDevices();
            helios.OpenDevices();
            cout << "Reinitializing DAC" << endl;
            startFrame = chrono::high_resolution_clock::now();
          }
          else{
            avgLatency = avgLatency + (float)latencySum / 1000. / 100.;
            numLatency++;
          }
          latencySum = 0;
          latencyCount = 0;
        }
      } while (helios.GetStatus(j) != 1);
      getFrames(loopCount);
      helios.WriteFrame(j, PPS, HELIOS_FLAGS_DEFAULT,
                        &frame[i % NUM_FRAMES][0], PTS_TOT);
      frameSuccess = true;
      if (i % 100 == 0) {
        stopFrame = chrono::high_resolution_clock::now();
        fps = 100. / ((chrono::duration_cast
                     <std::chrono::milliseconds>
                     (stopFrame - startFrame)).count() / 1000.);
        startFrame = chrono::high_resolution_clock::now();
        cout << "Avg buffer reads per frame: "
             << readCount / 100. << endl;
        cout << "Avg buffer latency (ms): "
             << avgLatency / (float)numLatency << endl;
        cout << "Last buffer latency (ms): "
             << (float)lastLatency / 1000. << endl;
        cout << "FPS: " << fps << endl << endl;
        avgLatency = 0;
        readCount = 0;
        numLatency = 0;
      }
    }
  }
  // freeing connection
  // helios.CloseDevices();
}
