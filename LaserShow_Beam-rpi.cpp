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

#include <stdlib.h>
#include <unistd.h>
#include <ola/DmxBuffer.h>
#include <ola/Logging.h>
#include <ola/client/StreamingClient.h>
#include <iostream>

#include "HeliosDac.h"

#define SCALE 1.
#define DECAY .06
#define PCT_THRESH .15
#define SCAN_TME_LIM 3000
#define SAMP_RATIO .2
#define COOLDOWN 10
#define COOLDOWN_DMX 90
#define MIN_VOL_PCT .01
#define MIN_FR_MAX 0.1
#define ROTATE_RATE 200
#define MAX_OVER_FRAMES 1800

#define NUM_ENTS 4
#define TRAVEL_FRAMES 60

#define SAMPLE_RATE    11025
#define TOT_SAMPLES    2048
#define N_FREQS (TOT_SAMPLES / 2)
#define BUFF_SZE       80
#define BASS_CUTOFF_HZ 80.
#define BASS_OFFSET   (BASS_CUTOFF_HZ * (float)TOT_SAMPLES / (float)SAMPLE_RATE)
#define TREB_OFFSET (1000. * (float)TOT_SAMPLES / (float)SAMPLE_RATE)


#define NUM_FRAMES 1
#define PPS 20000
#define BLANKING 10
#define GRAPH_SHARP BLANKING


#define PTS_TOT 400

#define E  2.71281
#define PI 3.14159
#define REAL 0
#define IMAG 1

#define PTS_SUB (int)((float)PTS_TOT * SAMP_RATIO)

using namespace std;

int UNIVERSE = 1;
ola::DmxBuffer BUFFER; // A DmxBuffer to hold the data.
ola::client::StreamingClient OLA_CLIENT(
  (ola::client::StreamingClient::Options())
);

HeliosPoint  frame[NUM_FRAMES][PTS_TOT];
fftw_complex SIGNAL[TOT_SAMPLES];
double       PREV_ARR[PTS_TOT];
double       LST_ARR[PTS_TOT];
double       PREV_ARR_BASS[PTS_SUB];
double       LST_ARR_BASS[PTS_SUB];
double       PREV_ARR_TREB[PTS_SUB];
double       LST_ARR_TREB[PTS_SUB];
double       PREV_MAXS[MAX_OVER_FRAMES];
double       PREV_MAXS_BASS[MAX_OVER_FRAMES];
double       PREV_MAXS_TREB[MAX_OVER_FRAMES];
double       SHIFT_SIG[TOT_SAMPLES];
double       LST_GLOB_MAX;
double       CUR_GLOB_MAX;
double       LST_BASS_MAX;
double       CUR_BASS_MAX = -1;
double       LST_TREB_MAX;
double       CUR_TREB_MAX = -1;
int          PREV_MAXS_END = 0;
int          ARR_END = 0;
bool         ARR_INIT = false;
int          COOLDOWN_IDX_BASS = 0;
int          COOLDOWN_IDX_TREB = 0;
int          COOLDOWN_IDX_BASS_DMX = 0;
int          COOLDOWN_IDX_TREB_DMX = 0;
bool         INC_FLG = false;
double       CUR_VOL = 0;

double       BASS_ARR[PTS_SUB];
double       TREB_ARR[PTS_SUB];


float LINE_STARTS[NUM_ENTS][2] {{0,0},{0,1},{1,0},{1,1}};
float LINE_ENDS[NUM_ENTS][2] {{1,1},{1,0},{0,1},{0,0}};
float LINE_COLORS[NUM_ENTS] {.2,.6,.8,1.};
float LINE_FILL[NUM_ENTS]  {1,1,1,1};
bool LINE_FILL_TOG[NUM_ENTS] {false,false,false,false};
bool LINE_REVERSE[NUM_ENTS] {false,false,false,false};
float LINE_PARA[NUM_ENTS] {0,0,0,0};
int CHG_LINE_PTH = 0;
int CHG_LINE_FILL = 0;
int PREV_X = 0;
int PREV_Y = 0;


 // std::chrono::high_resolution_clock::time_point latencySum;

