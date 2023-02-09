/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2012 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Carsten Griwodz
    griff@kom.tu-darmstadt.de

    based on linux/SDL_dspaudio.c by Sam Lantinga

    Reworked 2022 by Christian Holzapfel
*/
#include "SDL_config.h"

/* Allow access to a raw mixing buffer */

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "SDL_audio.h"
#include "../SDL_audio_c.h"
#include "../SDL_audiodev_c.h"
#include "SDL_umsaudio.h"

/* The tag name used by UMS audio */
#define UMS_DRIVER_NAME         "ums"

/* #define DEBUG_AUDIO 1 */

/* Audio driver functions */
static int UMS_OpenAudio(_THIS, SDL_AudioSpec *spec);
static void UMS_PlayAudio(_THIS);
static Uint8 *UMS_GetAudioBuf(_THIS);
static void UMS_CloseAudio(_THIS);
static void UMS_WaitAudio(_THIS);

/* Audio driver bootstrap functions */
static int Audio_Available(void)
{
    return 1;
}

static void Audio_DeleteDevice(_THIS)
{
    if(this->hidden->playbuf._buffer) SDL_free(this->hidden->playbuf._buffer);
    _somFree( _dev );
    SDL_free(this->hidden);
    SDL_free(this);
}

static SDL_AudioDevice *Audio_CreateDevice(int devindex)
{
    SDL_AudioDevice *this;
    UMSAudioDeviceMClass audio_device_class;
    UMSAudioDeviceMClass_ErrorCode audio_device_class_error;
	char* error_string;
	char* audio_formats_alias;
	char* audio_inputs_alias;
	char* audio_outputs_alias;

    /*
     * Allocate and initialize management storage and private management
     * storage for this SDL-using library.
     */
    this = (SDL_AudioDevice *)SDL_malloc(sizeof(SDL_AudioDevice));
    if ( this ) {
        SDL_memset(this, 0, (sizeof *this));
        this->hidden = (struct SDL_PrivateAudioData *)SDL_malloc((sizeof *this->hidden));
    }
    if ( (this == NULL) || (this->hidden == NULL) ) {
        SDL_OutOfMemory();
        if ( this ) {
            SDL_free(this);
        }
        return(0);
    }
    SDL_memset(this->hidden, 0, (sizeof *this->hidden));
#ifdef DEBUG_AUDIO
    fprintf(stderr, "Creating UMS Audio device\n");
#endif

    /*
     * Calls for UMS env initialization and audio object construction.
     */
    _ev = somGetGlobalEnvironment();
    
    audio_device_class = UMSAudioDeviceNewClass(UMSAudioDevice_MajorVersion,
												UMSAudioDevice_MinorVersion);
	if (audio_device_class == NULL)
	{
	    fprintf(stderr, "can't create AudioDeviceMClass metaclass\n");
	    return(0);
	}
    
    _dev = UMSAudioDeviceMClass_make_by_alias(audio_device_class,
													  _ev,
													  "Audio",
													  "PLAY",
													  UMSAudioDevice_BlockingIO,
													  &audio_device_class_error,
													  &error_string,
													  &audio_formats_alias,
													  &audio_inputs_alias,
													  &audio_outputs_alias);
	if (_dev == NULL)
	{
		fprintf(stderr, "can't create audio device object\n");
		return(0);
	}

    /*
     * Set the function pointers.
     */
    this->OpenAudio   = UMS_OpenAudio;
    this->WaitAudio   = UMS_WaitAudio;    
    this->PlayAudio   = UMS_PlayAudio;
    this->GetAudioBuf = UMS_GetAudioBuf;
    this->CloseAudio  = UMS_CloseAudio;
    this->free        = Audio_DeleteDevice;

#ifdef DEBUG_AUDIO
    fprintf(stderr, "done\n");
#endif
    return this;
}

AudioBootStrap UMS_bootstrap = {
	UMS_DRIVER_NAME, "AIX UMS audio",
	Audio_Available, Audio_CreateDevice
};

static Uint8 *UMS_GetAudioBuf(_THIS)
{
#ifdef DEBUG_AUDIO
    fprintf(stderr, "enter UMS_GetAudioBuf\n");
#endif
    return this->hidden->playbuf._buffer;
}

static void UMS_CloseAudio(_THIS)
{
    UMSAudioDevice_ReturnCode rc;

#ifdef DEBUG_AUDIO
    fprintf(stderr, "enter UMS_CloseAudio\n");
#endif
    rc = UMSAudioDevice_play_remaining_data(_dev, _ev, TRUE);
    rc = UMSAudioDevice_stop(_dev, _ev);
    rc = UMSAudioDevice_close(_dev, _ev);
}

/* This function waits until it is possible to write a full sound buffer */
static void UMS_WaitAudio(_THIS)
{
    /* We're in blocking mode, so there's nothing to do here */
}

