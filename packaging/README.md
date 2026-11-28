# 配る形にする（インストーラーと更新）

ここにあるものは、Windows 用のインストーラーづくりと、アプリが自分を更新するための一覧づくりに使う。実際に動かすのは kajisho5/backhaul-sdk のビルド（Zoom の SDK を持っているのがそちらだけなので）。

| ファイル | 中身 |
| --- | --- |
| `backhaul.iss` | インストーラーの設計図（Inno Setup）。ユーザーごとに入れ、管理者権限を求めない |
| `fetch_winsparkle.ps1` | 更新の部品（WinSparkle）を決まったバージョンで取ってくる。中身が変わっていたら止まる |
| `fetch_innosetup.ps1` | Inno Setup を決まったバージョンで入れる。同じく、中身が変わっていたら止まる |
| `make_feed.py` | インストーラーに署名し、`latest.json` と `appcast.xml` を作る |
| `update_public_key.txt` | 署名を確かめる鍵（公開してよいほう）。プログラムに組み込まれる |
| `notes/<バージョン>.en.txt`・`.ja.txt` | そのバージョンの変更点。アプリの「Software」のページと更新の窓に出る |

## 更新を出す手順
1. `CMakeLists.txt` のバージョン番号を上げる（メジャー番号は本人の指示なしに上げない）。
2. `packaging/notes/<新しい番号>.en.txt` と `.ja.txt` を書く。英語が無いとビルドが止まる。
3. main に入れる。
4. backhaul-sdk の「SDK入りでビルドを確かめる」を、`asio` を外し、`update_release` を入れて動かす。

ビルドは、今出ているバージョンより新しくないと止まる。インストーラーを置いてから一覧を置くので、途中で失敗しても、配ったアプリが無いファイルを取りに行くことはない。

## 署名の鍵
- 秘密のほうは backhaul-sdk の GitHub の「秘密の値」`UPDATE_SIGNING_KEY` にだけ置く（PEM 形式の Ed25519）。このリポには入れない。
- なくすと、配ったアプリを更新できなくなる（新しい鍵で作ったインストーラーを、配ったアプリは受け付けない）。控えを安全な場所に持っておく。
- 鍵を替えるときは、`update_public_key.txt` を新しい鍵に替えたバージョンを**古い鍵で**署名して先に配る。

## 確かめ方
```
python3 -m pip install cryptography
python3 -m unittest discover -s packaging -p "test_*.py"
```
