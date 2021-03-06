#### Codec2 LPC-10 1300 bps Speech Vocoder

The purpose of this version of the vocoder, is to break out the encoder and decoder parts into separate directories, and to delete the other modes.

The files were grouped using Netbeans, and compiled in Release mode.

If you burst the ZIP file it will create all the directories, and you can just type ```make``` and it will build the library. I used -O2 for optimization. This can be used, for example on Raspberry Pi devices. It's main use is for import into Netbeans.

#### Design Change
I'm an old geezer, so I like to see my data in the BSS section, rather then allocated into RAM using malloc. So you will notice that right off the bat, as there is an API change where you don't have to worry about the malloc pointer.

#### Input/Output Format

The vocoder changes 320 samples of 16-bit signed PCM at 8 kHz to 4 bytes that are packed with 52-bits of information. It also does the reverse.

```
LSP     36-bits
Pitch    7-bits
Energy   5-bits
Voicing  4-bits
---------------
Total   52-bits
```

Each vocoder frame is packed into 7 bytes, with each component individually  ```gray``` coded.

#### Testing
Here's how I compile the test programs. Put the library file in ```/usr/local/lib``` and do a ```sudo ldconfig```:
```
  cc -O2 c2enc.c -o c2enc -lcodec1300 -lm
  cc -O2 c2dec.c -o c2dec -lcodec1300 -lm
```
Then you can run the test programs (the binaries in the repository are for Ubuntu 64-bit):
```
./c2enc hts1.raw hts1.c2
./c2dec hts1.c2 result.raw
aplay -f S16_LE result.raw
```
You can also use the original c2enc/c2dec in the codec2, to compare results. In this case you can:
```
xxd hts1.c2 hts1.hex
xxd hts1-o.c2 hts1-o.hex
diff hts1-o.hex hts1.hex
```
As it stands right now, the audio sounds the same as codec2, but the RAW output from ```c2dec``` has some different bytes. Just a few bytes here and there. They seem to be off by 1 bit when they are wrong.

```
$ diff hts1-o.hex hts1.hex 
6c6
< 00000050: ec4c d5f0 f291 775c ddf5 90f3 d363 dfff  .L....w\.....c..
---
> 00000050: ec4c d5f0 f291 575c ddf5 90f3 d363 dfff  .L....W\.....c..
25c25
< 00000180: 50f0 ae63 300c c750 f04d 7210 0cd5 d0f0  P..c0..P.Mr.....
---
> 00000180: 50f0 ae63 300c c750 f04d 7210 0dd5 d0f0  P..c0..P.Mr.....
```

