#pragma once
#include "Core.h"
namespace guodesk::shell {
HWND Handle(winrt::Microsoft::UI::Xaml::Window const& window);
std::vector<std::wstring> Pick(HWND owner,bool folder=false,std::wstring const& title={});
std::wstring SaveFile(HWND owner,wchar_t const* defaultName);
void Open(HWND owner,std::wstring const& path);
void Reveal(HWND owner,std::wstring const& path);
int EntryContextMenu(HWND hwnd,std::wstring const& path,std::vector<std::wstring> const& custom);
std::wstring Name(std::wstring const& path);
void Fit(Zone& zone);
HWND DesktopHost();
bool Attach(HWND window,HWND host);
void Detach(HWND window);
winrt::fire_and_forget LoadIcon(std::wstring path,winrt::Microsoft::UI::Xaml::Controls::Image image);
}
