#pragma once
#include "Core.h"
namespace guodesk {
class Controller;
class TidyWindow {
 Controller& owner; bool closing=false,applying=false;
 winrt::Microsoft::UI::Xaml::Window window{nullptr}; HWND hwnd{};
 winrt::Microsoft::UI::Xaml::Controls::Grid root{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::StackPanel list{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBlock summary{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::Button apply{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::Button cancel{nullptr};
 winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer closeTimer{nullptr};
 std::vector<PlanItem> plan; std::vector<std::wstring> unmatched;
 winrt::Microsoft::UI::Xaml::Media::Brush ThemeBrush(wchar_t const* key,winrt::Windows::UI::Color fallback);
 void Build();
public:
 TidyWindow(Controller&);
 ~TidyWindow();
 void Show();
};
}
