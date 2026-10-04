#pragma once
#include "Core.h"
namespace guodesk {
class Controller;
class ClockWindow {
 Controller& owner; bool closing=false; bool analog=false;
 winrt::Microsoft::UI::Xaml::Window window{nullptr}; HWND hwnd{};
 winrt::Microsoft::UI::Xaml::Controls::Grid root{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::Canvas face{nullptr};
 winrt::Microsoft::UI::Xaml::Shapes::Rectangle hourHand{nullptr},minuteHand{nullptr},secondHand{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBlock time{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBlock date{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBlock lunar{nullptr};
 winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer tick{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::Flyout calFlyout{nullptr};
 std::vector<std::wstring> bgList; size_t bgIdx=0;
 winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer rotate{nullptr};
 winrt::Microsoft::UI::Xaml::Media::Brush ThemeBrush(wchar_t const* key,winrt::Windows::UI::Color fallback);
 void Update(); void BuildFace(); void Rotate(winrt::Microsoft::UI::Xaml::Shapes::Rectangle const& hand,double angle);
 void ApplyBackground(); void SetBgBrush(std::wstring const& path); void PopulateMenu(winrt::Microsoft::UI::Xaml::Controls::MenuFlyout const& menu);
public:
 ClockWindow(Controller&);
 ~ClockWindow();
 void Show();
};
}
