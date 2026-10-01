#pragma once
#include "Core.h"
namespace guodesk {
class Controller;
class ClockWindow {
 Controller& owner; bool closing=false;
 winrt::Microsoft::UI::Xaml::Window window{nullptr}; HWND hwnd{};
 winrt::Microsoft::UI::Xaml::Controls::Grid root{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBlock time{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBlock date{nullptr};
 winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer tick{nullptr};
 winrt::Microsoft::UI::Xaml::Media::Brush ThemeBrush(wchar_t const* key,winrt::Windows::UI::Color fallback);
 void Update();
public:
 ClockWindow(Controller&);
 ~ClockWindow();
 void Show();
};
}
