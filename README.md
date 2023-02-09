# SDL-1.2.16-aix43

An attempt of porting SDL-1.2 to AIX 4.3.3.
Feel free to use it for porting your favorite application to RS/6000!

Releases are compiled using xlC 6.0 for small footprint and maximum optimization.

# Sound
This version omits support for OSS, and rather incorporates drivers for IBM's UMS (Ultimedia Services) and the raw audio output using devices /dev/baud0 on Micro Channel systems and /dev/paud0 for PCI/onboard audio. 
Some people have reported instabilities using OSS under AIX, so let's try without.

Using the raw audio with a 100 ms buffer is the default; you shouldn't notice the delay.

To run any SDL using application using the UMS driver:

```
SDL_AUDIODRIVER="UMS" run_ums <application>
```

# Installation

Extract to /opt/SDL or whereever you feel is right.

# Limitations

I couldn't test many systems and graphics adapters, so rendering experience may very.
I had trouble making the SDL test utility "testsprite" not crash.
Feel invited to contribute ;-)

# Building

If you want to replicate:

```
. xlc-env-settings-aix43-ppc.sh
./configure --prefix=/opt/SDL --enable-oss=no --enable-pthread-sem=no
make
make install
```

# Additional SDL libraries
Releases contain compiled versions of
 - SDL_mixer
 - SDL_net
 - SDL_sound

Check out their repositories and licenses.

# License
Check the contained COPYING file for the authors' rights.
