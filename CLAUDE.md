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
- 入場券サーバーの鍵（`ADMIN_TOKEN`、発行したライセンスキー）。Cloudflare の「秘密の値」にだけ置く
- 販売の鍵（Stripe の `sk_…`・`whsec_…`、Resend の `re_…`）。同じく Cloudflare の「秘密の値」にだけ置く

## コードの決まり
- `src/core/` は Zoom・Qt・Windows に依存させない。ここは必ず自動テストを付け、Linux と Windows の両方で通す。
- Zoom SDKに触れるコードは `src/zoom/` に閉じ込める（SDK更新の影響をここだけにするため）。
- VST 3 SDKに触れるコードは `src/plugins/` に閉じ込める（同じ理由）。
- 警告はエラー扱い（MSVC `/W4 /WX`、GCC/Clang `-Wall -Wextra -Wpedantic -Wconversion -Werror`）。
- エラーの握りつぶし、テストを通すためだけの仮実装をしない。
- 画面の文は日本語で書き、QMLでは `qsTr("…")`、C++（`src/app/`）では `T("…")` で包む。英語は `src/app/dictionary_en.cpp` に1行足す（最初の表示は英語、設定で日本語に切り替え）。`python3 tools/check_texts.py` が抜けを見つける。

## 確かめ方
入場券サーバー（`server/`）は `npm ci && npm run check && npm test`。販売ページ（`site/`）は `npm ci && npm run build`。アプリは下のとおり。
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```
