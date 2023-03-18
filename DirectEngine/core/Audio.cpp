// FROM https://learn.microsoft.com/en-us/windows/win32/xaudio2/how-to--load-audio-data-files-in-xaudio2
#include "Audio.h"
#include "../Helpers.h"
#include <assert.h>

HRESULT FindChunk(HANDLE hFile, DWORD fourcc, DWORD& dwChunkSize, DWORD& dwChunkDataPosition)
{
    HRESULT hr = S_OK;
    if (INVALID_SET_FILE_POINTER == SetFilePointer(hFile, 0, NULL, FILE_BEGIN))
        return HRESULT_FROM_WIN32(GetLastError());

    DWORD dwChunkType;
    DWORD dwChunkDataSize;
    DWORD dwRIFFDataSize = 0;
    DWORD dwFileType;
    DWORD bytesRead = 0;
    DWORD dwOffset = 0;

    while (hr == S_OK)
    {
        DWORD dwRead;
        if (0 == ReadFile(hFile, &dwChunkType, sizeof(DWORD), &dwRead, NULL))
            hr = HRESULT_FROM_WIN32(GetLastError());

        if (0 == ReadFile(hFile, &dwChunkDataSize, sizeof(DWORD), &dwRead, NULL))
            hr = HRESULT_FROM_WIN32(GetLastError());

        switch (dwChunkType)
        {
        case fourccRIFF:
            dwRIFFDataSize = dwChunkDataSize;
            dwChunkDataSize = 4;
            if (0 == ReadFile(hFile, &dwFileType, sizeof(DWORD), &dwRead, NULL))
                hr = HRESULT_FROM_WIN32(GetLastError());
            break;

        default:
            if (INVALID_SET_FILE_POINTER == SetFilePointer(hFile, dwChunkDataSize, NULL, FILE_CURRENT))
                return HRESULT_FROM_WIN32(GetLastError());
        }

        dwOffset += sizeof(DWORD) * 2;

        if (dwChunkType == fourcc)
        {
            dwChunkSize = dwChunkDataSize;
            dwChunkDataPosition = dwOffset;
            return S_OK;
        }

        dwOffset += dwChunkDataSize;

        if (bytesRead >= dwRIFFDataSize) return S_FALSE;

    }

    return S_OK;

}

HRESULT ReadChunkData(HANDLE hFile, void* buffer, DWORD buffersize, DWORD bufferoffset)
{
    HRESULT hr = S_OK;
    if (INVALID_SET_FILE_POINTER == SetFilePointer(hFile, bufferoffset, NULL, FILE_BEGIN))
        return HRESULT_FROM_WIN32(GetLastError());
    DWORD dwRead;
    if (0 == ReadFile(hFile, buffer, buffersize, &dwRead, NULL))
        hr = HRESULT_FROM_WIN32(GetLastError());
    return hr;
}

AudioBuffer* LoadAudioFile(const wchar_t* path, MemoryArena& arena)
{
    AudioBuffer* audioBuffer = NewObject(arena, AudioBuffer);

    HANDLE audioFile = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

    if (INVALID_HANDLE_VALUE == audioFile) throw;
    if (INVALID_SET_FILE_POINTER == SetFilePointer(audioFile, 0, NULL, FILE_BEGIN)) throw;

    DWORD dwChunkSize;
    DWORD dwChunkPosition;
    //check the file type, should be fourccWAVE or 'XWMA'
    FindChunk(audioFile, fourccRIFF, dwChunkSize, dwChunkPosition);
    DWORD filetype;
    ReadChunkData(audioFile, &filetype, sizeof(DWORD), dwChunkPosition);
    if (filetype != fourccWAVE) throw;

    FindChunk(audioFile, fourccFMT, dwChunkSize, dwChunkPosition);
    ReadChunkData(audioFile, &audioBuffer->wfx, dwChunkSize, dwChunkPosition);

    //fill out the audio data buffer with the contents of the fourccDATA chunk
    FindChunk(audioFile, fourccDATA, dwChunkSize, dwChunkPosition);
    BYTE* pDataBuffer = new BYTE[dwChunkSize]; // @todo: fix alloc
    ReadChunkData(audioFile, pDataBuffer, dwChunkSize, dwChunkPosition);

    audioBuffer->buffer.AudioBytes = dwChunkSize;  //size of the audio buffer in bytes
    audioBuffer->buffer.pAudioData = pDataBuffer;  //buffer containing audio data
    audioBuffer->buffer.Flags = XAUDIO2_END_OF_STREAM; // tell the source voice not to expect any data after this buffer

    return audioBuffer;
}

void AudioSource::PlaySound(IXAudio2* xaudio, AudioBuffer* audioBuffer)
{
    WAVEFORMATEX* format = (WAVEFORMATEX*)&audioBuffer->wfx;

    audioEmitter.ChannelCount = format->nChannels;
    audioEmitter.CurveDistanceScaler = audioEmitter.DopplerScaler = 1.f;

    if (format->nChannels == 2)
    {
        channelPositions[0] = LEFT_AZIMUTH;
        channelPositions[1] = RIGHT_AZIMUTH;
        audioEmitter.pChannelAzimuths = channelPositions;
        audioEmitter.ChannelRadius = .1f;
    }

    ThrowIfFailed(xaudio->CreateSourceVoice(&source, format));
    ThrowIfFailed(source->SubmitSourceBuffer(&audioBuffer->buffer));
    ThrowIfFailed(source->Start(0));
}