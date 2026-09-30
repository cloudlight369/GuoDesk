#include "pch.h"
#include "Shell.h"
#include "I18n.h"
#include <robuffer.h>
using namespace winrt;
namespace guodesk::shell {
HWND Handle(Microsoft::UI::Xaml::Window const& w){HWND h{};check_hresult(w.as<IWindowNative>()->get_WindowHandle(&h));return h;}
std::vector<std::wstring> Pick(HWND owner,bool folder,std::wstring const& title){com_ptr<IFileOpenDialog> dialog;check_hresult(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(dialog.put())));DWORD options{};check_hresult(dialog->GetOptions(&options));check_hresult(dialog->SetOptions(options|FOS_ALLOWMULTISELECT|FOS_FORCEFILESYSTEM|(folder?FOS_PICKFOLDERS:0)));dialog->SetTitle(!title.empty()?title.c_str():folder?i18n::Tr(L"添加文件夹入口").c_str():i18n::Tr(L"添加文件或应用入口").c_str());HRESULT hr=dialog->Show(owner);if(hr==HRESULT_FROM_WIN32(ERROR_CANCELLED))return {};check_hresult(hr);com_ptr<IShellItemArray> items;check_hresult(dialog->GetResults(items.put()));DWORD count{};items->GetCount(&count);std::vector<std::wstring> result;for(DWORD i=0;i<count;++i){com_ptr<IShellItem> item;check_hresult(items->GetItemAt(i,item.put()));PWSTR p{};check_hresult(item->GetDisplayName(SIGDN_FILESYSPATH,&p));result.emplace_back(p);CoTaskMemFree(p);}return result;}
std::wstring SaveFile(HWND owner,wchar_t const* defaultName){com_ptr<IFileSaveDialog> dialog;check_hresult(CoCreateInstance(CLSID_FileSaveDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(dialog.put())));DWORD options{};check_hresult(dialog->GetOptions(&options));check_hresult(dialog->SetOptions(options|FOS_FORCEFILESYSTEM));check_hresult(dialog->SetFileName(defaultName));check_hresult(dialog->SetDefaultExtension(L"json"));HRESULT hr=dialog->Show(owner);if(hr==HRESULT_FROM_WIN32(ERROR_CANCELLED))return {};check_hresult(hr);com_ptr<IShellItem> item;check_hresult(dialog->GetResult(item.put()));PWSTR p{};check_hresult(item->GetDisplayName(SIGDN_FILESYSPATH,&p));std::wstring result(p);CoTaskMemFree(p);return result;}
void Open(HWND owner,std::wstring const& path){SHELLEXECUTEINFOW e{sizeof(e)};e.hwnd=owner;e.lpFile=path.c_str();e.nShow=SW_SHOWNORMAL;e.fMask=SEE_MASK_FLAG_NO_UI;if(!ShellExecuteExW(&e))throw_last_error();}
void Reveal(HWND,std::wstring const& path){PIDLIST_ABSOLUTE pidl{};check_hresult(SHParseDisplayName(path.c_str(),nullptr,&pidl,0,nullptr));auto hr=SHOpenFolderAndSelectItems(pidl,0,nullptr,0);CoTaskMemFree(pidl);check_hresult(hr);}
std::wstring Name(std::wstring const& path){SHFILEINFOW info{};if(SHGetFileInfoW(path.c_str(),0,&info,sizeof(info),SHGFI_DISPLAYNAME))return info.szDisplayName;return std::filesystem::path(path).filename().wstring();}
void Fit(Zone& z){RECT r{z.x,z.y,z.x+z.width,z.y+z.height};MONITORINFO info{sizeof(info)};GetMonitorInfoW(MonitorFromRect(&r,MONITOR_DEFAULTTONEAREST),&info);Clamp(z,info.rcWork);}
HWND DesktopHost(){HWND result{};EnumWindows([](HWND h,LPARAM p)->BOOL{if(FindWindowExW(h,nullptr,L"SHELLDLL_DefView",nullptr)){*reinterpret_cast<HWND*>(p)=h;return FALSE;}return TRUE;},reinterpret_cast<LPARAM>(&result));return result;}
bool Attach(HWND window,HWND host){if(!IsWindow(host))return false;RECT r{};GetWindowRect(window,&r);auto style=GetWindowLongPtrW(window,GWL_STYLE);SetWindowLongPtrW(window,GWL_STYLE,((style&~WS_POPUP)&~WS_THICKFRAME)|WS_CHILD);SetLastError(0);auto old=SetParent(window,host);if(!old&&GetLastError()){SetWindowLongPtrW(window,GWL_STYLE,style);return false;}POINT p{r.left,r.top};ScreenToClient(host,&p);SetWindowPos(window,HWND_TOP,p.x,p.y,r.right-r.left,r.bottom-r.top,SWP_FRAMECHANGED|SWP_NOACTIVATE);return GetParent(window)==host;}
void Detach(HWND window){RECT r{};GetWindowRect(window,&r);SetParent(window,nullptr);SetWindowLongPtrW(window,GWL_STYLE,(GetWindowLongPtrW(window,GWL_STYLE)&~WS_CHILD)|WS_POPUP|WS_THICKFRAME);SetWindowPos(window,HWND_NOTOPMOST,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_FRAMECHANGED|SWP_NOACTIVATE);}
fire_and_forget LoadIcon(std::wstring path,Microsoft::UI::Xaml::Controls::Image image){
 try{
  static std::map<std::wstring,Microsoft::UI::Xaml::Media::Imaging::WriteableBitmap> cache;
  auto hit=cache.find(path);
  if(hit!=cache.end()){image.Source(hit->second);co_return;}
  auto attrs=GetFileAttributesW(path.c_str());
  if(attrs==INVALID_FILE_ATTRIBUTES)co_return;
  com_ptr<IShellItemImageFactory> factory;
  check_hresult(SHCreateItemFromParsingName(path.c_str(),nullptr,IID_PPV_ARGS(factory.put())));
  HBITMAP handle{};
  check_hresult(factory->GetImage(SIZE{64,64},SIIGBF_RESIZETOFIT|SIIGBF_BIGGERSIZEOK,&handle));
  BITMAP info{};
  GetObjectW(handle,sizeof(info),&info);
  int width=info.bmWidth,height=info.bmHeight;
  BITMAPINFOHEADER header{sizeof(header)};
  header.biWidth=width;header.biHeight=-height;header.biPlanes=1;header.biBitCount=32;header.biCompression=BI_RGB;
  std::vector<uint8_t> pixels(static_cast<size_t>(width)*height*4);
  HDC screen=GetDC(nullptr);
  GetDIBits(screen,handle,0,height,pixels.data(),reinterpret_cast<BITMAPINFO*>(&header),DIB_RGB_COLORS);
  ReleaseDC(nullptr,screen);
  DeleteObject(handle);
  bool alpha=false;
  for(size_t i=3;i<pixels.size();i+=4)if(pixels[i]){alpha=true;break;}
  if(!alpha)for(size_t i=3;i<pixels.size();i+=4)pixels[i]=255;
  Microsoft::UI::Xaml::Media::Imaging::WriteableBitmap bitmap(width,height);
  auto bytes=bitmap.PixelBuffer().as<::Windows::Storage::Streams::IBufferByteAccess>();
  uint8_t* target{};
  check_hresult(bytes->Buffer(&target));
  memcpy(target,pixels.data(),pixels.size());
  cache.insert_or_assign(path,bitmap);
  image.Source(bitmap);
 }catch(...){}
}
}
