''' F0 contour (or log F0) , spectral envelope (or mel-cepstrum) and aperiodicity (or band aperiodicity) estimation

usage: analysis.py [-h|--help] [-L <l_limit>] [-U <u_limit>] [-t <ap_threshold>] [-m <order>] [-s <sp_file>] [-a <ap_file>] [-f <f0_file>] [-c <outconf>] (-i <input_wav>) [-w|--world]

options:
    -h, --help              Show this message and exit.
    -L, <l_limit>           Lower F0 limit in Hz (optional) [default: 71.0].
    -U, <u_limit>           Upper F0 limit in Hz (optional) [default: 800.0].
    -m, <order>             Order of mel-cepstrum. It is ignored if \'-world\' option specified (optional) [default: 24].
    -t, <ap_threshold>      Threshold for aperiodicity-based voiced/unvoiced decision, in range 0 to 1 [default: 0.85].
    -s, <sp_file>           File name in which you want to save vocal tract filter. You can select mcep or spec by \'-world\' option (optional).'
    -a, <ap_file>           File name which in which you want to save aperiodicity. You can select aperiodicity or band aperiodicity by \'-world\' option (optional).
    -f, <f0_file>           File name in which you want to save F0. You can select F0 or log F0 by \'-world\' option (optional).
    -c, <outconf>           Out speech parameter extract configure file name (optional).
    -i, <input_wav>         File name of input wav file (required).
    -w, --world             If specified, F0, spectral envelope and aperiodicity are extracted. If not specified, Log F0, mel-cepstrum, and band aperiodicity are extracted.
'''
from docopt import docopt

import numpy as np

import sys
import os

from scipy.io import wavfile
import pyworld as pw
from pysptk import *
import pyreaper

from config import Config


def analysis(file_name, l_limit, u_limit, ap_threshold, conf, outconf):
    fs, wav_int16 = wavfile.read(file_name)
    assert fs == conf.sample_rate
    wav_float64 = wav_int16.astype(np.float64)
    
    frame_period = conf.frame_period
    # F0 estimating
    if conf.f0_estimating_method == "dio":
        '''
        DIO F0 extraction algorithm.
        Waveform signal, Sampling frequency [Hz] -> Estimated F0 contour, Temporal position of each frame
        '''
        _f0, time = pw.dio(wav_float64, fs, f0_floor=l_limit, f0_ceil=u_limit, frame_period=frame_period)
        '''
        StoneMask F0 refinement algorithm
        Waveform signal, F0 contour, Temporal positions of each frame, Sampling frequency [Hz] -> Refined F0 contour
        '''
        f0 = pw.stonemask(wav_float64, _f0, time, fs)
    elif conf.f0_estimating_method == "harvest":
        f0, time = pw.harvest(wav_float64, fs, f0_floor=l_limit, f0_ceil=u_limit, frame_period=frame_period)
    elif conf.f0_estimating_method == "reaper":
        # Just the purpose of getting time (want to know the length full frames)
        _f0, time = pw.dio(wav_float64, fs, f0_floor=l_limit, f0_ceil=u_limit, frame_period=frame_period)
        # F0 estimating by REAPER
        pm_times, pm, f0_reaper_times, f0, corr = pyreaper.reaper(wav_int16, fs, minf0=l_limit, maxf0=u_limit, frame_period=frame_period*1e-3)
        voiced = ~(f0 == -1.)
        f0[~voiced] = 0. # unvoiced region's f0 = 0. [Hz]
        pad = len(time) - len(f0_reaper_times)
        f0 = np.pad(f0, [0, pad], 'constant')
        f0 = f0.astype(np.float64)
    else:
        sys.stderr.write("\nERROR!: F0 estimating method must be either  'dio', 'harvest' or 'reaper'.\n\n")
        sys.exit(1)
    
    '''
    CheapTrick harmonic spectral envelope estimation algorithm
    Waveform signal, F0 contour, Temporal position of each frame, Sampling frequency [Hz], Lower F0 limit in Hz (FFT size depends on the value)
      -> Spectral envelope (squared magnitude)
    '''
    sp = pw.cheaptrick(wav_float64, f0, time, fs, f0_floor=l_limit)
    
    '''
    FFT size depends on the lower F0 limit
    Sampling frequency [Hz], Lower F0 limit [Hz] -> Resulting FFT size
    '''
    FFTSIZE = pw.get_cheaptrick_fft_size(fs, l_limit)
    '''
    D4C aperiodicity estimation algorithm
    Waveform signal, F0 contour, Temporal position of each frame, Sampling frequency [Hz], FFT size to be used 
      -> Aperiodicity (envelope, linear magnitude relative to spectral envelope)
    '''
    ap = pw.d4c(wav_float64, f0, time, fs, threshold=ap_threshold, fft_size=FFTSIZE)
    
    with open(outconf, 'w') as f:
        f.write('F0LENGTH:\t'+str(len(f0))+'\nFFTSIZE:\t'+str(FFTSIZE)+'\nFS:\t'+str(fs)+'\nFRAMEPERIOD:\t'+str(frame_period)+'\n')
    return fs, f0, sp, ap

