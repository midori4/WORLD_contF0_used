//-----------------------------------------------------------------------------
// Copyright 2012-2015 Masanori Morise. All Rights Reserved.
// Author: mmorise [at] yamanashi.ac.jp (Masanori Morise)
//
// Voice synthesis based on f0, spectrogram and aperiodicity.
// forward_real_fft, inverse_real_fft and minimum_phase are used to speed up.
//-----------------------------------------------------------------------------
#include "./synthesis.h"

#include <math.h>

#include "./common.h"
#include "./constantnumbers.h"
#include "./matlabfunctions.h"

namespace {

void GetNoiseSpectrum(int noise_size, int fft_size,
    ForwardRealFFT *forward_real_fft) {
  double average = 0.0;
  for (int i = 0; i < noise_size; ++i) {
    forward_real_fft->waveform[i] = randn();
    average += forward_real_fft->waveform[i];
  }

  average /= static_cast<double>(noise_size);
  for (int i = 0; i < noise_size; ++i)
    forward_real_fft->waveform[i] -= average;
  for (int i = noise_size; i < fft_size; ++i)
    forward_real_fft->waveform[i] = 0.0;
  fft_execute(forward_real_fft->forward_fft);
}

//-----------------------------------------------------------------------------
// GetAperiodicResponse() calculates an aperiodic response.
//-----------------------------------------------------------------------------
void GetAperiodicResponse(int noise_size, int fft_size,
    double *spectrum, double *aperiodic_ratio,
    ForwardRealFFT *forward_real_fft,
    InverseRealFFT *inverse_real_fft, MinimumPhaseAnalysis *minimum_phase,
    double *aperiodic_response) {
  GetNoiseSpectrum(noise_size, fft_size, forward_real_fft);

  for (int i = 0; i <= minimum_phase->fft_size / 2; ++i)
    minimum_phase->log_spectrum[i] = log(spectrum[i] * aperiodic_ratio[i] + world::kMySafeGuardMinimum) / 2.0;
  GetMinimumPhaseSpectrum(minimum_phase);

  for (int i = 0; i <= fft_size / 2; ++i) {
    inverse_real_fft->spectrum[i][0] =
      minimum_phase->minimum_phase_spectrum[i][0];
    inverse_real_fft->spectrum[i][1] =
      minimum_phase->minimum_phase_spectrum[i][1];

    inverse_real_fft->spectrum[i][0] =
      minimum_phase->minimum_phase_spectrum[i][0] *
      forward_real_fft->spectrum[i][0] -
      minimum_phase->minimum_phase_spectrum[i][1] *
      forward_real_fft->spectrum[i][1];
    inverse_real_fft->spectrum[i][1] =
      minimum_phase->minimum_phase_spectrum[i][0] *
      forward_real_fft->spectrum[i][1] +
      minimum_phase->minimum_phase_spectrum[i][1] *
      forward_real_fft->spectrum[i][0];
  }
  fft_execute(inverse_real_fft->inverse_fft);
  fftshift(inverse_real_fft->waveform, fft_size, aperiodic_response);
}

//-----------------------------------------------------------------------------
// GetPeriodicResponse() calculates an aperiodic response.
//-----------------------------------------------------------------------------
void GetPeriodicResponse(int fft_size, double *spectrum,
    double *aperiodic_ratio,
    InverseRealFFT *inverse_real_fft, MinimumPhaseAnalysis *minimum_phase,
    double *periodic_response) {
  if (aperiodic_ratio[0] == 1.) {
    for (int i = 0; i < fft_size; ++i) periodic_response[i] = 0.0;
    return;
  }

  for (int i = 0; i <= minimum_phase->fft_size / 2; ++i)
    minimum_phase->log_spectrum[i] =
      log(spectrum[i] * (1.0 - aperiodic_ratio[i]) +
      world::kMySafeGuardMinimum) / 2.0;
  GetMinimumPhaseSpectrum(minimum_phase);
  
  for (int i = 0; i <= fft_size / 2; ++i) {
    inverse_real_fft->spectrum[i][0] =
      minimum_phase->minimum_phase_spectrum[i][0];
    inverse_real_fft->spectrum[i][1] =
      minimum_phase->minimum_phase_spectrum[i][1];
  }
  fft_execute(inverse_real_fft->inverse_fft);
  fftshift(inverse_real_fft->waveform, fft_size, periodic_response);
}

void GetSpectralEnvelope(double current_time, double frame_period,
    int f0_length, double **spectrogram, int fft_size,
    double *spectral_envelope) {
  int current_frame_floor =
    MyMin(f0_length - 1, static_cast<int>(floor(current_time / frame_period)));
  int current_frame_ceil =
    MyMin(f0_length - 1, static_cast<int>(ceil(current_time / frame_period)));
  double interpolation = current_time / frame_period - current_frame_floor;

  if (current_frame_floor == current_frame_ceil) {
    for (int i = 0; i <= fft_size / 2; ++i)
      spectral_envelope[i] = spectrogram[current_frame_floor][i];
  } else {
    for (int i = 0; i <= fft_size / 2; ++i)
      spectral_envelope[i] =
        (1.0 - interpolation) * spectrogram[current_frame_floor][i] +
        interpolation * spectrogram[current_frame_ceil][i];
  }
}

void GetAperiodicRatio(double current_time, double frame_period,
    int f0_length, double **aperiodicity, int fft_size,
    double *aperiodic_spectrum) {
  int current_frame_floor =
    MyMin(f0_length - 1, static_cast<int>(floor(current_time / frame_period)));
  int current_frame_ceil =
    MyMin(f0_length - 1, static_cast<int>(ceil(current_time / frame_period)));
  double interpolation = current_time / frame_period - current_frame_floor;
  
  if (current_frame_floor == current_frame_ceil) {
    for (int i = 0; i <= fft_size / 2; ++i)
      aperiodic_spectrum[i] =
        pow(aperiodicity[current_frame_floor][i], 2.0);
  } else {
    for (int i = 0; i <= fft_size / 2; ++i)
      aperiodic_spectrum[i] =
        pow((1.0 - interpolation) * aperiodicity[current_frame_floor][i] +
        interpolation * aperiodicity[current_frame_ceil][i], 2.0);
  }
}

//-----------------------------------------------------------------------------
// GetOneFrameSegment() calculates a periodic and aperiodic response at a time.
//-----------------------------------------------------------------------------
void GetOneFrameSegment(int noise_size,
    double **spectrogram, int fft_size, double **aperiodicity, int f0_length,
    double frame_period, double current_time, int fs,
    ForwardRealFFT *forward_real_fft, InverseRealFFT *inverse_real_fft,
    MinimumPhaseAnalysis *minimum_phase, double *response) {
  double *aperiodic_response = new double[fft_size];
  double *periodic_response = new double[fft_size];

  double *spectral_envelope = new double[fft_size];
  double *aperiodic_ratio = new double[fft_size];
  GetSpectralEnvelope(current_time, frame_period, f0_length, spectrogram,
      fft_size, spectral_envelope);
  GetAperiodicRatio(current_time, frame_period, f0_length, aperiodicity,
      fft_size, aperiodic_ratio);

  // Synthesis of the periodic response
  GetPeriodicResponse(fft_size, spectral_envelope, aperiodic_ratio,
    inverse_real_fft, minimum_phase, periodic_response);

  // Synthesis of the aperiodic response
  GetAperiodicResponse(noise_size, fft_size, spectral_envelope,
      aperiodic_ratio, forward_real_fft,
      inverse_real_fft, minimum_phase, aperiodic_response);

  double sqrt_noise_size = sqrt(static_cast<double>(noise_size));
  for (int i = 0; i < fft_size; ++i)
   response[i] =
      (periodic_response[i] * sqrt_noise_size + aperiodic_response[i]) /
      fft_size;
  
  delete[] spectral_envelope;
  delete[] aperiodic_ratio;
  delete[] periodic_response;
  delete[] aperiodic_response;
}

void GetTemporalParametersForTimeBase(double *cont_f0, int f0_length, int fs,
    int y_length, double frame_period, double *time_axis,
    double *coarse_time_axis, double *coarse_cont_f0) {
  for (int i = 0; i < y_length; ++i)
    time_axis[i] = i / static_cast<double>(fs);
  for (int i = 0; i < f0_length + 1; ++i)
    coarse_time_axis[i] = i * frame_period;
  for (int i = 0; i < f0_length + 1; ++i)
    coarse_cont_f0[i] = cont_f0[i];
  coarse_cont_f0[f0_length] = coarse_cont_f0[f0_length - 1] * 2 -
    coarse_cont_f0[f0_length - 2];
}


int GetPulseLocationsForTimeBase(double *interpolated_f0, double *time_axis,
    int y_length, int fs, double *pulse_locations,
    int *pulse_locations_index) {

  double *total_phase = new double[y_length];
  total_phase[0] = 2.0 * world::kPi * interpolated_f0[0] / fs;
  for (int i = 1; i < y_length; ++i)
    total_phase[i] = total_phase[i - 1] +
      2.0 * world::kPi * interpolated_f0[i] / fs;

  double *wrap_phase = new double[y_length];
  for (int i = 0; i < y_length; ++i)
    wrap_phase[i] = fmod(total_phase[i], 2.0 * world::kPi);

  double *wrap_phase_abs = new double[y_length];
  for (int i = 0; i < y_length - 1; ++i)
    wrap_phase_abs[i] = fabs(wrap_phase[i + 1] - wrap_phase[i]);

  int number_of_pulses = 0;
  for (int i = 0; i < y_length - 1; ++i) {
    if (wrap_phase_abs[i] > world::kPi) {
      pulse_locations[number_of_pulses] = time_axis[i];
      pulse_locations_index[number_of_pulses] = static_cast<int>
        (matlab_round(pulse_locations[number_of_pulses] * fs));
      ++number_of_pulses;
    }
  }

  delete[] wrap_phase_abs;
  delete[] wrap_phase;
  delete[] total_phase;

  return number_of_pulses;
}

int GetTimeBase(double *cont_f0, int f0_length, int fs,
    double frame_period, int y_length, double *pulse_locations,
    int *pulse_locations_index) {
  double *time_axis = new double[y_length];
  double *coarse_time_axis = new double[f0_length + 1];
  double *coarse_cont_f0 = new double[f0_length + 1];
  GetTemporalParametersForTimeBase(cont_f0, f0_length, fs, y_length, frame_period,
      time_axis, coarse_time_axis, coarse_cont_f0);
   
  double *interpolated_cont_f0 = new double[y_length];
  interp1(coarse_time_axis, coarse_cont_f0, f0_length + 1,
      time_axis, y_length, interpolated_cont_f0);
  
  int number_of_pulses = GetPulseLocationsForTimeBase(interpolated_cont_f0,
      time_axis, y_length, fs, pulse_locations, pulse_locations_index);
  
  delete[] coarse_cont_f0;
  delete[] coarse_time_axis;
  delete[] time_axis;
  delete[] interpolated_cont_f0;
  
  return number_of_pulses;
}

}  // namespace

