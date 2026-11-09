#include "Engine.h"
#include "DirectXManager.h"
#include "D3DResourceLeakChecker.h"

// Windowsアプリのエントリーポイント
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int nCmdShow) {
    // リークチェック構造体
    D3DResourceLeakChecker leakCheck;

    // DirectX基盤クラスを生成し、Engineへ渡す。
    DirectXManager* dxManager = new DirectXManager();
    {
        Engine engine;
        engine.Initialize(hInstance, nCmdShow, dxManager);
        engine.Run();
        engine.Finalize();
    }

    // Engineの所有物を解放してからDirectX基盤クラスを解放する。
    delete dxManager;
    return 0;
}