void rescaleXY (int &x, int &y, float cusScale = SCALE) {
  x = 0xFFF - x;
  y = 0xFFF - y;
  x = x * 0.8 * cusScale;
  y = y * cusScale + (1- cusScale)/2. * 0xFFF;;
  if (x > .8 * 0xFFF)
    x = .8 * 0xFFF;
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
    y = y + (sin((float)i / (float)rate * 2. * PI))
        * 0xFFF / 3.
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
  double maxAmpBass = 0.;
  double maxAmpTreb = 0.;
  double globalMax = 1.;
  double globalMaxBass = 1.;
  double globalMaxTreb = 1.;
  double frMax = 1.;
  double frMaxBass = 1.;
  double frMaxTreb = 1.;
  int arrIdx;
  int lastIdx = 0;
  double magVal;
  int grMaxCnt;
  int grMaxCntBass;
  int grMaxCntTreb;
  double diff;
  fftw_complex result[TOT_SAMPLES];
  fftw_plan plan = fftw_plan_dft_1d(TOT_SAMPLES,
                                    SIGNAL,
                                    result,
                                    FFTW_FORWARD,
                                    FFTW_ESTIMATE);
  fftw_execute(plan);

  for (int i = 0; i<PTS_TOT; i++ ) {
    arrIdx = (int)(pow((float)i / (float)PTS_TOT, E)
             * ((float)(TOT_SAMPLES / 2 - BASS_OFFSET - TREB_OFFSET))) + BASS_OFFSET;
    magVal = getAvg(result, lastIdx, arrIdx);
    lastIdx = arrIdx;
    magVal = pow(magVal, .75 + (.25 * ((float)(i) / (float)PTS_TOT)));
    if (!(magVal >= 0))
      magVal = 1.;
    if (magVal > maxAmp) {
      maxAmp = magVal;
    }
    if (i < PTS_SUB){
      if (magVal > maxAmpBass) {
        maxAmpBass = magVal;
      }
    }
    if (i >= PTS_TOT - PTS_SUB){
      if (magVal > maxAmpTreb) {
        maxAmpTreb = magVal;
      }
    }
    freq_arr[i] = magVal;
  }

  PREV_MAXS[PREV_MAXS_END] = maxAmp;
  PREV_MAXS_BASS[PREV_MAXS_END] = maxAmpBass;
  PREV_MAXS_TREB[PREV_MAXS_END] = maxAmpTreb;

  PREV_MAXS_END = (PREV_MAXS_END + 1) % MAX_OVER_FRAMES;
  double prevAvg = 0;
  bool notInit = false;
  for (i = (PREV_MAXS_END + 1) % MAX_OVER_FRAMES; i != int(PREV_MAXS_END + MAX_OVER_FRAMES - MAX_OVER_FRAMES * .1 ) % MAX_OVER_FRAMES; i++){
    i %= MAX_OVER_FRAMES;
    if (PREV_MAXS[i] == 1.) {
      notInit = true;
      break;
    }
    prevAvg += PREV_MAXS[i];
    if (i == int(PREV_MAXS_END + MAX_OVER_FRAMES - MAX_OVER_FRAMES * .1 ) % MAX_OVER_FRAMES ) break;
  }
  if (!notInit) {
    prevAvg /= MAX_OVER_FRAMES * .9;
    
    double curAvg = 0;
    for (i = int(PREV_MAXS_END + MAX_OVER_FRAMES - MAX_OVER_FRAMES * .1 ) % MAX_OVER_FRAMES; i != (PREV_MAXS_END + 1) % MAX_OVER_FRAMES; i++){
      i %= MAX_OVER_FRAMES;
      curAvg += PREV_MAXS[i];
      if (i == (PREV_MAXS_END + 1) % MAX_OVER_FRAMES ) break;
    }
    curAvg /= MAX_OVER_FRAMES * .1;
    CUR_VOL = min(curAvg / prevAvg, 1.);
  }
  else {
    CUR_VOL = 1.;
  }
  // cout << CUR_VOL << endl;

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
    if (PREV_MAXS_BASS[i] > globalMaxBass){
      grMaxCntBass = 0;
      for (int j = i; j < MAX_OVER_FRAMES; j++){
        if (PREV_MAXS_BASS[j] >= PREV_MAXS_BASS[i])
          grMaxCntBass++;
      }
      if (grMaxCntBass >= (float)MAX_OVER_FRAMES * MIN_FR_MAX)
        globalMaxBass = PREV_MAXS_BASS[i];
    }
    if (PREV_MAXS_TREB[i] > globalMaxTreb){
      grMaxCntTreb = 0;
      for (int j = i; j < MAX_OVER_FRAMES; j++){
        if (PREV_MAXS_TREB[j] >= PREV_MAXS_TREB[i])
          grMaxCntTreb++;
      }
      if (grMaxCntTreb >= (float)MAX_OVER_FRAMES * MIN_FR_MAX)
        globalMaxTreb = PREV_MAXS_TREB[i];
    }
//      globalMax += PREV_MAXS[i];
  }
//  globalMax /= (double)MAX_OVER_FRAMES;

  for (i = 0; i < PTS_TOT; i++){
    if (freq_arr[i] > frMax)
      frMax = freq_arr[i];
    if (i < PTS_SUB && freq_arr[i] > frMaxBass){
      frMaxBass = freq_arr[i];
    }
    if (i > PTS_TOT - PTS_SUB && freq_arr[i] > frMaxTreb){
      frMaxTreb = freq_arr[i];
    }
  }
  globalMax = max(globalMax, frMax);
  globalMaxBass = max(globalMaxBass, frMaxBass);
  globalMaxTreb = max(globalMaxTreb, frMaxTreb);

  if (globalMax < 1)
    globalMax = 1.;
  if (globalMaxBass < 1)
    globalMaxBass = 1.;
  if (globalMaxTreb < 1)
    globalMaxTreb = 1.;

  if(CUR_BASS_MAX == -1){
    CUR_BASS_MAX = globalMaxBass;
  }
  LST_BASS_MAX = CUR_BASS_MAX;
  CUR_BASS_MAX = globalMaxBass;

  if(CUR_TREB_MAX == -1){
    CUR_TREB_MAX = globalMaxTreb;
  }
  LST_TREB_MAX = CUR_TREB_MAX;
  CUR_TREB_MAX = globalMaxTreb;

  if(CUR_GLOB_MAX == -1){
    CUR_GLOB_MAX = globalMax;
  }
  LST_GLOB_MAX = CUR_GLOB_MAX;
  CUR_GLOB_MAX = globalMax;