def make_lf0(f0):
    f0[f0==0.] = 1.
    lf0 = np.log(f0)
    lf0[lf0==0.] = -1e+10
    return lf0.astype(np.float32)

def make_mcep(sp, order, fs):
    if fs == 16000:
        freq_warp = 0.42
    elif fs == 22050:
        freq_warp = 0.45
    elif fs == 44100:
        freq_warp = 0.53
    elif fs == 48000:
        freq_warp = 0.55
    else:
        sys.stderr.write('\nERROR!: sampling frequency is none of 16k, 24k, 32k and 48k [Hz].\n\n')
        sys.exit(1)
    
    mcep = sp2mc(sp, order, freq_warp)
    return mcep.astype(np.float32)

def make_bap(ap):
    bandwidth1 = (ap.shape[1] - 1) // 2 ** 3
    bandwidth2 = (ap.shape[1] - 1) // 2 ** 2
    bap = []
    for i in range(ap.shape[0]):
        # 5 band
        tmp = [ap[i][0:bandwidth1].mean(), ap[i][bandwidth1:bandwidth2].mean(),\
                ap[i][bandwidth2:2*bandwidth2].mean(), ap[i][2*bandwidth2:3*bandwidth2].mean(), ap[i][3*bandwidth2:].mean()]
        bap.append(tmp)
    bap = np.array(bap)
    return bap.astype(np.float32)

if __name__ == '__main__':
    args = docopt(__doc__)
    print("Command line args:\n", args)
    l_limit = float(args['-L'])
    u_limit = float(args['-U'])
    order = int(args['-m'])
    ap_threshold = float(args['-t'])
    sp_file = args['-s']
    ap_file = args['-a']
    f0_file = args['-f']
    outconf = args['-c']
    input_wav = args['-i']
    is_world = args['--world']
    
    conf = Config()
    print("sampling frequency: {}Hz, frame period: {}ms, F0 extraction method \"{}\"".format(conf.sample_rate, conf.frame_period, conf.f0_estimating_method))
    fs, f0, sp, ap = analysis(input_wav, l_limit, u_limit, ap_threshold, conf, outconf)
    
    if is_world:
        if f0_file is not None:
            f0.astype(np.float32).tofile(f0_file) # f0
        if sp_file is not None:
            sp.astype(np.float32).tofile(sp_file) # spectrum envelope
        if ap_file is not None:
            ap.astype(np.float32).tofile(ap_file) # aperiodicity
    else:
        if f0_file is not None:
            lf0 = make_lf0(f0)
            lf0.tofile(f0_file) # log f0
        if sp_file is not None:
            mcep = make_mcep(sp, order, fs)
            mcep.tofile(sp_file) # mel-cepstrum
        if ap_file is not None:
            bap = make_bap(ap)
            bap.tofile(ap_file) # band aperiodicity
    sys.exit(0)
