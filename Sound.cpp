#include "Sound.h"
#include <cassert> // assert を使うため

void Sound::Initialize() {
    HRESULT hr;

    // XAudioエンジンのインスタンスを生成
    hr = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
    assert(SUCCEEDED(hr)); // 成功したかチェック（Engine.cppでも使っている安全対策です）

    // マスターボイスを生成
    hr = xAudio2->CreateMasteringVoice(&masterVoice);
    assert(SUCCEEDED(hr));
}

void Sound::Finalize() {
    // XAudio2解放（これによって再生中の音もすべて安全に停止します）
    xAudio2.Reset();
}

SoundData Sound::SoundLoadWave(const char* filename) {
    // ①ファイルオープン
    // ファイル入力ストリームのインスタンス
    std::ifstream file;
    // .wavファイルをバイナリモードで開く
    file.open(filename, std::ios_base::binary);
    // ファイルオープン失敗を検出する
    assert(file.is_open());

    // ==========================================
    // ②.wavデータ読み込み
    // ==========================================

    // 【RIFFヘッダーの読み込み】
    RiffHeader riff;
    file.read((char*)&riff, sizeof(riff));
    // ファイルがRIFFかチェック
    if (strncmp(riff.chunk.id, "RIFF", 4) != 0) {
        assert(0);
    }
    // タイプがWAVEかチェック
    if (strncmp(riff.type, "WAVE", 4) != 0) {
        assert(0);
    }

    // 【Formatチャンクの読み込み】
    FormatChunk format = {};
    // チャンクヘッダーの確認
    file.read((char*)&format, sizeof(ChunkHeader));
    if (strncmp(format.chunk.id, "fmt ", 4) != 0) {
        assert(0);
    }
    // チャンク本体の読み込み
    assert(format.chunk.size <= sizeof(format.fmt));
    file.read((char*)&format.fmt, format.chunk.size);

    // 【Dataチャンクの読み込み】
    ChunkHeader data;
    file.read((char*)&data, sizeof(data));
    // JUNKチャンクを検出した場合
    while (strncmp(data.id, "data", 4) != 0) {
        // チャンクのサイズ分、読み取り位置を進める
        file.seekg(data.size, std::ios_base::cur);

        // 次のチャンクヘッダーを読み込む
        file.read((char*)&data, sizeof(data));

        // ファイルの末尾に達してしまった場合の安全対策
        if (file.eof()) {
            assert(0); // data チャンクが存在しない壊れたファイル
            break;
        }
    }

    // Dataチャンクのデータ部（波形データ）の読み込み
    char* pBuffer = new char[data.size];
    file.read(pBuffer, data.size);

    // ==========================================
    // ③ファイルクローズ
    // ==========================================
    // Waveファイルを閉じる
    file.close();

    // ==========================================
    // ④読み込んだ音声データをreturn
    // ==========================================
    SoundData soundData = {};

    soundData.wfex = format.fmt;                             // 読み込んだフォーマットを代入
    soundData.pBuffer = reinterpret_cast<BYTE*>(pBuffer);    // char* を BYTE* に変換して代入
    soundData.bufferSize = data.size;                        // 読み込んだデータサイズを代入

    return soundData;
}

void Sound::SoundUnload(SoundData* soundData) {
    // バッファのメモリを解放
    delete[] soundData->pBuffer;

    soundData->pBuffer = 0;
    soundData->bufferSize = 0;
    soundData->wfex = {};
}

void Sound::SoundPlayWave(const SoundData& soundData) {
    HRESULT hr;

    // 波形フォーマットを元にSourceVoiceの生成
    IXAudio2SourceVoice* pSourceVoice = nullptr;
    hr = xAudio2->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
    assert(SUCCEEDED(hr));

    // 再生する波形データの設定
    XAUDIO2_BUFFER buf{};
    buf.pAudioData = soundData.pBuffer;
    buf.AudioBytes = soundData.bufferSize;
    buf.Flags = XAUDIO2_END_OF_STREAM;

    // 波形データの再生
    hr = pSourceVoice->SubmitSourceBuffer(&buf);
    hr = pSourceVoice->Start();
}
