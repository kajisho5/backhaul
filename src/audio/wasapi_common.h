// Shared by the WASAPI output and input: Windows headers in the order they need,
// and small helpers for COM objects and text.
#pragma once

#include <windows.h>

#include <objbase.h>

#include <initguid.h>

#include <mmreg.h>

#include <ks.h>

#include <ksmedia.h>

#include <audioclient.h>
#include <mmdeviceapi.h>

#include <propidl.h>

#include <propkeydef.h>

#include <propsys.h>

#include <functiondiscoverykeys_devpkey.h>

#include <cstdio>
#include <string>

namespace backhaul::wasapi {

template <typename T>
class Com {
 public:
  Com() = default;
  ~Com() { reset(); }
  Com(const Com&) = delete;
  Com& operator=(const Com&) = delete;
  T** put() {
    reset();
    return &p_;
  }
  T* get() const { return p_; }
  T* operator->() const { return p_; }
  explicit operator bool() const { return p_ != nullptr; }
  void reset() {
    if (p_ != nullptr) {
      p_->Release();
      p_ = nullptr;
    }
  }

 private:
  T* p_ = nullptr;
};

class ComApartment {
 public:
  ComApartment() : ok_(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {}
  ~ComApartment() {
    if (ok_) CoUninitialize();
  }
  ComApartment(const ComApartment&) = delete;
  ComApartment& operator=(const ComApartment&) = delete;

 private:
  bool ok_;
};

inline std::string toUtf8(const wchar_t* text) {
  if (text == nullptr || *text == L'\0') return {};
  const int need = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
  if (need <= 1) return {};
  std::string out(static_cast<std::size_t>(need), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text, -1, out.data(), need, nullptr, nullptr);
  out.resize(static_cast<std::size_t>(need) - 1);
  return out;
}

inline std::wstring toWide(const std::string& text) {
  if (text.empty()) return {};
  const int need = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
  if (need <= 1) return {};
  std::wstring out(static_cast<std::size_t>(need), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, out.data(), need);
  out.resize(static_cast<std::size_t>(need) - 1);
  return out;
}

inline std::string hr(const char* what, HRESULT code) {
  char buffer[160];
  std::snprintf(buffer, sizeof(buffer), "%s (0x%08lX)", what, static_cast<unsigned long>(code));
  return buffer;
}

}  // namespace backhaul::wasapi
