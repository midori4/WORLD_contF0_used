// Copyright 2012-2015 Masanori Morise. All Rights Reserved.
// Author: mmorise [at] yamanashi.ac.jp (Masanori Morise)
//
// Test program for WORLD 0.1.2 (2012/08/19)
// Test program for WORLD 0.1.3 (2013/07/26)
// Test program for WORLD 0.1.4 (2014/04/29)
// Test program for WORLD 0.1.4_3 (2015/03/07)
// Test program for WORLD 0.2.0 (2015/05/29)
// Test program for WORLD 0.2.0_1 (2015/05/31)
// Test program for WORLD 0.2.0_2 (2015/06/06)

// test.exe input.wav outout.wav f0 spec
// input.wav  : Input file
// output.wav : Output file
// f0         : F0 scaling (a positive number)
// spec       : Formant scaling (a positive number)

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if (defined (__WIN32__) || defined (_WIN32)) && !defined (__MINGW32__)
#include <conio.h>
#include <windows.h>
#pragma comment(lib, "winmm.lib")
#pragma warning(disable : 4996)
#endif
#if (defined (__linux__) || defined(__CYGWIN__) || defined(__APPLE__))
#include <stdint.h>
#include <sys/time.h>
#include <libgen.h> // for sprintf
#include <unistd.h> // for getopt
#include <getopt.h>
#endif

#include "./d4c.h"  // This is the new function.
#include "./dio.h"
#include "./matlabfunctions.h"
#include "./cheaptrick.h"
#include "./stonemask.h"
#include "./synthesis.h"  // This is the new function.

// Frame shift [msec]
#define FRAMEPERIOD 5.0
#define F0FLOOR 71.0

#if (defined (__linux__) || defined(__CYGWIN__) || defined(__APPLE__))
// Linux porting section: implement timeGetTime() by gettimeofday(),
#ifndef DWORD
#define DWORD uint32_t
#endif
DWORD timeGetTime() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  DWORD ret = static_cast<DWORD>(tv.tv_usec / 1000 + tv.tv_sec * 1000);
  return ret;
}
#endif

typedef struct 
{
  int f0_length;
  int fft_size;
  int fs;
  double frame_period;
  double f0_floor;
  double *f0;
  double *time_axis;
  double **spectrogram;
  double **aperiodicity;
} ST_WORLD;

typedef struct
{
  char *featureFile[4];
  char *inputFile;
  char *outputFile;
  int mode;
  double framePeriod;
  double f0Floor;
  bool castFloat;
} CMDARGS;

enum FEATURE {F0_ = 1, SP_ = 2, AP_ = 4};

