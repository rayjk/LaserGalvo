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
#define DECAY .06
#define PCT_THRESH .02
#define SAMP_RATIO .5
#define COOLDOWN 30
#define MIN_VOL_PCT .01
#define MIN_FR_MAX 0.1
#define ROTATE_RATE 200
#define MAX_OVER_FRAMES 1800

#define SAMPLE_RATE    12000
#define TOT_SAMPLES    1024
#define N_FREQS (TOT_SAMPLES / 2)
#define BUFF_SZE       64
#define BASS_CUTOFF_HZ 80.
#define BASS_OFFSET   (BASS_CUTOFF_HZ * (float)TOT_SAMPLES / (float)SAMPLE_RATE)

#define NUM_FRAMES 1
#define PPS 20000
#define BLANKING 10
#define GRAPH_SHARP BLANKING


#define PTS_TOT 200

#define E  2.71281
#define PI 3.14159
#define REAL 0
#define IMAG 1

using namespace std;

HeliosPoint  frame[NUM_FRAMES][PTS_TOT];
fftw_complex SIGNAL[TOT_SAMPLES];
double       PREV_ARR[N_FREQS];
double       LST_ARR[N_FREQS];
double       PREV_MAXS[MAX_OVER_FRAMES];
double       SHIFT_SIG[TOT_SAMPLES];
int          PREV_MAXS_END = 0;
int          ARR_END = 0;
bool         ARR_INIT = false;
int          COOLDOWN_IDX_BASS = 0;
int          COOLDOWN_IDX_TREB = 0;
bool         INC_FLG = false;
double       CUR_VOL = 0;


 // std::chrono::high_resolution_clock::time_point latencySum;

