#include "Engine.h"

// Windowsアプリのエントリーポイント
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int nCmdShow) {
    // リークチェック構造体
    D3DResourceLeakChecker leakCheck;

    // エンジンのインスタンス作成
    Engine engine;

    // 初期化 (ウィンドウ作成・DirectX12初期化・リソース作成等)
    engine.Initialize(hInstance, nCmdShow, 1280, 720);

    // メインループの実行
    engine.Run();

    // 終了処理
    engine.Finalize();

    return 0;
}