static void UMS_PlayAudio(_THIS)
{
    UMSAudioDevice_ReturnCode rc;
    long                      samplesToWrite;
    long                      samplesWritten;
    
#ifdef DEBUG_AUDIO
    fprintf(stderr, "enter UMS_PlayAudio\n");
#endif
    samplesToWrite = this->hidden->playbuf._length/this->hidden->bytesPerSample;
    do
    {
        rc = UMSAudioDevice_write(_dev, _ev, &this->hidden->playbuf, samplesToWrite, &samplesWritten);
	    samplesToWrite -= samplesWritten;

	    /* rc values: UMSAudioDevice_Success
	     *            UMSAudioDevice_Failure
	     *            UMSAudioDevice_Preempted
	     *            UMSAudioDevice_Interrupted
	     *            UMSAudioDevice_DeviceError
	     */
	    if ( rc != UMSAudioDevice_Success ) {
#ifdef DEBUG_AUDIO
	        fprintf(stderr, "Returning from PlayAudio with devices error\n");
#endif
	        return;
	    }
    }
    while(samplesToWrite>0);

#ifdef DEBUG_AUDIO
    fprintf(stderr, "Wrote audio data and swapped buffer\n");
#endif
}

static int UMS_OpenAudio(_THIS, SDL_AudioSpec *spec)
{
    long   lgain;
    long   rgain;
    long   outRate;
    long   outBufSize;
    long   bitsPerSample;
    long   samplesPerSec;
    long   success;
    Uint16 test_format;
    int    dmaSize;
    UMSAudioDevice_ReturnCode rc;

#ifdef DEBUG_AUDIO
    fprintf(stderr, "enter UMS_OpenAudio\n");
#endif

    /* possible PCM, A_LAW or MU_LAW */
    rc = UMSAudioDevice_set_audio_format_type(_dev, _ev, "PCM");

    success = 0;
    test_format = SDL_FirstAudioFormat(spec->format);
    do
    {
#ifdef DEBUG_AUDIO
        fprintf(stderr, "Trying format 0x%4.4x: freq %u, \n", test_format, spec->freq);
#endif
        switch ( test_format )
        {
        case AUDIO_U8:
	        success       = 1;
            bitsPerSample = 8;
            if (  (UMSAudioDevice_set_sample_rate(_dev, _ev, spec->freq, &outRate) != UMSAudioDevice_Success)
                ||(UMSAudioDevice_set_byte_order(_dev, _ev, "MSB") != UMSAudioDevice_Success)           /* irrelevant */
            	||(UMSAudioDevice_set_number_format(_dev, _ev, "UNSIGNED") != UMSAudioDevice_Success))  /* possible SIGNED, UNSIGNED, or TWOS_COMPLEMENT */
            {
                success = 0;
            }
            break;
        case AUDIO_S16LSB:
	        success       = 1;
            bitsPerSample = 16;
            if (  (UMSAudioDevice_set_sample_rate(_dev, _ev, spec->freq, &outRate) != UMSAudioDevice_Success)
                ||(UMSAudioDevice_set_byte_order(_dev, _ev, "LSB") != UMSAudioDevice_Success)
            	||(UMSAudioDevice_set_number_format(_dev, _ev, "TWOS_COMPLEMENT") != UMSAudioDevice_Success))  /* possible SIGNED, UNSIGNED, or TWOS_COMPLEMENT */
            {
                success = 0;
            }
            break;
        case AUDIO_S16MSB:
	        success       = 1;
            bitsPerSample = 16;
            if (  (UMSAudioDevice_set_sample_rate(_dev, _ev, spec->freq, &outRate) != UMSAudioDevice_Success)
                ||(UMSAudioDevice_set_byte_order( _dev, _ev, "MSB") != UMSAudioDevice_Success)
				||(UMSAudioDevice_set_number_format(_dev, _ev, "TWOS_COMPLEMENT") != UMSAudioDevice_Success))  /* possible SIGNED, UNSIGNED, or TWOS_COMPLEMENT */
            {
                success = 0;
            }
            break;
#if 0
/*
 * These formats are not used by any real life systems so they are not
 * needed here.
 */
        case AUDIO_S8:
            success       = 1;
            bitsPerSample = 8;
            if (  (UMSAudioDevice_set_sample_rate(_dev, _ev, spec->freq, &outRate) != UMSAudioDevice_Success)
                ||(UMSAudioDevice_set_byte_order( _dev, _ev, "MSB") != UMSAudioDevice_Success)                 /* irrelevant */
            	||(UMSAudioDevice_set_number_format(_dev, _ev, "TWOS_COMPLEMENT") != UMSAudioDevice_Success))  /* possible SIGNED, UNSIGNED, or TWOS_COMPLEMENT */
            {
                success = 0;
            }
            break;
        case AUDIO_U16LSB:          
	        success       = 1;
            bitsPerSample = 16;
            if (  (UMSAudioDevice_set_sample_rate(_dev, _ev, spec->freq, &outRate) != UMSAudioDevice_Success)
                ||(UMSAudioDevice_set_byte_order(_dev, _ev, "LSB") != UMSAudioDevice_Success)           /* irrelevant */
            	||(UMSAudioDevice_set_number_format(_dev, _ev, "UNSIGNED") != UMSAudioDevice_Success))  /* possible SIGNED, UNSIGNED, or TWOS_COMPLEMENT */
            {
                success = 0;
            }
            break;
        case AUDIO_U16MSB:  
	        success       = 1;
            bitsPerSample = 16;
            if (  (UMSAudioDevice_set_sample_rate(_dev, _ev, spec->freq, &outRate ) != UMSAudioDevice_Success)
                ||(UMSAudioDevice_set_byte_order(_dev, _ev, "MSB") != UMSAudioDevice_Success)           /* irrelevant */
            	||(UMSAudioDevice_set_number_format(_dev, _ev, "UNSIGNED") != UMSAudioDevice_Success))  /* possible SIGNED, UNSIGNED, or TWOS_COMPLEMENT */
            {
                success = 0;
            }
            break;
#endif
        default:
            break;
        }
        if ( ! success ) {
            test_format = SDL_NextAudioFormat();
        }
    }
    while ( ! success && test_format );

    if ( success == 0 ) {
        SDL_SetError("Couldn't find any hardware audio formats");
        return -1;
    }

#ifdef DEBUG_AUDIO
    fprintf(stderr, "%u Bits per sample, %u channels, size %d\n", bitsPerSample, spec->channels, spec->size);
#endif

    spec->format = test_format;

    this->hidden->bytesPerSample   = (bitsPerSample / 8) * spec->channels;
    samplesPerSec                  = this->hidden->bytesPerSample * outRate;

    this->hidden->playbuf._length  = spec->size;
    this->hidden->playbuf._maximum = spec->size;
    this->hidden->playbuf._buffer  = (unsigned char*)malloc(spec->size);
  	if ( this->hidden->playbuf._buffer == NULL ) {
   		UMS_CloseAudio(this);
   		return(-1);
   	}
    
    #ifdef DEBUG_AUDIO
    fprintf(stderr, "%u bytes per sample, %u samples per sec, buffer %u bytes\n", this->hidden->bytesPerSample, samplesPerSec, spec->size);
    #endif

    rc = UMSAudioDevice_set_bits_per_sample(_dev, _ev, bitsPerSample);

    /*
     * Request a new DMA buffer size, maximum requested size 2048.
     * Takes effect with next initialize() call.
     * Devices may or may not support DMA.
     * Available buffer sizes are device dependent.
	 * A value of 256 seemslike a good compromise
	 * between interrupt load and audible delay.
     */
	dmaSize = 256;
    rc = UMSAudioDevice_set_DMA_buffer_size( _dev, _ev, dmaSize, &outBufSize);
    #ifdef DEBUG_AUDIO
   	fprintf(stderr, "Audio DMA buffer size: %u, requested: %u\n", outBufSize, dmaSize);
    #endif
	
	rc = UMSAudioDevice_set_audio_buffer_size(_dev, _ev, 2*spec->size, &outBufSize);
    #ifdef DEBUG_AUDIO
    fprintf(stderr, "Audio buffer size: %u\n", outBufSize);
    #endif

    /*
     * Set mono or stereo.
     * Takes effect with next initialize() call.
     */
    if ( spec->channels != 1 ) 			/* reduce to mono or stereo */
    	spec->channels = 2;
    rc = UMSAudioDevice_set_number_of_channels( _dev, _ev, spec->channels);

    /*
     * Switches the time format to the new format, immediately.
     * possible UMSAudioTypes_Msecs, UMSAudioTypes_Bytes or UMSAudioTypes_Samples
     */
    rc = UMSAudioDevice_set_time_format( _dev, _ev, UMSAudioTypes_Bytes );

    lgain = 100; /*maximum left input gain*/
    rgain = 100; /*maimum right input gain*/
    rc = UMSAudioDevice_disable_output( _dev, _ev, "INTERNAL_SPEAKER");
    rc = UMSAudioDevice_enable_output( _dev, _ev, "LINE_OUT", &lgain, &rgain);

    /*
     * Set the volume.
     * Takes effect immediately.
     */
    rc = UMSAudioDevice_set_volume( _dev, _ev, 100);

    /*
     * Set the balance.
     * Takes effect immediately.
     */
    rc = UMSAudioDevice_set_balance( _dev, _ev, 0);

    rc = UMSAudioDevice_initialize( _dev, _ev );
    rc = UMSAudioDevice_start( _dev, _ev );

    /* We're ready to rock and roll. :-) */
    return 0;
}