void rescaleXY (int &x, int &y, float cusScale = SCALE) {
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

  for (int i = 0; i<PTS_TOT; i++ ) {
    arrIdx = (int)(pow((float)i / (float)PTS_TOT, E)
             * ((float)(TOT_SAMPLES / 2 - BASS_OFFSET))) + BASS_OFFSET;
    magVal = getAvg(result, lastIdx, arrIdx);
    lastIdx = arrIdx;
    magVal = pow(magVal, .7 + (.3 * ((float)i / (float)PTS_TOT)));
    if (!(magVal >= 0))
      magVal = 1.;
    if (magVal > maxAmp) {
      maxAmp = magVal;
    }
    freq_arr[i] = magVal;
  }

  PREV_MAXS[PREV_MAXS_END] = maxAmp;
  PREV_MAXS_END = (PREV_MAXS_END + 1) % MAX_OVER_FRAMES;
  double prevAvg = 0;
  for (i = (PREV_MAXS_END + 1) % MAX_OVER_FRAMES; i != int(PREV_MAXS_END + MAX_OVER_FRAMES - MAX_OVER_FRAMES * .2 ) % MAX_OVER_FRAMES; i++){
    i %= MAX_OVER_FRAMES;
    prevAvg += PREV_MAXS[i];
    if (i == int(PREV_MAXS_END + MAX_OVER_FRAMES - MAX_OVER_FRAMES * .2 ) % MAX_OVER_FRAMES ) break;
  }
  prevAvg /= MAX_OVER_FRAMES * .8;
  
  double curAvg = 0;
  for (i = int(PREV_MAXS_END + MAX_OVER_FRAMES - MAX_OVER_FRAMES * .2 ) % MAX_OVER_FRAMES; i != (PREV_MAXS_END + 1) % MAX_OVER_FRAMES; i++){
    i %= MAX_OVER_FRAMES;
    curAvg += PREV_MAXS[i];
    if (i == (PREV_MAXS_END + 1) % MAX_OVER_FRAMES ) break;
  }
  curAvg /= MAX_OVER_FRAMES * .2;
  CUR_VOL = curAvg / prevAvg;
  // cout << prevAvg << endl;

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

  for (i = 0; i < PTS_TOT; i++){
    if (freq_arr[i] > frMax)
      frMax = freq_arr[i];
  }
  globalMax = max(globalMax, frMax);
  if (globalMax < 1)
    globalMax = 1.;
//  cout << globalMax << endl;
  for (i = 0; i < PTS_TOT; ++i) {
//    freq_arr[i] = min(freq_arr[i] / globalMax, 1.);
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
  if(i < N_FREQS)
    magVal = freq_arr[i];
  else
    magVal = freq_arr[i - N_FREQS];
  float rad = ((sin(loopCount/100.)+1.)/2.+magVal * (sin(PI+loopCount/100.))) / 2;
  y = 0xFFF/2+ (sin(2 * PI * (float)i
      / (float)(PTS_TOT - 1)) )* 0xFFF * rad;
  x = 0xFFF/2+ (cos(2 * PI * (float)i
      / (float)(PTS_TOT - 1)) )* 0xFFF * rad;
  hue = (int)((float)i / (float)(PTS_TOT - 1) * 360.
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

# define NUM_ENTS 4
# define TRAVEL_FRAMES 120
float LINE_STARTS[NUM_ENTS][2] {{0,0},{0,1},{1,0},{1,1}};
float LINE_ENDS[NUM_ENTS][2] {{1,1},{1,0},{0,1},{0,0}};
float LINE_COLORS[NUM_ENTS] {.2,.6,.8,1.};
float LINE_FILL[NUM_ENTS]  {1,1,1,1};
bool LINE_FILL_TOG[NUM_ENTS] {false,false,false,false};
bool LINE_REVERSE[NUM_ENTS] {false,false,false,false};
int LINE_PARA[NUM_ENTS] {0,0,0,0};
int CHG_LINE = 0;


void genLines(bool& genFlg, bool& chgColor, int i, int loopCount, double * freq_arr, int& lastIdx,
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
  int lineIter;

  // float rad = ((sin(loopCount/100.)+1.)/2.+magVal * (sin(PI+loopCount/100.))) / 2;
  // y = freq_arr[i] * 0xFFF;
  // x = i / (float)PTS_TOT * 0xFFF;

  int entIdx = i*NUM_ENTS / PTS_TOT;
  float locSpeed = TRAVEL_FRAMES / min(max((CUR_VOL), .2),2.);

  if (i*NUM_ENTS % PTS_TOT == 0){
    LINE_FILL_TOG[entIdx] = false;
  }

  if (loopCount - LINE_PARA[entIdx] > locSpeed){
    LINE_REVERSE[entIdx] = true;
    LINE_PARA[entIdx] = loopCount;
  }
  else if (loopCount - LINE_PARA[entIdx] == locSpeed && LINE_REVERSE[entIdx]){
    LINE_REVERSE[entIdx] = false;
    LINE_PARA[entIdx] = loopCount;
  }

  if (LINE_REVERSE[entIdx]){
    lineIter = locSpeed - (loopCount - LINE_PARA[entIdx]);
  }
  else {
    lineIter = loopCount - LINE_PARA[entIdx];
  }

  // float locSpeed = TRAVEL_FRAMES;
  fxs = LINE_STARTS[entIdx][0] * (1. - (float)lineIter / locSpeed) + LINE_ENDS[entIdx][0] 
      * ((float)lineIter  / locSpeed);
  fys = LINE_STARTS[entIdx][1] * (1. - (float)lineIter / locSpeed) + LINE_ENDS[entIdx][1] 
      * ((float)lineIter  / locSpeed);

  fxe = LINE_STARTS[(entIdx+1)%NUM_ENTS][0] * (1. - (float)lineIter / locSpeed) + LINE_ENDS[(entIdx+1)%NUM_ENTS][0] 
      * ((float)lineIter  / locSpeed);
  fye = LINE_STARTS[(entIdx+1)%NUM_ENTS][1] * (1. - (float)lineIter / locSpeed) + LINE_ENDS[(entIdx+1)%NUM_ENTS][1] 
      * ((float)lineIter  / locSpeed);

  fx = fxs * (1. - float(i % (PTS_TOT / NUM_ENTS)) / (PTS_TOT / NUM_ENTS)) + fxe * float(i % (PTS_TOT / NUM_ENTS)) / (PTS_TOT / NUM_ENTS);
  fy = fys * (1. - float(i % (PTS_TOT / NUM_ENTS)) / (PTS_TOT / NUM_ENTS)) + fye * float(i % (PTS_TOT / NUM_ENTS)) / (PTS_TOT / NUM_ENTS);


  if (genFlg && entIdx == CHG_LINE) {
    // LINE_STARTS[CHG_LINE][0] = fx;
    // LINE_STARTS[CHG_LINE][1] = fy;
    rxs = static_cast <float> (rand()) / static_cast <float> (RAND_MAX) / 3.;
    rys = static_cast <float> (rand()) / static_cast <float> (RAND_MAX) / 3.;
    rxe = static_cast <float> (rand()) / static_cast <float> (RAND_MAX) / 3.;
    rye = static_cast <float> (rand()) / static_cast <float> (RAND_MAX) / 3.;
    rxs = rxs + (rand() % 2) * (2./3.);
    rxe = rxe + (rand() % 2) * (2./3.);
    rys = rys + (rand() % 2) * (2./3.);
    rye = rye + (rand() % 2) * (2./3.);
    LINE_STARTS[CHG_LINE][0] = rxs;
    LINE_STARTS[CHG_LINE][1] = rys;
    LINE_ENDS[CHG_LINE][0] = rxe;
    LINE_ENDS[CHG_LINE][1] = rye;
    LINE_PARA[CHG_LINE] = loopCount + 1;
    CHG_LINE = (CHG_LINE + 1) % NUM_ENTS;
    genFlg = false;
  }
  if (chgColor && entIdx == CHG_LINE){
    LINE_COLORS[CHG_LINE] = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
    LINE_FILL[(CHG_LINE + NUM_ENTS - 1) % NUM_ENTS] = 1.;
    LINE_FILL[CHG_LINE] = max(static_cast <float> (rand()) / static_cast <float> (RAND_MAX), (float).15);
    CHG_LINE = (CHG_LINE + 1) % NUM_ENTS;
    chgColor = false;
  }
  x = fx * 0xFFF;
  y = fy * 0xFFF;
  // hue = (int)((float)i / (float)(PTS_TOT - 1) * 360.
  //     * 2. + loopCount * 2.) % 360;
  hue = LINE_COLORS[entIdx] * 360.;

  if (i % (int)((PTS_TOT / NUM_ENTS) * LINE_FILL[entIdx]) == 0){
    LINE_FILL_TOG[entIdx] = !LINE_FILL_TOG[entIdx];
  }


  if (LINE_FILL_TOG[entIdx]){
    HSVtoRGB(hue, 100., 100., r, g, b);

  }
  else{
    HSVtoRGB(hue, 100., 0., r, g, b);

  }
  
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
  double freq_arr[N_FREQS];
  double magAvg = 0.;
  double prevLocMax = 0.;
  double curMax = 0.;
  bool genFlg = false;
  bool bassFlg = false;
  bool trebFlg = false;
  double diffAvg;

  getFreqArr(freq_arr);

  diffAvg = 0;
  for (int i = 0; i < PTS_TOT * SAMP_RATIO; i++) {
    diffAvg += max(freq_arr[i] - LST_ARR[i], 0.);
    // diffAvg += freq_arr[i] - LST_ARR[i];
  }
  diffAvg /= (float)PTS_TOT * SAMP_RATIO;
  if (diffAvg > PCT_THRESH){
    bassFlg = true;
  }

  diffAvg = 0;
  for (int i = 1. - SAMP_RATIO; i < PTS_TOT; i++) {
    diffAvg += max(freq_arr[i] - LST_ARR[i], 0.);
    // diffAvg += freq_arr[i] - LST_ARR[i];
  }
  diffAvg /= (float)PTS_TOT * SAMP_RATIO;
  if (diffAvg > PCT_THRESH){
    trebFlg = true;
  }

  bassFlg = bassFlg && (abs(loopCount - COOLDOWN_IDX_BASS) > COOLDOWN);
  trebFlg = trebFlg && (abs(loopCount - COOLDOWN_IDX_TREB) > COOLDOWN);
  if (bassFlg){
    // INC_FLG = true;
    // genFlg = true;
    COOLDOWN_IDX_BASS = loopCount;
  }
  if (trebFlg){
    // INC_FLG = true;
    // genFlg = true;
    COOLDOWN_IDX_TREB = loopCount;
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

  for (int i = 0; i < NUM_FRAMES; i++) {
    loopCount = loopCount + i;
    locIter = 0;

    if (bassFlg) {cout << "bass: " << diffAvg << endl;}
    if (trebFlg) {cout << "treb: " << diffAvg << endl;}

    for (int j = 0; j < PTS_TOT; j++){

      
      // if (genFlg){
        // if (loopCount % 2) {
        //   locIter = PTS_TOT - j;
        // }
        // else{
        //   locIter = j;
        // }
      genLines(bassFlg, trebFlg, j, loopCount, freq_arr, lastIdx, x, y, r, g, b);

      // }
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
