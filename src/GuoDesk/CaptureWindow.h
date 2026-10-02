#pragma once
#include "Core.h"
namespace guodesk {
class Controller;
class CaptureWindow {
 Controller& owner; bool closing=false;
 winrt::Microsoft::UI::Xaml::Window window{nullptr}; HWND hwnd{};
 winrt::Microsoft::UI::Xaml::Controls::Grid root{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::Border paper{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBox body{nullptr};
 winrt::Microsoft::UI::Xaml::Media::Brush ThemeBrush(wchar_t const* key,winrt::Windows::UI::Color fallback);
 void Commit(bool asTodo);
public:
 CaptureWindow(Controller&);
 ~CaptureWindow();
 void Show();
};
}