namespace {
bool CheckLoadedFile(double *x, int fs, int nbit, int x_length) {
  if (x == NULL) {
    fprintf(stderr, "error: File not found.\n");
    return false;
  }
#if 0
  printf("File information\n");
  printf("Sampling : %d Hz %d Bit\n", fs, nbit);
  printf("Length %d [sample]\n", x_length);
  printf("Length %f [sec]\n", static_cast<double>(x_length) / fs);
#endif
  return true;
}

template<typename T_from, typename T_to, typename T_toType>
void memcastcpy(T_from to, T_to castTo, int n)
{
  for (int i = 0; i < n; ++i)
    castTo[i] = (T_toType)to[i];
}

void F0Estimation(double *x, int x_length, int fs, int f0_length, double *f0,
    double *time_axis, double FramePeriod, double F0Floor) {
  double *refined_f0 = new double[f0_length];

  DioOption option;
  InitializeDioOption(&option);  // Initialize the option
  // Modification of the option
  option.frame_period = FramePeriod;
  // Valuable option.speed represents the ratio for downsampling.
  // The signal is downsampled to fs / speed Hz.
  // If you want to obtain the accurate result, speed should be set to 1.
  option.speed = 1;
  // You should not set option.f0_floor to under world::kFloorF0.
  // If you want to analyze such low F0 speech, please change world::kFloorF0.
  // Processing speed may sacrify, provided that the FFT length changes.
  option.f0_floor = F0Floor;
  // You can give a positive real number as the threshold.
  // Most strict value is 0, but almost all results are counted as unvoiced.
  // The value from 0.02 to 0.2 would be reasonable.
  option.allowed_range = 0.1;

  fprintf(stderr, "\nAnalysis\n");
  DWORD elapsed_time = timeGetTime();
  Dio(x, x_length, fs, option, time_axis, f0);
  fprintf(stderr, "DIO: %d [msec]\n", timeGetTime() - elapsed_time);

  // StoneMask is carried out to improve the estimation performance.
  elapsed_time = timeGetTime();
  StoneMask(x, x_length, fs, time_axis, f0, f0_length, refined_f0);
  fprintf(stderr, "StoneMask: %d [msec]\n", timeGetTime() - elapsed_time);

  for (int i = 0; i < f0_length; ++i) f0[i] = refined_f0[i];

  delete[] refined_f0;
  return;
}

void SpectralEnvelopeEstimation(double *x, int x_length, int fs,
  double *time_axis, double *f0, int f0_length, double **spectrogram) {
  DWORD elapsed_time = timeGetTime();
  CheapTrick(x, x_length, fs, time_axis, f0, f0_length, spectrogram);
  fprintf(stderr, "CheapTrick: %d [msec]\n", timeGetTime() - elapsed_time);
}

void AperiodicityEstimation(double *x, int x_length, int fs, double *time_axis,
    double *f0, int f0_length, int fft_size, double **aperiodicity) {
  DWORD elapsed_time = timeGetTime();
  D4C(x, x_length, fs, time_axis, f0, f0_length, fft_size, aperiodicity);
  fprintf(stderr, "D4C: %d [msec]\n", timeGetTime() - elapsed_time);
}

#if 0
void ParameterModification(int argc, char *argv[], int fs, double *f0,
    int f0_length, double **spectrogram)
{
  int fft_size = GetFFTSizeForCheapTrick(fs);
  // F0 scaling
  if (argc >= 5) {
    double shift = atof(argv[4]);
    for (int i = 0; i < f0_length; ++i) f0[i] *= shift;
  }
  // Spectral stretching
  if (argc >= 6) {
    double ratio = atof(argv[5]);
    double *freq_axis1 = new double[fft_size];
    double *freq_axis2 = new double[fft_size];
    double *spectrum1 = new double[fft_size];
    double *spectrum2 = new double[fft_size];

    for (int i = 0; i <= fft_size / 2; ++i) {
      freq_axis1[i] = ratio * i / fft_size * fs;
      freq_axis2[i] = static_cast<double>(i) / fft_size * fs;
    }
    for (int i = 0; i < f0_length; ++i) {
      for (int j = 0; j <= fft_size / 2; ++j)
        spectrum1[j] = log(spectrogram[i][j]);
      interp1(freq_axis1, spectrum1, fft_size / 2 + 1, freq_axis2,
        fft_size / 2 + 1, spectrum2);
      for (int j = 0; j <= fft_size / 2; ++j)
        spectrogram[i][j] = exp(spectrum2[j]);
      if (ratio < 1.0) {
        for (int j = static_cast<int>(fft_size / 2.0 * ratio);
            j <= fft_size / 2; ++j)
          spectrogram[i][j] =
          spectrogram[i][static_cast<int>(fft_size / 2.0 * ratio) - 1];
      }
    }
    delete[] spectrum1;
    delete[] spectrum2;
    delete[] freq_axis1;
    delete[] freq_axis2;
  }
}
#endif

void WaveformSynthesis(double *f0, int f0_length, double **spectrogram,
    double **aperiodicity, int fft_size, double frame_period, int fs,
    int y_length, double *y) {
  DWORD elapsed_time;
  // Synthesis by the aperiodicity
  fprintf(stderr, "\nSynthesis\n");
  elapsed_time = timeGetTime();
  Synthesis(f0, f0_length, spectrogram, aperiodicity,
      fft_size, frame_period, fs, y_length, y);
  fprintf(stderr, "WORLD: %d [msec]\n", timeGetTime() - elapsed_time);
}


void InitializeWORLD(ST_WORLD *data, int f0_length, int fft_size, int fs, double frame_period)
{
  data->f0_length = f0_length;
  data->fft_size = fft_size;
  data->f0 = new double[f0_length];
  data->time_axis = new double[f0_length];
  data->spectrogram = new double *[f0_length];
  data->aperiodicity = new double *[f0_length];
  for (int i = 0; i < f0_length; ++i) {
    data->spectrogram[i] = new double[fft_size / 2 + 1];
    data->aperiodicity[i] = new double[fft_size / 2 + 1];
  }
  data->fs = fs;
  data->frame_period = frame_period;
}

void LoadFeature(ST_WORLD *data, CMDARGS *args)
{
  FILE *fr;
  char buf[1024];
  int i, f0_length, fft_size, fs;
  double frame_period;
  float *tmpf0;
  float *tmpspap;

  if ((fr = fopen(args->featureFile[0], "r")) == NULL) {
    fprintf(stderr, "ERROR! %s can't open\n", args->featureFile[0]);
    exit(1);
  }

  while (fgets(buf, 1024, fr) != NULL) {
    if (strncmp(buf, "F0LENGTH:", 9) == 0)
      sscanf(buf, "%*s %d", &f0_length);
    if (strncmp(buf, "FFTSIZE:", 8) == 0)
      sscanf(buf, "%*s %d", &fft_size);
    if (strncmp(buf, "FS:", 3) == 0)
      sscanf(buf, "%*s %d", &fs);
    if (strncmp(buf, "FRAMEPERIOD:", 12) == 0)
      sscanf(buf, "%*s %lf", &frame_period);
  }
  fclose(fr);

  InitializeWORLD(data, f0_length, fft_size, fs, frame_period);

  if (args->castFloat) {
    tmpf0 = new float[data->f0_length];
    tmpspap = new float[data->fft_size / 2 + 1];
  }

  // Read F0
  if ((fr = fopen(args->featureFile[1], "r")) == NULL) {
    fprintf(stderr, "ERROR! %s can't open\n", args->featureFile[1]);
    exit(1);
  }

  if (args->castFloat) {
    fread(tmpf0, sizeof(float), data->f0_length, fr);
    memcastcpy<float*,double*,double>(tmpf0, data->f0, data->f0_length);
    delete[] tmpf0;
    tmpf0 = 0;
  } else{
    fread(data->f0, sizeof(double), data->f0_length, fr);
  }
  fclose(fr);

  // Read SP
  if ((fr = fopen(args->featureFile[2], "r")) == NULL) {
    fprintf(stderr, "ERROR! %s can't open\n", args->featureFile[2]);
    exit(1);
  }

  if (args->castFloat) {
    for (int i = 0; i < data->f0_length; ++i) {
      fread(tmpspap, sizeof(float), data->fft_size / 2 + 1, fr);
      memcastcpy<float*,double*,double>(tmpspap, data->spectrogram[i], data->fft_size / 2 + 1);
    }
  } else {
    for (i = 0; i < data->f0_length; i++)
      fread(data->spectrogram[i], sizeof(double), data->fft_size / 2 + 1, fr);
  }
  fclose(fr);

  // Read AP
  if ((fr = fopen(args->featureFile[3], "r")) == NULL) {
    fprintf(stderr, "ERROR! %s can't open\n", args->featureFile[3]);
    exit(1);
  }

  if (args->castFloat) {
    for (int i = 0; i < data->f0_length; ++i) {
      fread(tmpspap, sizeof(float), data->fft_size/2 + 1, fr);
      memcastcpy<float*,double*,double>(tmpspap, data->aperiodicity[i], data->fft_size/2 + 1);
    }
    delete[] tmpspap;
    tmpspap = 0;
  } else {
    for (i = 0; i < data->f0_length; i++)
      fread(data->aperiodicity[i], sizeof(double), data->fft_size / 2 + 1, fr);
  }
  fclose(fr);
}

void OutputFeatures(ST_WORLD *data, CMDARGS *args, char *name, int mode)
{
  int i;
  char fileName[1024];
  FILE *fp;
  
  float *tmpf0;
  float *tmpspap;

  if (args->castFloat) {
    tmpf0 = new float[data->f0_length];
    tmpspap = new float[data->fft_size / 2 + 1];    
  }

  fp = fopen(args->featureFile[0], "wb");
  if (fp == NULL) {
    fprintf(stderr, "can't write conf file: %s\n", args->featureFile[0]);
    return;
  }
  fprintf(fp, "F0LENGTH:\t%d\n", data->f0_length);
  fprintf(fp, "FFTSIZE:\t%d\n", data->fft_size);
  fprintf(fp, "FS:\t%d\n", data->fs);
  fprintf(fp, "FRAMEPERIOD:\t%lf\n", data->frame_period);
  fclose(fp);

#ifdef USEPREFIX
  if (mode & F0_) {
    sprintf(fileName, "%s.f0", name);
    fp = fopen(fileName, "wb");
    if (fp == NULL) {
      fprintf(stderr, "can't write f0 file: %s\n", fileName);
      return;
    }
    fwrite(data->f0, sizeof(double), data->f0_length, fp);
    fclose(fp);
  }

  if (mode & SP_) {
    sprintf(fileName, "%s.sp", name);
    fp = fopen(fileName, "wb");
    if (fp == NULL) {
      fprintf(stderr, "can't write sp file: %s\n", fileName);
      return;
    }
    for (i = 0; i < data->f0_length; i++)
      fwrite(data->spectrogram[i], sizeof(double), data->fft_size / 2 + 1, fp);
    fclose(fp);
  }

  if (mode & AP_) {
    sprintf(fileName, "%s.ap", name);
    fp = fopen(fileName, "wb");
    if (fp == NULL) {
      fprintf(stderr, "can't write ap file: %s\n", fileName);
      return;
    }
    for (i = 0; i < data->f0_length; i++)
      fwrite(data->aperiodicity[i], sizeof(double), data->fft_size / 2 + 1, fp);
    fclose(fp);
  }
#endif
  if (mode & F0_) {
    fp = fopen(args->featureFile[1], "wb");
    if (fp == NULL) {
      fprintf(stderr, "can't write f0 file: %s\n", args->featureFile[1]);
      return;
    }
    if (args->castFloat) {
      memcastcpy<double*,float*,float>(data->f0, tmpf0, data->f0_length);
      fwrite(tmpf0, sizeof(float), data->f0_length, fp);
    }
    else
      fwrite(data->f0, sizeof(double), data->f0_length, fp);
    fclose(fp);
  }

  if (mode & SP_) {
    fp = fopen(args->featureFile[2], "wb");
    if (fp == NULL) {
      fprintf(stderr, "can't write sp file: %s\n", args->featureFile[2]);
      return;
    }
    for (i = 0; i < data->f0_length; i++) {
      if (args->castFloat) {
        memcastcpy<double*,float*,float>(data->spectrogram[i], tmpspap, data->fft_size/2 + 1);
        fwrite(tmpspap, sizeof(float), data->fft_size / 2 + 1, fp);
      }else {
        fwrite(data->spectrogram[i], sizeof(double), data->fft_size / 2 + 1, fp);
      }
    }
    fclose(fp);
  }

  if (mode & AP_) {
    fp = fopen(args->featureFile[3], "wb");
    if (fp == NULL) {
      fprintf(stderr, "can't write ap file: %s\n", args->featureFile[3]);
      return;
    }
    for (i = 0; i < data->f0_length; i++) {
      if (args->castFloat) {
        memcastcpy<double*,float*,float>(data->aperiodicity[i],tmpspap,data->fft_size/2 + 1);
        fwrite(tmpspap, sizeof(float), data->fft_size / 2 + 1, fp);
      } else {
        fwrite(data->aperiodicity[i], sizeof(double), data->fft_size / 2 + 1, fp);
      }
    }
    fclose(fp);
  }
}

void DestroyWORLD(ST_WORLD *data)
{
  delete[] data->time_axis;
  delete[] data->f0;
  for (int i = 0; i < data->f0_length; ++i) {
    delete[] data->spectrogram[i];
    delete[] data->aperiodicity[i];
  }
  delete[] data->spectrogram;
  delete[] data->aperiodicity;
}

#if 0
void Usage(char *app)
{
  fprintf(stderr, "Usage:　%s Input.wav Mode [FramePeriod] [F0scaling] [FormantShift]\n", app);
  fprintf(stderr, "  STDOUT: file information\n");
  fprintf(stderr, "  PARAMETER:\n");
  fprintf(stderr, "    Input.wav: monolal wav\n");
  fprintf(stderr, "    Mode: 0 -> Output synthesis wav (Syn_Input.wav) default\n");
  fprintf(stderr, "          1 -> Output F0 (Syn_Input.f0)\n");
  fprintf(stderr, "          2 -> Output SpectralEnvelope (Syn_Input.sp)\n");
  fprintf(stderr, "          4 -> Output Aperiodicity (Syn_Input.ap)\n");
  fprintf(stderr, "          7 -> Output All\n");
  fprintf(stderr, "  OPTION:\n");
  fprintf(stderr, "    FramePeriod: positive number (default 5.0)\n");
  fprintf(stderr, "    F0scaling: positive number\n");
  fprintf(stderr, "    FormantShift: positive number\n");
  exit(1);
}
#endif
int isVaildFileName(CMDARGS *data)
{
  int isVaild = 1;
  if (data->mode & F0_
   && data->featureFile[1] == NULL) {
    fprintf(stderr, "Set output f0 file name\n\tUse opt --outf0 FILENAME\n");
    isVaild = 0;
  }
  if (data->mode & SP_
   && data->featureFile[2] == NULL) {
    fprintf(stderr, "Set output sp file name\n\tUse opt --outsp FILENAME\n");
    isVaild = 0;
  }
  if (data->mode & AP_
   && data->featureFile[3] == NULL) {
    fprintf(stderr, "Set output ap file name\n\tUse opt --outap FILENAME\n");
    isVaild = 0;
  }
  if (data->featureFile[0] == NULL) {
    fprintf(stderr, "Set output conf file name\n\tUse opt --outconf FILENAME\n");
    isVaild = 0; 
  }

  return isVaild;
}

void longCommandLineParse(int argc, char **argv, CMDARGS *data)
{
  int opt, option_index, long_opt;
  char *optStr = "i:o:m:v";
  char tmpName[1024], outFile[1024];
  int i;
  struct option long_options[] =
  {
    {"outf0", required_argument, &long_opt, 'F'},
    {"outsp", required_argument, &long_opt, 'S'},
    {"outap", required_argument, &long_opt, 'A'},
    {"outconf", required_argument, &long_opt, 'C'},
    //{"inf0", required_argument, &long_opt, 'f'},
    //{"insp", required_argument, &long_opt, 's'},
    //{"inap", required_argument, &long_opt, 'a'},
    //{"inconf", required_argument, &long_opt, 'c'},
    //{"f0scale", required_argument, &long_opt, 'm'},
    //{"spec", required_argument, &long_opt, 'M'},
    {"f0floor", required_argument, &long_opt, 'b'},
    {"frameshift", required_argument, &long_opt, 'p'},
    {"castfloat", no_argument, NULL, 1},
    {0, 0, 0, 0}
  };

  data->framePeriod = FRAMEPERIOD;
  data->mode = 0;
  data->inputFile = NULL;
  data->outputFile = NULL;
  data->f0Floor = F0FLOOR;
  data->castFloat = false;
  for (i = 0; i < 4; i++)
    data->featureFile[i] = NULL;

  while ((opt = getopt_long(argc, argv, optStr, long_options, &option_index)) != -1) {
    switch(opt) {
      case 0:
        switch (long_opt) {
          case 'F':
            data->featureFile[1] = optarg;
            break;
          case 'S':
            data->featureFile[2] = optarg;
            break;
          case 'A':
            data->featureFile[3] = optarg;
            break;
          case 'C':
            data->featureFile[0] = optarg;
            break;
          case 'b':
            data->f0Floor = atof(optarg);
            break;
          case 'p':
            data->framePeriod = atof(optarg);
            break;
          case '?':
            fprintf(stderr, "unknown option\n");
            exit(1);
        }
        break;
      case 1:
        data->castFloat = true;
        break;
      case 'i':
        data->inputFile = optarg;
        break;
      case 'o':
        data->outputFile = optarg;
        break;
      case 'm':
        data->mode = atoi(optarg);
        break;
      case 'v':
        fprintf(stderr, "Demo program for WORLD 0.2.0_2\n");
        exit(0);
      case '?':
        fprintf(stderr, "unknown option\n");
        exit(1);
    }
  }
  
  if (data->mode == 0) {
    for (i = 0; optind < argc && i < 4; optind++, i++) {
      data->featureFile[i] = argv[optind];
    }
    if (i != 4) {
      fprintf(stderr, "feature data error, reference README\n");
      exit(1);
    }
    if (data->outputFile == NULL) {
      fprintf(stderr, "Need output file name\n");
      exit(1);
    }
  } else if (data->mode > -1 && data->mode < 8) {
    if (data->inputFile == NULL) {
      fprintf(stderr, "Can't find input, reference README\n");
      exit(1);
    }
    if (!isVaildFileName(data))
      exit(1);
    #if 0
    if (data->outputFile == NULL) {
      // use for prefix????
      sprintf(tmpName, "Syn_%s", basename(data->inputFile));
      sprintf(outFile, "%s/%s", dirname(data->inputFile), tmpName);
      data->outputFile = strdup(outFile);

      exit(1);
    }
    #endif
  } else {
    fprintf(stderr, "Mode out of range, reference README\n");
    exit(1);
  }
}

}  // namespace

