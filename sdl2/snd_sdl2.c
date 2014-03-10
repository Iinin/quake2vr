/*
	snd_sdl.c

	Sound code taken from SDLQuake and modified to work with Quake2
	Robert B�uml 2001-12-25

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

	$Id: snd_sdl.c,v 1.2 2002/02/09 20:29:38 relnev Exp $
*/

#include "SDL.h"

#include "../client/client.h"
#include "../client/snd_loc.h"

static int  snd_inited;
static dma_t *shm;

static void
paint_audio (void *unused, Uint8 * stream, int len)
{

	SDL_memset(stream,0,len)
		;
	if (shm) {
		int posOffset = (shm->samplebits / 4);
		if (posOffset > 0)
			posOffset = len / posOffset;
		else
			Com_Printf("FATAL\n");
		shm->buffer = stream;
		shm->samplepos += posOffset;
		// Check for samplepos overflow?
		S_PaintChannels (shm->samplepos);
	}
}

qboolean
SNDDMA_Init (void)
{
	SDL_AudioSpec desired, obtained;
	int desired_bits, freq;
	char reqdriver[128];
	cvar_t *s_sdldriver;

	s_sdldriver = (Cvar_Get("s_sdldriver", "auto", CVAR_ARCHIVE));
/*
#ifdef _WIN32
	s_sdldriver = (Cvar_Get("s_sdldriver", "xaudio2", CVAR_ARCHIVE));
#elif __linux__
	s_sdldriver = (Cvar_Get("s_sdldriver", "alsa", CVAR_ARCHIVE));
#elif __APPLE__
	s_sdldriver = (Cvar_Get("s_sdldriver", "CoreAudio", CVAR_ARCHIVE));
#else
	s_sdldriver = (Cvar_Get("s_sdldriver", "dsp", CVAR_ARCHIVE));
#endif
	*/
	if (strncmp(s_sdldriver->string,"default",8) && strncmp(s_sdldriver->string,"auto",5))
	{
#ifdef _WIN32
		_snprintf(reqdriver, sizeof(reqdriver), "%s=%s", "SDL_AUDIODRIVER", s_sdldriver->string);
#else
		snprintf(reqdriver, sizeof(reqdriver), "%s=%s", "SDL_AUDIODRIVER", s_sdldriver->string);
#endif
	}
	putenv(reqdriver);
	if (SDL_WasInit(SDL_INIT_EVERYTHING) == 0) {
		if (SDL_Init(SDL_INIT_AUDIO) < 0) {
			Com_Printf ("Couldn't init SDL audio: %s\n", SDL_GetError ());
			return 0;
		}
	} else if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
		if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
			Com_Printf ("Couldn't init SDL audio: %s\n", SDL_GetError ());
			return 0;
		} 

	}

	{
		const char* drivername = SDL_GetCurrentAudioDriver();
		if(drivername == NULL)
		{
			drivername = "(UNKNOWN)";
		}
		Com_Printf("SDL audio driver is \"%s\".\n", drivername);
	}
	
	snd_inited = 0;
	desired_bits = (Cvar_Get("sndbits", "16", CVAR_ARCHIVE))->value;

	/* Set up the desired format */
	freq = (Cvar_Get("s_khz", "0", CVAR_ARCHIVE))->value;
	if (freq == 44)
		desired.freq = 44100;
	else if (freq == 22)
		desired.freq = 22050;
	else
		desired.freq = 11025;
	
	switch (desired_bits) {
		case 8:
			desired.format = AUDIO_U8;
			break;
		case 16:
			if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
				desired.format = AUDIO_S16MSB;
			else
				desired.format = AUDIO_S16LSB;
			break;
		default:
			Com_Printf ("Unknown number of audio bits: %d\n", desired_bits);
			return 0;
	}
	desired.channels = (Cvar_Get("sndchannels", "2", CVAR_ARCHIVE))->value;
	
	if (desired.freq == 44100)
		desired.samples = 2048;
	else if (desired.freq == 22050)
		desired.samples = 1024;
	else
		desired.samples = 512;
	
	desired.callback = paint_audio;
	
	/* Open the audio device */
	if (SDL_OpenAudio (&desired, &obtained) < 0) {
		Com_Printf ("Couldn't open SDL audio: %s\n", SDL_GetError ());
		return 0;
	}

	/* Make sure we can support the audio format */
	switch (obtained.format) {
		case AUDIO_U8:
			/* Supported */
			break;
		case AUDIO_S16LSB:
		case AUDIO_S16MSB:
			if (((obtained.format == AUDIO_S16LSB) &&
				 (SDL_BYTEORDER == SDL_LIL_ENDIAN)) ||
				((obtained.format == AUDIO_S16MSB) &&
				 (SDL_BYTEORDER == SDL_BIG_ENDIAN))) {
				/* Supported */
				break;
			}
			/* Unsupported, fall through */ ;
		default:
			/* Not supported -- force SDL to do our bidding */
			SDL_CloseAudio ();
			if (SDL_OpenAudio (&desired, NULL) < 0) {
				Com_Printf ("Couldn't open SDL audio: %s\n", SDL_GetError ());
				return 0;
			}
			memcpy (&obtained, &desired, sizeof (desired));
			break;
	}
	SDL_PauseAudio (0);

	/* Fill the audio DMA information block */
	shm = &dma;
	shm->samplebits = (obtained.format & 0xFF);
	shm->speed = obtained.freq;
	shm->channels = obtained.channels;
	shm->samples = obtained.samples * shm->channels;
	shm->samplepos = 0;
	shm->submission_chunk = 1;
	shm->buffer = NULL;

	snd_inited = 1;
	return 1;
}

int
SNDDMA_GetDMAPos (void)
{
	return shm->samplepos;
}

void
SNDDMA_Shutdown (void)
{
	if (snd_inited) {
		SDL_CloseAudio ();
		snd_inited = 0;
	}

	if (SDL_WasInit(SDL_INIT_EVERYTHING) == SDL_INIT_AUDIO)
		SDL_Quit();
	else
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

/*

	SNDDMA_Submit

	Send sound to device if buffer isn't really the dma buffer

*/
void
SNDDMA_Submit (void)
{
}


void SNDDMA_BeginPainting(void)
{    
}

/*
===========
S_Activate

Called when the main window gains or loses focus.
The window have been destroyed and recreated
between a deactivate and an activate.
===========
*/
void S_Activate (qboolean active)
{
//

	SDL_PauseAudio(!active);

}