//  cout << globalMax << endl;
  for (i = 0; i < PTS_TOT; ++i) {
//    freq_arr[i] = min(freq_arr[i] / globalMax, 1.);

    // Treble
    if (i >= PTS_TOT - PTS_SUB){
      int locIterTreb = i - (PTS_TOT - PTS_SUB);
      if (frMaxTreb > MIN_VOL_PCT * (float)0xFFFF)
        TREB_ARR[locIterTreb] = freq_arr[i] / globalMaxTreb;
      else
        TREB_ARR[locIterTreb] = 0;
      diff = PREV_ARR_TREB[locIterTreb] - TREB_ARR[locIterTreb];

      if (diff > 0) {
        TREB_ARR[locIterTreb] = PREV_ARR_TREB[locIterTreb] - diff * DECAY;
      }
      LST_ARR_TREB[locIterTreb] = PREV_ARR_TREB[locIterTreb];
      PREV_ARR_TREB[locIterTreb] = TREB_ARR[locIterTreb];

    }
    // Bass
    if (i < PTS_SUB){
      if (frMaxBass > MIN_VOL_PCT * (float)0xFFFF)
        BASS_ARR[i] = freq_arr[i] / globalMaxBass;
      else
        BASS_ARR[i] = 0;
      diff = PREV_ARR_BASS[i] - BASS_ARR[i];

      if (diff > 0) {
        BASS_ARR[i] = PREV_ARR_BASS[i] - diff * DECAY;
      }
      LST_ARR_BASS[i] = PREV_ARR_BASS[i];
      PREV_ARR_BASS[i] = BASS_ARR[i];
    }

    // Global
    if (frMax > MIN_VOL_PCT * (float)0xFFFF)
      freq_arr[i] = freq_arr[i] / globalMax;
    else
      freq_arr[i] = 0;
    double diff = PREV_ARR[i] - freq_arr[i];

    if (diff > 0) {
      freq_arr[i] = PREV_ARR[i] - diff * DECAY;
    }

    LST_ARR[i] = PREV_ARR[i];
    PREV_ARR[i] = freq_arr[i];
  }
  fftw_destroy_plan(plan);
}

