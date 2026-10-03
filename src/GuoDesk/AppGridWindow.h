#pragma once
#include "Core.h"
namespace guodesk {
class Controller;
class AppGridWindow {
 Controller& owner; bool closing=false;
 winrt::Microsoft::UI::Xaml::Window window{nullptr}; HWND hwnd{};
 winrt::Microsoft::UI::Xaml::Controls::Grid root{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::StackPanel list{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBox input{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBlock count{nullptr};
 std::vector<AppShortcut> apps; std::wstring query;
 winrt::Microsoft::UI::Xaml::Media::Brush ThemeBrush(wchar_t const* key,winrt::Windows::UI::Color fallback);
 void Scan(); void Rebuild(); void SaveGeometry();
 void Launch(AppShortcut const& a,std::wstring const& file={});
 winrt::fire_and_forget DropOn(AppShortcut a,winrt::Microsoft::UI::Xaml::DragEventArgs args);
 static LRESULT CALLBACK Subclass(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
public:
 AppGridWindow(Controller&);
 ~AppGridWindow();
 void Show();
};
}