//-----------------------------------------------------------------------------
// Test program.
// test.exe input.wav mode f0 spec
// input.wav  : argv[1] Input file
// Mode       : argv[2] Output file
// FrameShift : argv[3] FramePeriod (a positive number)
// f0         : argv[3] F0 scaling (a positive number)
// spec       : argv[4] Formant shift (a positive number)
//-----------------------------------------------------------------------------
int main(int argc, char *argv[]) 
{
  int fs, nbit, x_length;
  ST_WORLD data;
  CMDARGS args;

  longCommandLineParse(argc, argv, &args);

  if (args.mode) {
    double *x = wavread(args.inputFile, &fs, &nbit, &x_length);

    if (CheckLoadedFile(x, fs, nbit, x_length) == false)
      return 1;

    // The number of samples for F0
    int f0_length = GetSamplesForDIO(fs, x_length, args.framePeriod);

    // FFT size for CheapTrick
    int fft_size = GetFFTSizeForCheapTrick(fs);
    
    // Allocate memories
    InitializeWORLD(&data, f0_length, fft_size, fs, args.framePeriod);

    // F0 estimation
    F0Estimation(x, x_length, fs, f0_length, data.f0, data.time_axis, args.framePeriod, args.f0Floor);

    // Spectral envelope estimation
    //if (genOutput || mode & SP_)
    if (args.mode & SP_)
      SpectralEnvelopeEstimation(x, x_length, fs, data.time_axis, data.f0, f0_length, 
        data.spectrogram);

    // Aperiodicity estimation by D4C
    //if (genOutput || mode & AP_)
    if (args.mode & AP_)
      AperiodicityEstimation(x, x_length, fs, data.time_axis, data.f0, f0_length,
        fft_size, data.aperiodicity);

    // Note that F0 must not be changed until all parameters are estimated.
    //ParameterModification(argc, argv, fs, data.f0, f0_length, data.spectrogram);

    OutputFeatures(&data, &args, args.outputFile, args.mode);
    fprintf(stderr, "Analyze Complete\n");
    delete[] x;
  } else {
    LoadFeature(&data, &args);
    // The length of the output waveform
    int y_length =
      static_cast<int>((data.f0_length - 1) * args.framePeriod / 1000.0 * data.fs) + 1;
    double *y = new double[y_length];
    // Synthesis
    WaveformSynthesis(data.f0, data.f0_length, data.spectrogram, data.aperiodicity, data.fft_size, 
      args.framePeriod, data.fs, y_length, y);
    // Output
    wavwrite(y, y_length, data.fs, 16, args.outputFile);
    delete[] y;
  }
  DestroyWORLD(&data);
  
  return 0;
}
