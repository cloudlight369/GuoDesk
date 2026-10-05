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
std::vector<std::wstring> ClipPaths();
bool HasClipFiles();
bool ClipIsMove();
void ClipboardCopy(std::vector<std::wstring> const& paths,bool cut);
std::vector<std::wstring> PasteInto(std::wstring const& destDir);
std::wstring CreateFolder(std::wstring const& dir,std::wstring const& baseName);
void RenamePath(std::wstring const& path,std::wstring const& newName);
void DeletePaths(std::vector<std::wstring> const& paths,bool permanent);
std::vector<AppShortcut> EnumerateApps();
std::vector<char> MakeHdrop(std::vector<std::wstring> const& paths);
HRESULT DragOut(HWND,std::vector<std::wstring> const& paths);
void Fit(Zone& zone);
HWND DesktopHost();
bool Attach(HWND window,HWND host);
void Detach(HWND window);
winrt::fire_and_forget LoadIcon(std::wstring path,winrt::Microsoft::UI::Xaml::Controls::Image image);
}
