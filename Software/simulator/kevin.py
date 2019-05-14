# Algorithm derived from https://stackoverflow.com/questions/20408388/how-to-filter-fft-data-for-audio-visualisation
# Wave file decoding modified from https://stackoverflow.com/questions/2648151/python-frequency-detection
# Some other detail may be available at https://dsp.stackexchange.com/questions/27703/how-does-adobe-after-effects-generate-its-audio-spectrum-effect

import numpy as np
from pyqtgraph.Qt import QtGui, QtCore
import pyqtgraph as pg
import struct
import pyaudio

import sys
import wave
from itertools import chain

N = 1024        # FFT Length (Change to any power of 2)
ALPHA = 1       # Smoothing coefficient (1 = no smoothing. Try 1/2, 1/4, 1/8...)
BINNING = True

if len(sys.argv) < 2:
    print("Plays a wave file.\n\nUsage: %s filename.wav" % sys.argv[0])
    sys.exit(-1)

freqs = [33, 65, 131, 262, 523, 1047, 2093, 4186, 8372]
bins = [int(f*N/44100) for f in freqs] + [N//2]
bins = [0, 1, 2, 4, 6, 9, 12, 16, 20, 24, N//2]

if BINNING:
    SPECTRUM_WIDTH = len(bins) - 1
    s = 0.2
else:
    SPECTRUM_WIDTH = N//2
    s = 32/N

# for bargraph-ifying line graph data
spacing = 2/N 
bar_x = lambda x: [x+s, x+s, x+1-s, x+1-s]
bar_y = lambda y: [0, int(y), int(y), 0]

class Kevin(object):
    def __init__(self):

        # Audio stream setup
        self.p = pyaudio.PyAudio()

        self.wf = wave.open(sys.argv[1], 'rb')
        self.SAMPWIDTH = self.wf.getsampwidth()
        self.FORMAT = self.p.get_format_from_width(self.SAMPWIDTH)
        self.CHANNELS = self.wf.getnchannels()
        self.RATE = self.wf.getframerate()
        self.CHUNK = 2048
        self.WINDOW = np.kaiser(N, beta=14)

        self.stream = self.p.open(
            format=self.FORMAT,
            channels=self.CHANNELS,
            rate=self.RATE,
            output=True
        )
        
        self.data = self.wf.readframes(self.CHUNK)
        self.fft_smoothed = np.zeros(SPECTRUM_WIDTH)

        # PyQT app setup
        pg.setConfigOptions(antialias=True)
        self.app = QtGui.QApplication(sys.argv)
        self.win = pg.GraphicsWindow(title='Spectrum Analyzer')
        self.win.setWindowTitle('Spectrum Analyzer')
        display = self.app.desktop().screenGeometry()
        display = (0.1*display.width(), 0.1*display.height(),
                   0.8*display.width(), 0.8*display.height())
        self.win.setGeometry(*display)

        # PyQT graphs setup
        self.traces = {}

        wf_xlabels = [(0, '0'), (2048, '2048'), (4096, '4096')]
        wf_xaxis = pg.AxisItem(orientation='bottom')
        wf_xaxis.setTicks([wf_xlabels])

        self.waveform = self.win.addPlot(
            title='WAVEFORM', row=1, col=1, axisItems={'bottom': wf_xaxis},
        )
        self.traces[self.waveform.titleLabel.text] = self.waveform.plot(
            pen='c', width=3
        )
        self.waveform.setYRange(-32767, 32767, padding=0)
        self.waveform.setXRange(0, 2 * self.CHUNK, padding=0.005)

        sp_xaxis = pg.AxisItem(orientation='bottom')
        self.spectrum = self.win.addPlot(
            title='SPECTRUM', row=2, col=1, axisItems={'bottom': sp_xaxis},
        )
        self.traces[self.spectrum.titleLabel.text] = self.spectrum.plot(
            pen='m', width=3
        )
        self.spectrum.setYRange(0, 10, padding=0)
        self.spectrum.setXRange(0, SPECTRUM_WIDTH, padding=0.005)
    
    def start(self):
        if (sys.flags.interactive != 1) or not hasattr(QtCore, 'PYQT_VERSION'):
            QtGui.QApplication.instance().exec_()
    
    def stop(self):
        self.stream.write(self.data)
        self.stream.close()
        self.p.terminate()
        self.app.exit()
    
    def update(self):

        if len(self.data) == self.CHUNK*self.SAMPWIDTH*self.CHANNELS:

            # play audio
            self.stream.write(self.data)

            # unpack into readable values
            data_unpacked = np.array(wave.struct.unpack(
                f"{len(self.data)//self.SAMPWIDTH}h",
                self.data)
            )

            # display waveform data
            self.traces[self.waveform.titleLabel.text].setData(
                x=np.arange(0, 2*self.CHUNK, 2/self.CHANNELS),
                y=data_unpacked
            )

            # calculate FFT
            # apply window to sample to reduce spectral leakage / gibbs phenomenon
            sample = data_unpacked[::len(data_unpacked)//N]*self.WINDOW
            fft_raw = np.abs(np.fft.fft(sample, n=N)[:N//2])

            # take log to prevent any dwarfing
            fft_dB = 20 * np.log10(fft_raw)
            fft_dB[fft_dB == -np.inf] = 0
            if BINNING:
                fft_dB = [np.mean(fft_dB[bins[i]:bins[i+1]])
                           for i in range(SPECTRUM_WIDTH)]
                np.nan_to_num(fft_dB, False)

            # normalization
            scale = np.max(fft_dB) - np.min(fft_dB) + 0.00001
            fft_norm = 10 * np.max(data_unpacked) / 32767 * (fft_dB - np.min(fft_dB)) / scale

            # EMA
            self.fft_smoothed = ALPHA * fft_norm + (1 - ALPHA) * self.fft_smoothed
            
            # Scaling
            # After applying white and pink noise test signals, I saw a non-uniform response
            # To counter this, I just manually set the gain across the spectrum
            # I want something that looks visually correct, not necessarily what *is* correct
            M = np.mean(self.fft_smoothed)
            GAIN = [M/(M-np.log10(x)) for x in range(1,SPECTRUM_WIDTH+1)]
            #print(self.fft_smoothed)

            # Make it a bar graph
            x = [0] + list(chain.from_iterable(bar_x(i) for i in np.arange(SPECTRUM_WIDTH))) + [SPECTRUM_WIDTH]
            y = [0] + list(chain.from_iterable(bar_y(j) for j in self.fft_smoothed)) + [0]

            if not np.any(fft_raw == 0):
                self.traces[self.spectrum.titleLabel.text].setData(x=x, y=y)

            self.data = self.wf.readframes(self.CHUNK)
        
        else:
            self.stop()
    
    def animation(self):
        timer = QtCore.QTimer()
        timer.timeout.connect(self.update)
        timer.start(0)
        self.start()


if __name__ == '__main__':

    kevin = Kevin()
    kevin.animation()
