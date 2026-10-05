#pragma once
#include "Core.h"
namespace guodesk {
class Controller;
class PreviewWindow {
 Controller& owner; bool closing=false; std::shared_ptr<bool> alive{std::make_shared<bool>(true)};
 winrt::Microsoft::UI::Xaml::Window window{nullptr}; HWND hwnd{};
 winrt::Microsoft::UI::Xaml::Controls::Grid root{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::Border card{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ScrollViewer scroller{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBlock head{nullptr},meta{nullptr},bodyText{nullptr},hint{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::Image picture{nullptr};
 std::vector<std::wstring> paths; size_t index=0;
 void OnKey(winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& a);
 winrt::Microsoft::UI::Xaml::Media::Brush ThemeBrush(wchar_t const* key,winrt::Windows::UI::Color fallback);
 void RequestClose(); void Render(); void Step(int delta);
public:
 PreviewWindow(Controller&);
 ~PreviewWindow();
 void Open(std::vector<std::wstring> const& list,size_t start);
};
}
