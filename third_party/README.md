# third_party

Vendor kits that this repository must not contain go here on your own machine or in CI:

- `zoom-sdk/` — Zoom Meeting SDK for Windows (download from the Zoom App Marketplace); pass its `x64` folder as `BACKHAUL_ZOOM_SDK_DIR`
- `asio-sdk/` — Steinberg ASIO SDK

Everything in this folder except this file is ignored by git.
