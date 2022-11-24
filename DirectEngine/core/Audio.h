#pragma once

#include "Memory.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <xaudio2Redist.h>
#include <x3daudioRedist.h>

#ifdef _XBOX //Big-Endian
#define fourccRIFF 'RIFF'
#define fourccDATA 'data'
#define fourccFMT 'fmt '
#define fourccWAVE 'WAVE'
#define fourccXWMA 'XWMA'
#define fourccDPDS 'dpds'
#endif

#ifndef _XBOX //Little-Endian
#define fourccRIFF 'FFIR'
#define fourccDATA 'atad'
#define fourccFMT ' tmf'
#define fourccWAVE 'EVAW'
#define fourccXWMA 'AMWX'
#define fourccDPDS 'sdpd'
#endif

#define LEFT_AZIMUTH (3.f * X3DAUDIO_PI / 2.f);
#define RIGHT_AZIMUTH (X3DAUDIO_PI / 2.f);
#define FRONT_LEFT_AZIMUTH (7.f * X3DAUDIO_PI / 4.f);
#define FRONT_RIGHT_AZIMUTH (X3DAUDIO_PI / 4.f);
#define FRONT_CENTER_AZIMUTH (0.f);
#define LOW_FREQUENCY_AZIMUTH (X3DAUDIO_2PI);
#define BACK_LEFT_AZIMUTH (5.f * X3DAUDIO_PI / 4.f);
#define BACK_RIGHT_AZIMUTH (3.f * X3DAUDIO_PI / 4.f);
#define BACK_CENTER_AZIMUTH (X3DAUDIO_PI);

enum class AudioFile
{
	PlayerDamage,
	EnemyDeath,
	Shoot
};

struct AudioBuffer
{
	WAVEFORMATEXTENSIBLE wfx = { 0 };
	XAUDIO2_BUFFER buffer = { 0 };
};

struct AudioSource
{
	IXAudio2SourceVoice* source;
	X3DAUDIO_EMITTER audioEmitter;
	float channelPositions[2];

	void PlaySound(IXAudio2* xaudio, AudioBuffer* audioBuffer);
};

HRESULT FindChunk(HANDLE hFile, DWORD fourcc, DWORD& dwChunkSize, DWORD& dwChunkDataPosition);
HRESULT ReadChunkData(HANDLE hFile, void* buffer, DWORD buffersize, DWORD bufferoffset);
AudioBuffer* LoadAudioFile(const wchar_t* path, MemoryArena& arena);
void PlaySound(IXAudio2* xaudio, IXAudio2SourceVoice* source, AudioBuffer* audioBuffer);