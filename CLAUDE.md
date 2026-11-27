# backhaul — 作業ルール

現場用Zoom取り込みアプリ（Windows専用）。設計の要点は `docs/design.ja.md`。

## 進め方
- 設計とデザインの承認なしに実装・mainへの反映・リリースをしない。頼まれていない機能を足さない。
- 画面・報告・ドキュメントでは「版」と書かず「バージョン」と書く。
- サンプルやテストの名前は必ず架空名にする（実在の人物・会場・案件名を使わない）。
- コミットの作者メールは info@seventh-well.com。

## 入れてはいけないもの（公開リポ）
- Zoom Meeting SDK本体、Steinberg ASIO開発キット（`third_party/` に置き、コミットしない）
- Zoomの鍵類（Client ID/Secret、ZAK、会議のパスコード）

## コードの決まり
- `src/core/` は Zoom・Qt・Windows に依存させない。ここは必ず自動テストを付け、Linux と Windows の両方で通す。
- Zoom SDKに触れるコードは `src/zoom/` に閉じ込める（SDK更新の影響をここだけにするため）。
- VST 3 SDKに触れるコードは `src/plugins/` に閉じ込める（同じ理由）。
- 警告はエラー扱い（MSVC `/W4 /WX`、GCC/Clang `-Wall -Wextra -Wpedantic -Wconversion -Werror`）。
- エラーの握りつぶし、テストを通すためだけの仮実装をしない。

## 確かめ方
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```
