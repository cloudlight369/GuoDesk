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
 // 全区共用这一块面板：每次 Open 换一代，排队中的"关闭"只作废它那一代，别把刚摊开的下一叠也带走
 unsigned long long generation=0;
 bool dragArmed=false, dragFired=false; POINT dragStart{}; std::wstring dragPath;
 void OnKey(winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& a);
 void PeekDragMoved(); void BeginDrag(std::wstring const& path);
 winrt::Microsoft::UI::Xaml::Media::Brush ThemeBrush(wchar_t const* key,winrt::Windows::UI::Color fallback);
 void RequestClose();
public:
 StackPeekWindow(Controller&);
 ~StackPeekWindow();
 void Open(std::wstring zone,std::wstring stack,std::wstring const& title,std::vector<std::wstring> const& items,RECT const& anchor,int anchorDpi);
};
}
