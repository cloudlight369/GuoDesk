#pragma once
#include "Core.h"
namespace guodesk {
class Controller;
// 叠放浮层：把一叠成员摊在分区外面的小窗里，单击打开、按住拖出去，分区高度一点不动
class StackPeekWindow {
 Controller& owner; bool closing=false; std::shared_ptr<bool> alive{std::make_shared<bool>(true)};
 winrt::Microsoft::UI::Xaml::Window window{nullptr}; HWND hwnd{};
 winrt::Microsoft::UI::Xaml::Controls::Grid root{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBlock head{nullptr}, hint{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::Grid board{nullptr};
 std::wstring zoneId, stackId;
 bool dragArmed=false; POINT dragStart{}; std::wstring dragPath;
 void OnKey(winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& a);
 void BeginDrag(std::wstring const& path);
 winrt::Microsoft::UI::Xaml::Media::Brush ThemeBrush(wchar_t const* key,winrt::Windows::UI::Color fallback);
 void RequestClose();
public:
 StackPeekWindow(Controller&);
 ~StackPeekWindow();
 void Open(std::wstring zone,std::wstring stack,std::wstring const& title,std::vector<std::wstring> const& items,RECT const& anchor);
};
}