void HSVtoRGB(float H, float S, float V, int& r, int& g, int& b) {
  if(H>360 || H<0 || S>100 || S<0 || V>100 || V<0) {
      cout << "The given HSV values are not in valid range" << endl;
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

bool isPeak(int freq, double * freq_arr){
  bool peak = true;
  for (int i = 1; i < 10; i++){
    if (
      freq_arr[freq - i] > freq_arr[freq] || 
      freq_arr[freq + i] > freq_arr[freq])
      peak = false;
  }
  return peak;
}

void genSpect(int i, float loopCount, double * freq_arr,
              int& x, int& y, int& r, int& g, int& b){
  float hue;
  if (i < PTS_TOT - 10 && i > 10){
    x = i / (float) PTS_TOT * 0xFFF;
    hue = (int)(i / (float)PTS_TOT * 360. + loopCount * 2.) % 360;
    // if (isPeak(i, freq_arr))
    //   PREV_Y = freq_arr[min((int)(i * (PTS_TOT / (float)(PTS_TOT - 10))), PTS_TOT - 1)] * 0xFFF;
    // y = PREV_Y;
    y = freq_arr[min((int)(i * (PTS_TOT / (float)(PTS_TOT - 10))), PTS_TOT - 1)] * 0xFFF;
    HSVtoRGB(hue, 100., 100., r, g, b);
  }
  else{
    y = freq_arr[10]* 0xFFF;
    x = 0;
    HSVtoRGB(0., 100., 0., r, g, b);
  }
}

float SYM_HUE = 0.;

void genSym(int i, float loopCount, bool bassFlg, bool trebFlg, double curBass, double curTreb, double * freq_arr,
              int& x, int& y, int& r, int& g, int& b){
  float fx;
  y = 0xFFF / 2;
  float sX = 0xFFF / 2 - 0xFFF / 2 * curBass;
  float eX = (0xFFF / 2 - sX) * (curTreb) + sX;
  if (i < PTS_TOT / 2.){
    fx = sX * (1. - i / (float)(PTS_TOT / 2.)) + eX * (i / (float)(PTS_TOT / 2.));
  }
  else{
    fx = (0xFFF - eX) * (1. - i / (float)(PTS_TOT)) + (0xFFF - sX) * (i / (float)(PTS_TOT));
  }
  if (bassFlg || trebFlg)
    SYM_HUE = rand() % 360;
  x = fx;
  HSVtoRGB(SYM_HUE, 100., 100., r, g, b);
  rotateXY (x, y, loopCount, 120, false);
}

double lstBass = 0;
double lstTreb = 0;

void genCirclePts(int i, float loopCount, double curBass, double curTreb, double * freq_arr,
              int& x, int& y, int& r, int& g, int& b){
  int circPts = 40;
  int blanking = 6;
  int chgBlanking = 10;
  float rad;


  // float rad = ((sin(loopCount/100.)+1.)/2.+magVal * (sin(PI+loopCount/100.))) / 2;
  int locIter = (i - chgBlanking * 2) * circPts / ((PTS_TOT - chgBlanking * 2) / 2);
  // cout << locIter << endl;
  if (i < PTS_TOT / 2){
    rad = 1 - curBass;
    loopCount = -1 * loopCount;
  }
  else{
    rad = curTreb * .8 + .2;
  }
  y = (sin((locIter + (loopCount / 8.))/(float)circPts * 2*PI) / 2. * rad + .5)  * 0xFFF;
  x = (cos((locIter + (loopCount / 8.))/(float)circPts * 2*PI) / 2. * rad + .5) * 0xFFF;
  
  int hue = (int)((float)locIter / (circPts / 2) * 360.) % 360;
  if (i % (int)(PTS_TOT / (float)circPts) < blanking || (i - PTS_TOT / 2 < chgBlanking && i > PTS_TOT / 2) || i < chgBlanking) {
    HSVtoRGB(90, 100., 0., r, g, b);
  }
  else{
    HSVtoRGB(hue, 100., 100., r, g, b);
  }
}

void genLines(bool& genFlg, bool& chgColor, int i, float loopCount, double * freq_arr,
              int& x, int& y, int& r, int& g, int& b) {
  int hue;
  float rxs;
  float rys;
  float rxe;
  float rye;
  float fxs;
  float fys;
  float fxe;
  float fye;
  float fx;
  float fy;
  float lineIter;

  // float rad = ((sin(loopCount/100.)+1.)/2.+magVal * (sin(PI+loopCount/100.))) / 2;
  // y = freq_arr[i] * 0xFFF;
  // x = i / (float)PTS_TOT * 0xFFF;

  int entIdx = i*NUM_ENTS / PTS_TOT;
  // float locSpeed = TRAVEL_FRAMES / min(max((CUR_VOL), .2),2.);

  if (i*NUM_ENTS % PTS_TOT == 0){
    LINE_FILL_TOG[entIdx] = false;
  }

  if (loopCount - LINE_PARA[entIdx] > TRAVEL_FRAMES){
    if (LINE_REVERSE[entIdx]){
      LINE_REVERSE[entIdx] = false;
    }
    else{
      LINE_REVERSE[entIdx] = true;
    }
    LINE_PARA[entIdx] = loopCount;
  }


  if (genFlg && i*NUM_ENTS % PTS_TOT == 0){
    LINE_REVERSE[entIdx] = !LINE_REVERSE[entIdx];
    LINE_PARA[entIdx] = loopCount  - (TRAVEL_FRAMES - (loopCount - LINE_PARA[entIdx]));
  }

  if (LINE_REVERSE[entIdx]){
    lineIter = TRAVEL_FRAMES - (loopCount - LINE_PARA[entIdx]);
  }
  else {
    lineIter = loopCount - LINE_PARA[entIdx];
  }

  // float locSpeed = TRAVEL_FRAMES;
  fxs = LINE_STARTS[entIdx][0] * (1. - (float)lineIter / TRAVEL_FRAMES) + LINE_ENDS[entIdx][0] 
      * ((float)lineIter  / TRAVEL_FRAMES);
  fys = LINE_STARTS[entIdx][1] * (1. - (float)lineIter / TRAVEL_FRAMES) + LINE_ENDS[entIdx][1] 
      * ((float)lineIter  / TRAVEL_FRAMES);

  fxe = LINE_STARTS[(entIdx+1)%NUM_ENTS][0] * (1. - (float)lineIter / TRAVEL_FRAMES) + LINE_ENDS[(entIdx+1)%NUM_ENTS][0] 
      * ((float)lineIter  / TRAVEL_FRAMES);
  fye = LINE_STARTS[(entIdx+1)%NUM_ENTS][1] * (1. - (float)lineIter / TRAVEL_FRAMES) + LINE_ENDS[(entIdx+1)%NUM_ENTS][1] 
      * ((float)lineIter  / TRAVEL_FRAMES);

  fx = fxs * (1. - float(i % (PTS_TOT / NUM_ENTS)) / (PTS_TOT / NUM_ENTS)) + fxe * float(i % (PTS_TOT / NUM_ENTS)) / (PTS_TOT / NUM_ENTS);
  fy = fys * (1. - float(i % (PTS_TOT / NUM_ENTS)) / (PTS_TOT / NUM_ENTS)) + fye * float(i % (PTS_TOT / NUM_ENTS)) / (PTS_TOT / NUM_ENTS);


  if (genFlg && entIdx == CHG_LINE_PTH) {
    // LINE_STARTS[CHG_LINE][0] = fx;
    // LINE_STARTS[CHG_LINE][1] = fy;
    // rxs = static_cast <float> (rand()) / static_cast <float> (RAND_MAX) / 3.;
    // rys = static_cast <float> (rand()) / static_cast <float> (RAND_MAX) / 3.;
    // rxe = static_cast <float> (rand()) / static_cast <float> (RAND_MAX) / 3.;
    // rye = static_cast <float> (rand()) / static_cast <float> (RAND_MAX) / 3.;
    // rxs = rxs + (rand() % 2) * (2./3.);
    // rxe = rxe + (rand() % 2) * (2./3.);
    // rys = rys + (rand() % 2) * (2./3.);
    // rye = rye + (rand() % 2) * (2./3.);
    if (entIdx % 2){
      rxs = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
      rxe = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
      rys = static_cast <float> (rand() % 2);
      rye = (float)(((int)rys + 1) % 2);
    }
    else{
      rys = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
      rye = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
      rxs = static_cast <float> (rand() % 2);
      rxe = (float)(((int)rys + 1) % 2);
    }
    LINE_STARTS[CHG_LINE_PTH][0] = rxs;
    LINE_STARTS[CHG_LINE_PTH][1] = rys;
    LINE_ENDS[CHG_LINE_PTH][0] = rxe;
    LINE_ENDS[CHG_LINE_PTH][1] = rye;
    LINE_PARA[CHG_LINE_PTH] = loopCount + 1;

    LINE_COLORS[CHG_LINE_PTH] = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
    LINE_FILL[CHG_LINE_PTH] = max(static_cast <float> (rand()) / static_cast <float> (RAND_MAX), (float).1);

    CHG_LINE_PTH = (CHG_LINE_PTH + 1) % NUM_ENTS;
    genFlg = false;
  }
  if (chgColor && entIdx == CHG_LINE_FILL){
    LINE_COLORS[CHG_LINE_FILL] = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
    // LINE_FILL[(CHG_LINE_FILL + NUM_ENTS - 1) % NUM_ENTS] = 1.;
    LINE_FILL[CHG_LINE_FILL] = max(static_cast <float> (rand()) / static_cast <float> (RAND_MAX), (float).1);
    CHG_LINE_FILL = (CHG_LINE_FILL + 1) % NUM_ENTS;
    chgColor = false;
  }
  x = fx * 0xFFF;
  y = fy * 0xFFF;
  // hue = (int)((float)i / (float)(PTS_TOT - 1) * 360.
  //     * 2. + loopCount * 2.) % 360;
  hue = LINE_COLORS[entIdx] * 360.;

  if (i * 2 % (int)((PTS_TOT / NUM_ENTS) * LINE_FILL[entIdx]) == 0){
    LINE_FILL_TOG[entIdx] = !LINE_FILL_TOG[entIdx];
  }


  if (LINE_FILL_TOG[entIdx]){
    HSVtoRGB(hue, 100., 100., r, g, b);

  }
  else{
    HSVtoRGB(hue, 100., 0., r, g, b);

  }
  
}

#define SHOT_FRAMES 30.
int SHOT_ST = 0 - SHOT_FRAMES;


int SCAN_ST = 0;
int SCAN_SEL = -1;
bool SCAN_REV = false;
int SCAN_COL_OFFSET;
int MODE_SEL = 0;

#define SCAN_FRMS 30.

void shotgun(int i, float loopCount, int& x, int& y, int& r, int& g, int& b){
  if (i % 20 == 10){
      HSVtoRGB( (float)(rand() % 360), 100., 100., r, g, b);
  } 
  else if (i % 20 == 15){
    HSVtoRGB(0., 100., 100., r, g, b);
  } 
  else if (i % 20 == 0){
    PREV_X = rand() % 0xFFF;
    PREV_Y = rand() % 0xFFF;
    HSVtoRGB( 0., 100., 0., r, g, b);
  }
  x = PREV_X;
  y = PREV_Y;
}

void HScan(int i, float loopCount, int& x, int& y, int& r, int& g, int& b){
  //loopCount /= CUR_VOL;
  if (loopCount - SCAN_ST > SCAN_FRMS)
    SCAN_ST = loopCount;
  if ((int)loopCount % 2 == 0)
    i = PTS_TOT - i;
  if (!SCAN_REV)
    y = i / (float)PTS_TOT * 0xFFF;
  else
    y = (PTS_TOT - i) / (float)PTS_TOT * 0xFFF;
  int locIter = loopCount - SCAN_ST;
  if (locIter < SCAN_FRMS / 2)
    x = locIter * 2 / SCAN_FRMS * 0xFFF;
  else
    x = (SCAN_FRMS - (locIter - SCAN_FRMS / 2) * 2) / SCAN_FRMS * 0xFFF;
  if (i > 10 && i < PTS_TOT - 10 && (i < PTS_TOT / 3. || i > PTS_TOT - (PTS_TOT / 3.)))
    HSVtoRGB((int)(x / (float)0xFFF * 360. + SCAN_COL_OFFSET) % 360, 100., 100., r, g, b);
  else
    HSVtoRGB(0., 100., 0., r, g, b);
  if (locIter == SCAN_FRMS)
    SCAN_SEL = -1;
}

void VScan(int i, float loopCount, int& x, int& y, int& r, int& g, int& b){
  // loopCount /= CUR_VOL;
  if (loopCount - SCAN_ST > SCAN_FRMS)
    SCAN_ST = loopCount;
  if ((int)loopCount % 2 == 0)
    i = PTS_TOT - i;
  if (!SCAN_REV)
    x = i / (float)PTS_TOT * 0xFFF;
  else
    x = (PTS_TOT - i) / (float)PTS_TOT * 0xFFF;
  int locIter = loopCount - SCAN_ST;
  if (locIter < SCAN_FRMS / 2)
    y = locIter * 2 / SCAN_FRMS * 0xFFF;
  else
    y = (SCAN_FRMS - (locIter - SCAN_FRMS / 2) * 2) / SCAN_FRMS * 0xFFF;
  if (i > 10 && i < PTS_TOT - 10 && (i < PTS_TOT / 3. || i > PTS_TOT - (PTS_TOT / 3.)))
    HSVtoRGB((int)(y / (float)0xFFF * 360. + SCAN_COL_OFFSET) % 360, 100., 100., r, g, b);
  else
    HSVtoRGB(0., 100., 0., r, g, b);
  if (locIter == SCAN_FRMS)
    SCAN_SEL = -1;
}

#define DMX_PTS 240
int DMX_CIRC[2][DMX_PTS];
int LST_CIRC_IDX = 0;
#define MIN_CIRC 85
#define PUSH_CENT 32
int randRadiusX;
int randRadiusY;
bool DMX_REVERSE = false;

void calcDmxCirc(){
  int randOffsetX = static_cast <int> (rand()) % (128);
  randRadiusX = min(max(static_cast <int> (rand()) % (255 - randOffsetX), MIN_CIRC), 255 - randOffsetX);
  
  int randOffsetY = max(static_cast <int> (rand()) % (128), PUSH_CENT);
  if(128 - randOffsetY < 16){
      randOffsetY = 128 - 16;
  }
  do {randRadiusY = min(max(static_cast <int> (rand()) % (255 - randOffsetY), MIN_CIRC), 255 - randOffsetY - PUSH_CENT);}
  while(abs(randOffsetY + randRadiusY - 127.5) < 64);

  if(!DMX_REVERSE){
    for(int i=0; i<DMX_PTS; i++){
      DMX_CIRC[0][i] = (sin(2*PI * (i / (float)DMX_PTS)) / 2. + .5) * (float)randRadiusX + randOffsetX;
    }
    for(int i=0; i<DMX_PTS; i++){
      DMX_CIRC[1][i] = (cos(2*PI * (i / (float)DMX_PTS)) / 2. + .5) * (float)randRadiusY + randOffsetY;
    }
  }
  else{
    for(int i=DMX_PTS; i>0; --i){
      DMX_CIRC[0][i] = (sin(2*PI * (i / (float)DMX_PTS)) / 2. + .5) * (float)randRadiusX + randOffsetX;
    }
    for(int i=0; i<DMX_PTS; i++){
      DMX_CIRC[1][i] = (cos(2*PI * (i / (float)DMX_PTS)) / 2. + .5) * (float)randRadiusY + randOffsetY;
    }
  }
  DMX_REVERSE = !DMX_REVERSE;
  LST_CIRC_IDX = static_cast <int> (rand()) % DMX_PTS;
  // cout << randOffsetY << '\t' <<  randRadiusY << endl;

}

std::chrono::high_resolution_clock::time_point LAST_DMX_MOVE;
std::chrono::high_resolution_clock::time_point LAST_DMX_LIGHT;
int DMX_COLOR = 0;
int DMX_PAT = 0;

void genDmx(bool& bassFlg, bool& trebFlg, double bassMaxCur, double trebMaxCur, 
            float loopCount, double * freq_arr){
  std::chrono::high_resolution_clock::time_point cur_tme;
  cur_tme = chrono::high_resolution_clock::now();
  long int sinceLastLight;
  long int sinceLastMove;
  int brightnessBass;
  int brightnessTreb;
  float whiteDiv = 1.;

  sinceLastMove = chrono::duration_cast
            <std::chrono::microseconds>(cur_tme - LAST_DMX_MOVE).count();
  sinceLastLight = chrono::duration_cast
            <std::chrono::microseconds>(cur_tme - LAST_DMX_LIGHT).count();
  if(sinceLastLight > 25000){
    LAST_DMX_LIGHT = cur_tme;
    if (DMX_COLOR == 0){
      whiteDiv = 2.;
    }
    brightnessBass = min(pow(bassMaxCur, 2.) * 255. / whiteDiv, 255.);
    brightnessTreb = min(pow(trebMaxCur, 2.) * 255. / whiteDiv, 255.);
    // brightnessBass = bassMaxCur * 255.;
    // brightnessTreb = trebMaxCur * 255.;
    BUFFER.SetChannel(7, brightnessBass);
    BUFFER.SetChannel(18, brightnessTreb);

    // cout << brightnessBass << endl;

    if(sinceLastMove > max((int)(100000. * (max(randRadiusX, randRadiusY) / (float)(256 - MIN_CIRC))), 25000)){
      LAST_DMX_MOVE = cur_tme;
      // double maxAmp = 0;
      // double domFreq = 0;
      // for(int i = 0; i < PTS_TOT; i++){
      //   if(freq_arr[i] > maxAmp){
      //     maxAmp = freq_arr[i];
      //     domFreq = i;
      //   }
      // }
      // int dmxTilt = domFreq / (float) PTS_TOT * 255.;
      if(bassFlg){
        calcDmxCirc();
        bassFlg = false;
        // int randVal = static_cast <int> (rand()) % 256;
        DMX_COLOR += 10;
        DMX_COLOR %= 140;
        DMX_PAT += 8;
        DMX_PAT %= 64;
        BUFFER.SetChannel(4, DMX_COLOR);
        BUFFER.SetChannel(5, DMX_PAT);
        BUFFER.SetChannel(15, DMX_COLOR);
        BUFFER.SetChannel(16, DMX_PAT);
      }
      // cout << sinceLast << endl;

      LST_CIRC_IDX = (int)(round(LST_CIRC_IDX + 8.* CUR_VOL)) % DMX_PTS;
      BUFFER.SetChannel(0, DMX_CIRC[0][LST_CIRC_IDX]);
      BUFFER.SetChannel(2, DMX_CIRC[1][LST_CIRC_IDX]);
      BUFFER.SetChannel(11, DMX_CIRC[0][LST_CIRC_IDX]);
      BUFFER.SetChannel(13, DMX_CIRC[1][LST_CIRC_IDX]);

    }
    if (!OLA_CLIENT.SendDmx(UNIVERSE, BUFFER)) {
      cout << "Send DMX failed" << endl;
    }
  }
}


bool bassFlgDmx = false;
bool trebFlgDmx = false;
double LST_BASS_DIFFS[3] = {0,0,0};
double LST_TREB_DIFFS[3] = {0,0,0};
int LST_BASS_DIFFS_END = 0;
int LST_TREB_DIFFS_END = 0;
std::chrono::high_resolution_clock::time_point TmeBassLst;
std::chrono::high_resolution_clock::time_point TmeBassNow;
std::chrono::high_resolution_clock::time_point TmeTrebLst;
std::chrono::high_resolution_clock::time_point TmeTrebNow;

void getFrames(float loopCountF)
{
  int loopCount = (int)loopCountF;
  int x = 0;
  int y = 0;
  int r = 0;
  int g = 0;
  int b = 0;
  int hue;
  int lastIdx;
  int locIter;
  double freq_arr[N_FREQS];
  double magAvg = 0.;
  double prevLocMax = 0.;
  double curMax = 0.;
  bool genFlg = false;
  bool bassFlg = false;
  bool trebFlg = false;
  bool doScan = false;
  bool doShot = false;

  double diffAvg;

  getFreqArr(freq_arr);

  // diffAvg = 0;
  // for (int i = 0; i < PTS_TOT * SAMP_RATIO; i++) {
  //   diffAvg += max(freq_arr[i] - LST_ARR[i], 0.);
  //   // diffAvg += freq_arr[i] - LST_ARR[i];
  // }
  // diffAvg /= (float)PTS_TOT * SAMP_RATIO;
  // if (diffAvg > PCT_THRESH){
  //   bassFlg = true;
  // }

  // diffAvg = 0;
  // for (int i = (1. - SAMP_RATIO) * PTS_TOT; i < PTS_TOT; i++) {
  //   diffAvg += max(freq_arr[i] - LST_ARR[i], 0.);
  //   // diffAvg += freq_arr[i] - LST_ARR[i];
  // }
  // diffAvg /= (float)PTS_TOT * SAMP_RATIO;
  // if (diffAvg > PCT_THRESH){
  //   trebFlg = true;
  // }

  double bassMaxCur = 0;
  double bassMaxPrev = 0;
  double trebMaxCur = 0;
  double trebMaxPrev = 0;

  double bassMaxCurGlob = 0;
  double trebMaxCurGlob = 0;
  
  double bassAvgCurGlob = 0;
  double trebAvgCurGlob = 0;

  double bassAvgCur = 0;
  double trebAvgCur = 0;
  double bassAvgPrev = 0;
  double trebAvgPrev = 0;

  double curBass;
  double curTreb;

  // curBass = (curBass * .2 + bassAvg * .5 + lstBass * .4);
  // curBass = (bassAvg * .1 + lstBass * .9);
  // curTreb = (curTreb * .2 + trebAvg * .5 + lstTreb * .4);
  // lstBass = curBass;
  // lstTreb = curTreb;
  // cout << curBass << endl;

  for (int i = 0; i < PTS_SUB; i++){
    if (BASS_ARR[i] > bassMaxCur){
      bassMaxCur = BASS_ARR[i];
    }
    if (LST_ARR_BASS[i] > bassMaxPrev){
      bassMaxPrev = LST_ARR_BASS[i];
    }
    if (freq_arr[i] > bassMaxCurGlob){
      bassMaxCurGlob = freq_arr[i];
    }
    if (TREB_ARR[i] > trebMaxCur){
      trebMaxCur = TREB_ARR[i];
    }
    if (LST_ARR_TREB[i] > trebMaxPrev){
      trebMaxPrev = LST_ARR_TREB[i];
    }
    if (freq_arr[i + (PTS_TOT - PTS_SUB)] > trebMaxCurGlob){
      trebMaxCurGlob = freq_arr[i];
    }
  }

  int bCurCnt = 0;
  int bCurCntGlob = 0;
  int bPreCnt = 0;
  int tCurCnt = 0;
  int tCurCntGlob = 0;
  int tPreCnt = 0;
  double avgThresh = 0.;
  for (int i = 0; i < PTS_SUB; i++){
    if (BASS_ARR[i] > avgThresh * bassMaxCur){
      bassAvgCur += BASS_ARR[i];
      bCurCnt++;
    }
    if (freq_arr[i] > avgThresh * bassMaxCurGlob){
      bassAvgCurGlob += freq_arr[i];
      bCurCntGlob++;
    }
    if (LST_ARR_BASS[i] > avgThresh * bassMaxPrev){
      bassAvgPrev += LST_ARR_BASS[i];
      bPreCnt++;}
    if (TREB_ARR[i] > avgThresh * trebMaxCur){
      trebAvgCur += TREB_ARR[i];
      tCurCnt++;}
    if (freq_arr[i + (PTS_TOT - PTS_SUB)] > avgThresh * trebMaxCurGlob){
      trebAvgCurGlob += freq_arr[i];
      tCurCntGlob++;
    }
    if (LST_ARR_TREB[i] > avgThresh * trebMaxPrev){
      trebAvgPrev += LST_ARR_TREB[i];
      tPreCnt++;}
  }

  bassAvgCur /= (double)bCurCnt;
  bassAvgCurGlob /= (double)bCurCntGlob;
  bassAvgPrev /= (double)bPreCnt;
  trebAvgCur /= (double)tCurCnt;
  trebAvgCurGlob /= (double)tCurCntGlob;
  trebAvgPrev /= (double)tPreCnt;

  bassAvgPrev *= LST_BASS_MAX / CUR_BASS_MAX;
  bassMaxPrev *= LST_BASS_MAX / CUR_BASS_MAX;
  trebAvgPrev *= LST_TREB_MAX / CUR_TREB_MAX;
  trebMaxPrev *= LST_TREB_MAX / CUR_TREB_MAX;


  curBass = (bassMaxCur * .1 + bassMaxPrev * .9);
  curTreb = (trebMaxCur * .1 + trebMaxPrev * .9);


  // double bassDiff = bassMaxCur - bassMaxPrev;
  // double trebDiff = trebMaxCur - trebMaxPrev;

  double bassDiff = bassAvgCur - bassAvgPrev;
  double trebDiff = trebAvgCur - trebAvgPrev;

  LST_BASS_DIFFS[LST_BASS_DIFFS_END] = bassDiff;
  LST_BASS_DIFFS_END = (LST_BASS_DIFFS_END + 1 )% 3;

  LST_TREB_DIFFS[LST_TREB_DIFFS_END] = trebDiff;
  LST_TREB_DIFFS_END = (LST_TREB_DIFFS_END + 1 )% 3;

  bassDiff = 0;
  trebDiff = 0;
  for (int i=0; i<3; i++){
    // if(LST_BASS_DIFFS[i] > 0){
      bassDiff += LST_BASS_DIFFS[i];
      trebDiff += LST_TREB_DIFFS[i];
    // }
  }

  // double bassSanity = 0.;
  // for (int i = 0; i <= PTS_SUB/4.; i++){
  //   if (freq_arr[i + PTS_SUB] > bassSanity){
  //     bassSanity = freq_arr[i + PTS_SUB];
  //   }
  // }

  // if (bassDiff > PCT_THRESH && bassMaxCurGlob > bassSanity){
  // if (bassDiff > PCT_THRESH && bassMaxCurGlob > freq_arr[PTS_SUB]){
  if (bassDiff > PCT_THRESH){
    bassFlg = true;
    // cout << CUR_BASS_MAX << '\t' << LST_BASS_MAX << endl;
  }
  if (trebDiff > PCT_THRESH){
    trebFlg = true;
  }
  // cout << bassMaxCur << endl;

  // bassFlg = bassFlg && bCurCnt > 5 && bassMaxCurGlob > .3 && (abs(loopCount - COOLDOWN_IDX_BASS) > COOLDOWN);
  bassFlg = bassFlg && bCurCnt > 5 && (abs(loopCount - COOLDOWN_IDX_BASS) > COOLDOWN);
  // trebFlg = trebFlg && tCurCnt > 5 && trebMaxCurGlob > .3 && (abs(loopCount - COOLDOWN_IDX_TREB) > COOLDOWN);
  trebFlg = trebFlg && tCurCnt > 5 && (abs(loopCount - COOLDOWN_IDX_TREB) > COOLDOWN);
  bassFlgDmx = bassFlgDmx || (bassFlg && bassMaxCurGlob > .2 && (abs(loopCount - COOLDOWN_IDX_BASS_DMX) > COOLDOWN_DMX));
  trebFlgDmx = trebFlgDmx || (trebFlg && trebMaxCurGlob > .2 && (abs(loopCount - COOLDOWN_IDX_TREB_DMX) > COOLDOWN_DMX));



  if (loopCount - SHOT_ST < SHOT_FRAMES)
    doShot = true;

  if (bassFlg){
    // INC_FLG = true;
    // genFlg = true;
    TmeBassNow = chrono::high_resolution_clock::now();
    long int sinceBass = chrono::duration_cast
                      <std::chrono::milliseconds>(TmeBassNow - TmeBassLst).count();
    TmeBassLst = TmeBassNow;
    if (sinceBass > SCAN_TME_LIM && !doShot){
      doScan = true;
    }
    COOLDOWN_IDX_BASS = loopCount;
  }
  else{
    doScan = false;
  }

  if (SCAN_SEL == -1 && doScan && CUR_VOL > .7){
  // if (SCAN_SEL == -1 && bassMaxCur == 1){
    SCAN_SEL = rand() % 2;
    SCAN_COL_OFFSET = rand() % 360;
    SCAN_REV = !SCAN_REV;
  }

  if (doScan || doShot)
      MODE_SEL = rand() % 4;
      
  if (CUR_VOL < .2 && MODE_SEL == 3){
    MODE_SEL = rand() % 3;
  }


  if (trebFlg){
    // INC_FLG = true;
    // genFlg = true;
    TmeTrebNow = chrono::high_resolution_clock::now();
    long int sinceTreb = chrono::duration_cast
                      <std::chrono::milliseconds>(TmeTrebNow - TmeTrebLst).count();
    TmeTrebLst = TmeTrebNow;
    if (sinceTreb > SCAN_TME_LIM * 2 && SCAN_SEL == -1){
      doShot = true;
      if (loopCount - SHOT_ST > SHOT_FRAMES)
        SHOT_ST = loopCount;
    }
    COOLDOWN_IDX_TREB = loopCount;
  }

  // cout << bassFlgDmx << endl;
  if (bassFlgDmx){
    COOLDOWN_IDX_BASS_DMX = loopCount;
  }
  if (trebFlgDmx){
      COOLDOWN_IDX_TREB_DMX = loopCount;
  }
  // else if (diffAvg <= .015 && INC_FLG){
  // // else if (diffAvg <= 0 && INC_FLG){
  //   if (abs(loopCount - COOLDOWN_IDX) > 2){
  //     genFlg = true;
  //     COOLDOWN_IDX = loopCount;
  //   }
  //   INC_FLG = false;
  // }
  // cout << diffAvg << endl;

  // genDmx(bassFlgDmx, trebFlg, bassAvgCur, trebAvgCur, loopCountF, freq_arr);
  genDmx(bassFlgDmx, trebFlg, bassMaxCur, trebMaxCur, loopCountF, freq_arr);

  for (int i = 0; i < NUM_FRAMES; i++) {
    loopCount = loopCount + i;
    loopCountF = loopCountF + i;
    locIter = 0;

    if (bassFlg) {cout << "bassDiff: " << bassDiff << '\t' << "bassAvgCur: " << bassAvgCur << '\t' << "bassMaxGlob: " << bassMaxCurGlob << bCurCnt << endl;}
    // if (trebFlg) {cout << "treb: " << trebDiff << endl;}

    for (int j = 0; j < PTS_TOT; j++){


      // genLines(bassFlg, trebFlg, j, loopCountF, freq_arr, x, y, r, g, b);
      if (doShot)
        shotgun(j, loopCount, x, y, r, g, b);
      else if (SCAN_SEL == 0)
        VScan(j, loopCount, x, y, r, g, b);
      else if (SCAN_SEL == 1)
        HScan(j, loopCount, x, y, r, g, b);
      else if (MODE_SEL == 0)
        genLines(bassFlg, trebFlg, j, loopCountF, freq_arr, x, y, r, g, b);
      else if (MODE_SEL == 1)
        genCirclePts(j, loopCountF, bassAvgCur, trebAvgCur, freq_arr, x, y, r, g, b);
      else if (MODE_SEL == 2)
        genSpect(j, loopCountF, freq_arr, x, y, r, g, b);
      else if (MODE_SEL == 3)
        genSym(j, loopCountF, bassFlg, trebFlg, bassMaxCur, trebMaxCur, freq_arr, x, y, r, g, b);

      rescaleXY(x, y);
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
  float loopCount = 0;

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

  calcDmxCirc();

  ola::InitLogging(ola::OLA_LOG_WARN, ola::OLA_LOG_STDERR);
  BUFFER.Blackout(); // Set all channels to 0
  // Create a new client.

  // Setup the client, this connects to the server

  while (!OLA_CLIENT.Setup()) {
    std::cerr << "DMX Setup failed, retrying..." << endl;
    sleep(1);
  }
  unsigned int universe = 1;
  // BUFFER.SetChannel(7,0);
  // BUFFER.SetChannel(6,0);
  // BUFFER.SetChannel(9,0);
  // BUFFER.SetChannel(10,0);
  // BUFFER.SetChannel(8,0);
  LAST_DMX_LIGHT = chrono::high_resolution_clock::now();
  LAST_DMX_MOVE = chrono::high_resolution_clock::now();
  TmeBassLst = chrono::high_resolution_clock::now();
  if (!OLA_CLIENT.SendDmx(UNIVERSE, BUFFER)) {
    cout << "Send DMX failed" << endl;
  }
  sleep(1);

  for(int jj = 0; jj<TOT_SAMPLES / 2; ++jj)
    PREV_ARR[jj] = 0.;
  for(int kk = 0; kk<MAX_OVER_FRAMES; ++kk)
    PREV_MAXS[kk] = 1.;

  startFrame = chrono::high_resolution_clock::now();
  while (1)
  {
    i++;
    loopCount = loopCount + NUM_FRAMES * CUR_VOL;

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