void Synthesis(double *cont_f0, int f0_length, double **spectrogram,
    double **aperiodicity, int fft_size, double frame_period, int fs,
    int y_length, double *y) {
  double *impulse_response = new double[fft_size];
  for (int i = 0; i < y_length; ++i) y[i] = 0.0;

  MinimumPhaseAnalysis minimum_phase = {0};
  InitializeMinimumPhaseAnalysis(fft_size, &minimum_phase);
  InverseRealFFT inverse_real_fft = {0};
  InitializeInverseRealFFT(fft_size, &inverse_real_fft);
  ForwardRealFFT forward_real_fft = {0};
  InitializeForwardRealFFT(fft_size, &forward_real_fft);

  double *pulse_locations = new double[y_length];
  int *pulse_locations_index = new int[y_length];
  int number_of_pulses = GetTimeBase(cont_f0, f0_length, fs, frame_period / 1000.0,
      y_length, pulse_locations, pulse_locations_index);

  frame_period /= 1000.0;
  int noise_size;

  for (int i = 0; i < number_of_pulses; ++i) {
    noise_size = pulse_locations_index[MyMin(number_of_pulses - 1, i + 1)] -
      pulse_locations_index[i];

    GetOneFrameSegment(noise_size,
        spectrogram, fft_size, aperiodicity, f0_length, frame_period,
        pulse_locations[i], fs, &forward_real_fft, &inverse_real_fft,
        &minimum_phase, impulse_response);

    int index = 0;
    for (int j = 0; j < fft_size; ++j) {
      index = MyMin(y_length - 1,
        MyMax(0, j + pulse_locations_index[i] - fft_size / 2 + 1));
        y[index] += impulse_response[j];
    }
  }

  delete[] pulse_locations;
  delete[] pulse_locations_index;

  DestroyMinimumPhaseAnalysis(&minimum_phase);
  DestroyInverseRealFFT(&inverse_real_fft);
  DestroyForwardRealFFT(&forward_real_fft);

  delete[] impulse_response;
}
