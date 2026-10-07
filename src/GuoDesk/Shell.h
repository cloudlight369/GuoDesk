#pragma once
#include <atomic>
#include "Core.h"
namespace guodesk::shell {
// 后台文件操作的取消令牌与进度回调（done,total,currentName）
using CancelFlag=std::shared_ptr<std::atomic<bool>>;
using ProgressFn=std::function<void(long long,long long,std::wstring const&)>;
struct TransferResult { std::vector<std::wstring> made; long long skipped=0,failed=0; bool cancelled=false; };
struct DeleteResult { std::vector<std::wstring> gone; long long failed=0,nuked=0; bool cancelled=false; };
struct ClipFiles { std::vector<std::wstring> paths; bool move=false, busy=false; };
struct DownloadResult { std::wstring path; long long bytes=0,total=0; unsigned status=0; bool cancelled=false,tooLarge=false; };
HWND Handle(winrt::Microsoft::UI::Xaml::Window const& window);
std::vector<std::wstring> Pick(HWND owner,bool folder=false,std::wstring const& title={});
std::wstring SaveFile(HWND owner,wchar_t const* defaultName);
void Open(HWND owner,std::wstring const& path);
void Reveal(HWND owner,std::wstring const& path);
int EntryContextMenu(HWND hwnd,std::wstring const& path,std::vector<std::wstring> const& custom);
std::wstring Name(std::wstring const& path);
ClipFiles ReadClipFiles();
bool HasClipFiles();
bool RecycleCapable(std::wstring const& path);
long long CountNonRecyclable(std::vector<std::wstring> const& paths);
void ClipboardCopy(std::vector<std::wstring> const& paths,bool cut);
TransferResult TransferFiles(std::vector<std::wstring> const& sources,std::wstring const& destDir,bool move,CancelFlag const& cancel,ProgressFn const& progress);
DownloadResult DownloadFile(std::wstring const& url,std::wstring const& destDir,CancelFlag const& cancel,ProgressFn const& progress,long long limitBytes);
std::wstring CreateFolder(std::wstring const& dir,std::wstring const& baseName);
void RenamePath(std::wstring const& path,std::wstring const& newName);
DeleteResult DeleteFiles(std::vector<std::wstring> const& paths,bool permanent,CancelFlag const& cancel,ProgressFn const& progress);
std::vector<AppShortcut> EnumerateApps();
std::vector<char> MakeHdrop(std::vector<std::wstring> const& paths);
HRESULT DragOut(HWND,std::vector<std::wstring> const& paths);
void Fit(Zone& zone,int dpi=96);
HWND DesktopHost();
bool Attach(HWND window,HWND host);
void Detach(HWND window);
winrt::fire_and_forget LoadIcon(std::wstring path,winrt::Microsoft::UI::Xaml::Controls::Image image);
}
