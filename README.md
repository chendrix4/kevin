![](https://i.imgur.com/gU2tHtN.gif)

### Introduction<hr>
Kevin is an Arduino-powered, wall-mount audio visualizer. Complete with some vintage flair and sitting at nearly 2ft tall and 4ft wide, it is a killer addition to your next party.

Tutorial available on [Instructables](https://www.instructables.com/id/Retro-LED-Strip-Audio-Visualizer/).
Demo available on [Youtube](https://www.youtube.com/watch?v=PSi9F99n9rk).

### File Reference<hr>
```
├───Hardware
     audio.sch            Circuit schematic showing TRRS breakout, Audio input biasing, and 
                            Arduino connections
     eagle.epf            Eagle project file
     schematic.pdf        Schematic pdf in case you don't want to install Eagle
└───Software
    ├───kevin_fft
         kevin_fft.ino    Only for reference. This runs much slower than FHT
    ├───kevin_fht
         kevin_fht.ino    The actual file deployed on my Arduino
    └───simulator
         kevin.py         A python audio visualizer simulator. Play around with the math in
                            Kevin.update() to get different effects
         sin*.wav         Test files generated from audiocheck.net. When input to the sim,
                            you should see a peak only at the given frequency.
         pinknoise.wav    Not entirely sure of the difference b/w this and whitenoise, but I
         whitenoise.wav     was trying to use these files to equalize the display. The sim
                            should show a completely flat response for one (both?) of these
                            files, so I was trying to add a GAIN array that would handle that.
                            I don't have anything working yet.
         Songs/*          Not included here to avoid any copyright violations, but you can give
                            the sim any wav file. I used youtube-dl and ffmpeg to grab several.
```

### TODO<hr>
- **Sound bar**: Will house the power adapter, amplifier, and all other circuitry. When I make this, I will create a PCB.
- **Themes**: Matrix, Fireplace, "Sticky" Peaks, maybe even Pong?
- **Display Tuning**: Equalize the display with the white or pink noise files. Also, is alpha-smoothing needed?
