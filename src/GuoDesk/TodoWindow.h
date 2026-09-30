#pragma once
#include "Core.h"
namespace guodesk {
class Controller;
class TodoWindow {
 Controller& owner; bool closing=false;
 winrt::Microsoft::UI::Xaml::Window window{nullptr}; HWND hwnd{};
 winrt::Microsoft::UI::Xaml::Controls::Grid root{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::StackPanel list{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBlock count{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBox input{nullptr};
 winrt::Microsoft::UI::Xaml::Media::Brush ThemeBrush(wchar_t const* key,winrt::Windows::UI::Color fallback);
 void Rebuild();
 std::wstring CountText();
 void Add();
public:
 TodoWindow(Controller&);
 ~TodoWindow();
 void Show();
};
}
