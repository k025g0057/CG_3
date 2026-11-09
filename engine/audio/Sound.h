#pragma once
#include <xaudio2.h>
#pragma comment(lib,"xaudio2.lib")
#include <fstream>
#include <wrl/client.h> 

using Microsoft::WRL::ComPtr; 

struct ChunkHeader
{
    char id[4];
    int32_t size;
};

struct RiffHeader
{
    ChunkHeader chunk;
    char type[4];
};

struct FormatChunk
{
    ChunkHeader chunk;
    WAVEFORMATEX fmt;
};

struct SoundData
{
    // 波形フォーマット
    WAVEFORMATEX wfex;
    // バッファの先頭アドレス
    BYTE* pBuffer;
    // バッファのサイズ
    unsigned int bufferSize;
};


class Sound{
public:
    // 初期化処理
    void Initialize();

    void Finalize();

    SoundData SoundLoadWave(const char* filename);

    void SoundUnload(SoundData* soundData);

    void SoundPlayWave(const SoundData& soundData);
private:
    ComPtr<IXAudio2> xAudio2;

    IXAudio2MasteringVoice* masterVoice = nullptr;
};

