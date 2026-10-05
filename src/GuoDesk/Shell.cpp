#include "pch.h"
#include "Shell.h"
#include "I18n.h"
#include <robuffer.h>
#include <shlwapi.h>
#include <ole2.h>
#include <oleidl.h>
using namespace winrt;
namespace guodesk::shell {
HWND Handle(Microsoft::UI::Xaml::Window const& w){HWND h{};check_hresult(w.as<IWindowNative>()->get_WindowHandle(&h));return h;}
std::vector<std::wstring> Pick(HWND owner,bool folder,std::wstring const& title){com_ptr<IFileOpenDialog> dialog;check_hresult(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(dialog.put())));DWORD options{};check_hresult(dialog->GetOptions(&options));check_hresult(dialog->SetOptions(options|FOS_ALLOWMULTISELECT|FOS_FORCEFILESYSTEM|(folder?FOS_PICKFOLDERS:0)));dialog->SetTitle(!title.empty()?title.c_str():folder?i18n::Tr(L"添加文件夹入口").c_str():i18n::Tr(L"添加文件或应用入口").c_str());HRESULT hr=dialog->Show(owner);if(hr==HRESULT_FROM_WIN32(ERROR_CANCELLED))return {};check_hresult(hr);com_ptr<IShellItemArray> items;check_hresult(dialog->GetResults(items.put()));DWORD count{};items->GetCount(&count);std::vector<std::wstring> result;for(DWORD i=0;i<count;++i){com_ptr<IShellItem> item;check_hresult(items->GetItemAt(i,item.put()));PWSTR p{};check_hresult(item->GetDisplayName(SIGDN_FILESYSPATH,&p));result.emplace_back(p);CoTaskMemFree(p);}return result;}
std::wstring SaveFile(HWND owner,wchar_t const* defaultName){com_ptr<IFileSaveDialog> dialog;check_hresult(CoCreateInstance(CLSID_FileSaveDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(dialog.put())));DWORD options{};check_hresult(dialog->GetOptions(&options));check_hresult(dialog->SetOptions(options|FOS_FORCEFILESYSTEM));check_hresult(dialog->SetFileName(defaultName));check_hresult(dialog->SetDefaultExtension(L"json"));HRESULT hr=dialog->Show(owner);if(hr==HRESULT_FROM_WIN32(ERROR_CANCELLED))return {};check_hresult(hr);com_ptr<IShellItem> item;check_hresult(dialog->GetResult(item.put()));PWSTR p{};check_hresult(item->GetDisplayName(SIGDN_FILESYSPATH,&p));std::wstring result(p);CoTaskMemFree(p);return result;}
void Open(HWND owner,std::wstring const& path){SHELLEXECUTEINFOW e{sizeof(e)};e.hwnd=owner;e.lpFile=path.c_str();e.nShow=SW_SHOWNORMAL;e.fMask=SEE_MASK_FLAG_NO_UI;if(!ShellExecuteExW(&e))throw_last_error();}
void Reveal(HWND,std::wstring const& path){PIDLIST_ABSOLUTE pidl{};check_hresult(SHParseDisplayName(path.c_str(),nullptr,&pidl,0,nullptr));auto hr=SHOpenFolderAndSelectItems(pidl,0,nullptr,0);CoTaskMemFree(pidl);check_hresult(hr);}
int EntryContextMenu(HWND hwnd,std::wstring const& path,std::vector<std::wstring> const& custom){
 int result=-1;
 PIDLIST_ABSOLUTE pidl{};
 if(FAILED(SHParseDisplayName(path.c_str(),nullptr,&pidl,0,nullptr))||!pidl)return result;
 auto parent=ILClone(pidl);
 if(parent)ILRemoveLastID(parent);
 PCUITEMID_CHILD child=ILFindLastID(pidl);
 com_ptr<IShellFolder> folder;
 com_ptr<IContextMenu> context;
 if(parent&&SUCCEEDED(SHBindToObject(nullptr,parent,nullptr,IID_PPV_ARGS(folder.put())))&&SUCCEEDED(folder->GetUIObjectOf(hwnd,1,&child,IID_IContextMenu,nullptr,context.put_void()))){
  if(HMENU menu=CreatePopupMenu()){
   if(SUCCEEDED(context->QueryContextMenu(menu,0,1,0x6FFF,GetKeyState(VK_SHIFT)<0?CMF_EXTENDEDVERBS:CMF_NORMAL))){
    if(!custom.empty()){AppendMenuW(menu,MF_SEPARATOR,0,nullptr);for(size_t i=0;i<custom.size();++i)AppendMenuW(menu,MF_STRING,static_cast<UINT_PTR>(0x7000+i),custom[i].c_str());}
    POINT p{};GetCursorPos(&p);
    SetForegroundWindow(hwnd);
    int cmd=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY|TPM_RIGHTBUTTON,p.x,p.y,0,hwnd,nullptr);
    if(cmd>=0x7000)result=cmd-0x7000;
    else if(cmd>0){CMINVOKECOMMANDINFO info{sizeof(info)};info.hwnd=hwnd;info.lpVerb=MAKEINTRESOURCEA(cmd);info.nShow=SW_SHOWNORMAL;context->InvokeCommand(&info);}
   }
   DestroyMenu(menu);
  }
 }
 if(parent)ILFree(parent);
 ILFree(pidl);
 return result;
}
std::wstring Name(std::wstring const& path){SHFILEINFOW info{};if(SHGetFileInfoW(path.c_str(),0,&info,sizeof(info),SHGFI_DISPLAYNAME))return info.szDisplayName;return std::filesystem::path(path).filename().wstring();}
std::vector<AppShortcut> EnumerateApps(){
 std::vector<AppShortcut> out;std::set<std::wstring> seen;
 auto known=[](KNOWNFOLDERID const& id){PWSTR p{};std::wstring r;if(SUCCEEDED(SHGetKnownFolderPath(id,0,nullptr,&p))){r=p;CoTaskMemFree(p);}return r;};
 auto scan=[&](std::filesystem::path const& root){
  std::error_code ec;std::filesystem::recursive_directory_iterator it(root,std::filesystem::directory_options::skip_permission_denied,ec);
  if(ec)return;
  for(auto const& e:it){
   try{
    if(out.size()>=512)break;
    std::error_code fe;if(!e.is_regular_file(fe)||fe)continue;
    auto lext=e.path().extension().wstring();CharLowerBuffW(lext.data(),static_cast<DWORD>(lext.size()));if(lext!=L".lnk")continue;
    std::wstring lk=e.path().wstring();CharLowerBuffW(lk.data(),static_cast<DWORD>(lk.size()));if(!seen.insert(lk).second)continue;
    com_ptr<IShellLinkW> link;if(FAILED(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(link.put()))))continue;
    com_ptr<IPersistFile> pf;if(FAILED(link->QueryInterface(IID_PPV_ARGS(pf.put())))||FAILED(pf->Load(e.path().c_str(),0)))continue;
    WCHAR target[1024]{};WIN32_FIND_DATAW fd{};if(FAILED(link->GetPath(target,static_cast<DWORD>(sizeof(target)/sizeof(WCHAR)),&fd,SLGP_SHORTPATH))||target[0]==0)continue;
    WCHAR args[512]{};link->GetArguments(args,static_cast<DWORD>(sizeof(args)/sizeof(WCHAR)));
    AppShortcut a;a.name=e.path().stem().wstring();a.linkPath=e.path().wstring();a.target=target;a.arguments=args;
    if(ShouldListApp(a.name,a.target))out.push_back(std::move(a));
   }catch(...){}
  }
 };
 auto user=known(FOLDERID_StartMenu);if(!user.empty())scan(std::filesystem::path(user)/L"Programs");
 auto common=known(FOLDERID_CommonStartMenu);if(!common.empty())scan(std::filesystem::path(common)/L"Programs");
 SortAppsByName(out);
 return out;
}
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
  if(cache.size()>2048)cache.clear();
  cache.insert_or_assign(path,bitmap);
  image.Source(bitmap);
 }catch(...){}
}
std::vector<char> MakeHdrop(std::vector<std::wstring> const& paths){
 std::vector<char> blob(sizeof(DROPFILES),0);
 auto* df=reinterpret_cast<DROPFILES*>(blob.data());
 df->pFiles=static_cast<DWORD>(sizeof(DROPFILES));df->fWide=TRUE;
 for(auto const& path:paths){for(wchar_t c:path){auto* b=reinterpret_cast<char*>(&c);blob.insert(blob.end(),b,b+sizeof(wchar_t));}blob.insert(blob.end(),2,0);}
 blob.insert(blob.end(),2,0);
 return blob;
}
struct DragSource final : IDataObject,IDropSource {
 LONG refs{1};std::vector<char> blob;
 explicit DragSource(std::vector<char>&& b):blob(std::move(b)){}
 STDMETHOD(QueryInterface)(REFIID riid,void** out)override{if(riid==IID_IUnknown||riid==IID_IDataObject)*out=static_cast<IDataObject*>(this);else if(riid==IID_IDropSource)*out=static_cast<IDropSource*>(this);else{*out=nullptr;return E_NOINTERFACE;}AddRef();return S_OK;}
 STDMETHOD_(ULONG,AddRef)()override{return InterlockedIncrement(&refs);}
 STDMETHOD_(ULONG,Release)()override{auto r=InterlockedDecrement(&refs);if(!r)delete this;return r;}
 STDMETHOD(QueryContinueDrag)(BOOL escape,DWORD keyState)override{if(escape)return DRAGDROP_S_CANCEL;if(!(keyState&MK_LBUTTON))return DRAGDROP_S_DROP;return S_OK;}
 STDMETHOD(GiveFeedback)(DWORD){return DRAGDROP_S_USEDEFAULTCURSORS;}
 STDMETHOD(GetData)(FORMATETC* f,STGMEDIUM* m)override{if(f->cfFormat!=CF_HDROP||f->tymed!=TYMED_HGLOBAL||f->dwAspect!=DVASPECT_CONTENT)return DV_E_FORMATETC;HGLOBAL g=GlobalAlloc(GMEM_MOVEABLE,blob.size());if(!g)return E_OUTOFMEMORY;CopyMemory(GlobalLock(g),blob.data(),blob.size());GlobalUnlock(g);m->tymed=TYMED_HGLOBAL;m->hGlobal=g;m->pUnkForRelease=nullptr;return S_OK;}
 STDMETHOD(GetDataHere)(FORMATETC* f,STGMEDIUM* m)override{return GetData(f,m);}
 STDMETHOD(QueryGetData)(FORMATETC* f)override{return f->cfFormat==CF_HDROP&&f->tymed==TYMED_HGLOBAL&&f->dwAspect==DVASPECT_CONTENT?S_OK:DV_E_FORMATETC;}
 STDMETHOD(GetCanonicalFormatEtc)(FORMATETC*,FORMATETC* out)override{out->ptd=nullptr;return DATA_S_SAMEFORMATETC;}
 STDMETHOD(SetData)(FORMATETC*,STGMEDIUM*,BOOL)override{return E_NOTIMPL;}
 STDMETHOD(EnumFormatEtc)(DWORD dir,IEnumFORMATETC** e)override{if(dir!=DATADIR_GET)return E_NOTIMPL;FORMATETC fe{CF_HDROP,nullptr,DVASPECT_CONTENT,-1,TYMED_HGLOBAL};return SHCreateStdEnumFmtEtc(1,&fe,e);}
 STDMETHOD(DAdvise)(FORMATETC*,DWORD,IAdviseSink*,DWORD*)override{return E_NOTIMPL;}
 STDMETHOD(DUnadvise)(DWORD)override{return E_NOTIMPL;}
 STDMETHOD(EnumDAdvise)(IEnumSTATDATA**)override{return E_NOTIMPL;}
};
HRESULT DragOut(HWND hwnd,std::vector<std::wstring> const& paths){
 static bool active=false;
 if(active||paths.empty())return S_FALSE;
 active=true;
 auto blob=MakeHdrop(paths);
 auto* source=new DragSource(std::move(blob));
 HRESULT init=OleInitialize(nullptr);
 if(hwnd)SetCapture(hwnd);
 DWORD effect=DROPEFFECT_LINK;
 HRESULT r=DoDragDrop(source,source,DROPEFFECT_LINK|DROPEFFECT_COPY,&effect);
 if(hwnd)ReleaseCapture();
 if(init==S_OK)OleUninitialize();
 source->Release();
 active=false;
 return r;
}
static std::wstring TrimTail(std::wstring v){while(!v.empty()&&v.back()==L'\\')v.pop_back();return v;}
static std::wstring Zipped(std::vector<std::wstring> const& paths){std::wstring s;for(auto const& p:paths){s+=p;s.push_back(L'\0');}s.push_back(L'\0');return s;}
static bool SamePath(std::wstring const& a,std::wstring const& b){return CompareStringOrdinal(a.c_str(),-1,b.c_str(),-1,TRUE)==CSTR_EQUAL;}
static std::vector<std::wstring> ExistingNames(std::wstring const& dir){
 std::vector<std::wstring> out;
 WIN32_FIND_DATAW fd{};
 HANDLE h=FindFirstFileW((TrimTail(dir)+L"\\*").c_str(),&fd);
 if(h==INVALID_HANDLE_VALUE)return out;
 do{std::wstring n=fd.cFileName;if(n!=L"."&&n!=L"..")out.push_back(std::move(n));}while(FindNextFileW(h,&fd));
 FindClose(h);
 return out;
}
static int RunOp(SHFILEOPSTRUCTW& op){
 op.hwnd=nullptr;
 op.fFlags|=FOF_SILENT|FOF_NOCONFIRMATION|FOF_NOERRORUI|FOF_MULTIDESTFILES;
 auto r=SHFileOperationW(&op);
 if(r==ERROR_CANCELLED)return 0;
 if(r!=0||op.fAnyOperationsAborted)return -1;
 return 1;
}
static int Run(SHFILEOPSTRUCTW& op){
 auto r=RunOp(op);
 if(r==0)throw std::runtime_error("operation cancelled");
 if(r<0)throw std::runtime_error("file operation failed");
 return 1;
}
// 一次剪贴板会话同时读取文件列表与首选拖放效果：分两次打开会让剪切在两次读取之间被其它程序清掉
// 0=取到内容 1=剪贴板没有文件 2=剪贴板正被其它程序占用
static int ReadClip(std::vector<std::wstring>& out,bool& move){
 out.clear();move=false;
 if(!IsClipboardFormatAvailable(CF_HDROP))return 1;
 bool opened=false;
 for(int i=0;i<10;++i){if(OpenClipboard(nullptr)){opened=true;break;}Sleep(40);}
 if(!opened)return 2;
 if(HANDLE h=GetClipboardData(CF_HDROP)){
  auto* df=reinterpret_cast<HDROP>(GlobalLock(h));
  if(df){
   for(UINT i=0,n=DragQueryFileW(df,0xffffffff,nullptr,0);i<n;++i){
    UINT const len=DragQueryFileW(df,i,nullptr,0);
    if(!len)continue;
    std::wstring p;
    try{p.resize(static_cast<size_t>(len)+1);}catch(...){continue;}
    auto wrote=DragQueryFileW(df,i,p.data(),static_cast<UINT>(p.size()));
    if(!wrote)continue;
    p.resize(wrote);
    out.push_back(std::move(p));
   }
   GlobalUnlock(h);
  }
 }
 if(!out.empty()){
  if(HANDLE e=GetClipboardData(static_cast<UINT>(RegisterClipboardFormatW(L"Preferred DropEffect")))){
   auto* p=reinterpret_cast<DWORD*>(GlobalLock(e));
   if(p){move=(*p&DROPEFFECT_MOVE)!=0&&(*p&DROPEFFECT_COPY)==0;GlobalUnlock(e);}
  }
 }
 CloseClipboard();
 return out.empty()?1:0;
}
ClipFiles ReadClipFiles(){
 ClipFiles c;bool move=false;
 auto r=ReadClip(c.paths,move);
 c.move=move;c.busy=r==2;
 return c;
}
bool HasClipFiles(){return IsClipboardFormatAvailable(CF_HDROP)!=FALSE;}
static std::wstring VolumeRoot(std::wstring const& path){
 auto root=std::filesystem::path(path).root_name().wstring();
 if(root.empty())return {};
 if(root.back()!=L'\\'&&root.back()!=L'/')root+=L'\\';
 return root;
}
bool RecycleCapable(std::wstring const& path){
 auto root=VolumeRoot(path);
 if(root.empty())return false;
 switch(GetDriveTypeW(root.c_str())){
  case DRIVE_FIXED:case DRIVE_REMOVABLE:break;
  default:return false;
 }
 auto exists=[&](wchar_t const* name){WIN32_FIND_DATAW fd{};HANDLE h=FindFirstFileW((root+name).c_str(),&fd);if(h==INVALID_HANDLE_VALUE)return false;FindClose(h);return true;};
 return exists(L"$Recycle.Bin")||exists(L"Recycler");
}
// 逐路径判定会在失效盘/网络盘上反复阻塞 UI（GetDriveTypeW 可挂起数秒），故按卷根去重后再计数
long long CountNonRecyclable(std::vector<std::wstring> const& paths){
 std::vector<std::pair<std::wstring,bool>> cache;long long n=0;
 auto find=[&](std::wstring const& root)->int{for(size_t i=0;i<cache.size();++i)if(cache[i].first==root)return static_cast<int>(i);return -1;};
 for(auto const& p:paths){
  auto root=VolumeRoot(p);
  if(root.empty()){++n;continue;}
  int at=find(root);
  if(at<0){cache.emplace_back(root,RecycleCapable(p));at=static_cast<int>(cache.size()-1);}
  if(!cache[at].second)++n;
 }
 return n;
}
void ClipboardCopy(std::vector<std::wstring> const& paths,bool cut){
 if(paths.empty())return;
 auto blob=MakeHdrop(paths);
 // 其它进程（资源管理器、剪贴板工具、脚本）可能短暂占用剪贴板，重试比直接失败更贴近用户预期
 bool opened=false;
 for(int i=0;i<10;++i){if(OpenClipboard(nullptr)){opened=true;break;}Sleep(40);}
 if(!opened)throw_last_error();
 if(!EmptyClipboard()){CloseClipboard();throw_last_error();}
 HGLOBAL drop=GlobalAlloc(GMEM_MOVEABLE,blob.size());
 void* dst=drop?GlobalLock(drop):nullptr;
 if(!drop||!dst){if(drop)GlobalFree(drop);CloseClipboard();throw std::runtime_error("clipboard out of memory");}
 CopyMemory(dst,blob.data(),blob.size());GlobalUnlock(drop);
 if(!SetClipboardData(CF_HDROP,drop)){GlobalFree(drop);CloseClipboard();throw_last_error();}
 HGLOBAL effect=GlobalAlloc(GMEM_MOVEABLE,sizeof(DWORD));
 if(effect){
  auto* p=reinterpret_cast<DWORD*>(GlobalLock(effect));
  if(p){*p=cut?DROPEFFECT_MOVE:(DROPEFFECT_COPY|DROPEFFECT_MOVE);GlobalUnlock(effect);SetClipboardData(static_cast<UINT>(RegisterClipboardFormatW(L"Preferred DropEffect")),effect);}
  else GlobalFree(effect);
 }
 CloseClipboard();
}
static bool InsideRoot(std::wstring const& root,std::wstring const& path){
 auto a=TrimTail(PathKey(root)),b=TrimTail(PathKey(path));
 if(a.size()>=b.size())return false;
 if(b.compare(0,a.size(),a)!=0)return false;
 return b[a.size()]==L'\\';
}
TransferResult TransferFiles(std::vector<std::wstring> const& sources,std::wstring const& destDir,bool move,CancelFlag const& cancel,ProgressFn const& progress){
 TransferResult r;
 auto dest=TrimTail(std::filesystem::path(destDir).wstring());
 if(dest.empty()){r.failed=static_cast<long long>(sources.size());return r;}
 auto taken=ExistingNames(dest);
 long long const total=static_cast<long long>(sources.size());
 long long done=0;
 if(progress)progress(0,total,std::wstring());
 for(auto const& src:sources){
  if(cancel&&cancel->load()){r.cancelled=true;break;}
  bool keepGoing=true;
  try{
   std::filesystem::path p(src);
   auto name=p.filename().wstring();
   if(name.empty())++r.failed;
   // 同目录粘贴、把文件夹放进它自己的子目录：跳过而不是自我嵌套副本
   else if(SamePath(src,dest+L"\\"+name)||SamePath(TrimTail(p.parent_path().wstring()),dest)||InsideRoot(dest,src))++r.skipped;
   else{
    std::error_code ec;bool dir=std::filesystem::is_directory(p,ec);
    auto unique=UniqueName(taken,dir?name:p.stem().wstring(),dir?std::wstring():p.extension().wstring());
    auto target=dest+L"\\"+unique;
    auto from=Zipped({src}),to=Zipped({target});
    SHFILEOPSTRUCTW op{};op.wFunc=move?FO_MOVE:FO_COPY;op.pFrom=from.c_str();op.pTo=to.c_str();
    auto res=RunOp(op);
    if(res<0)++r.failed;
    else if(res==0){r.cancelled=true;keepGoing=false;}
    else{taken.push_back(unique);r.made.push_back(target);}
   }
  }catch(...){++r.failed;}
  ++done;
  if(progress)progress(done,total,Name(src));
  if(!keepGoing)break;
 }
 if(progress&&done>=total)progress(total,total,std::wstring());
 return r;
}
std::wstring CreateFolder(std::wstring const& dir,std::wstring const& baseName){
 auto dest=TrimTail(std::filesystem::path(dir).wstring());
 if(dest.empty())throw std::runtime_error("no target folder");
 auto name=UniqueName(ExistingNames(dest),baseName.empty()?std::wstring(L"新建文件夹"):baseName);
 auto target=dest+L"\\"+name;
 if(!CreateDirectoryW(target.c_str(),nullptr)&&GetLastError()!=ERROR_ALREADY_EXISTS)throw std::runtime_error("cannot create folder");
 return target;
}
void RenamePath(std::wstring const& path,std::wstring const& newName){
 std::wstring name=newName;
 while(!name.empty()&&name.front()==L' ')name.erase(name.begin());
 while(!name.empty()&&name.back()==L' ')name.pop_back();
 if(name.empty()||name.back()==L'.'||name.find_first_of(L"\\/:*?\"<>|")!=std::wstring::npos)throw std::runtime_error("invalid name");
 if(IsReservedDeviceName(name))throw std::runtime_error("reserved device name");
 std::filesystem::path p(path);
 auto target=TrimTail(p.parent_path().wstring())+L"\\"+name;
 if(SamePath(path,target))return;
 if(GetFileAttributesW(target.c_str())!=INVALID_FILE_ATTRIBUTES)throw std::runtime_error("name already exists");
 auto from=Zipped({path}),to=Zipped({target});
 SHFILEOPSTRUCTW op{};op.wFunc=FO_RENAME;op.pFrom=from.c_str();op.pTo=to.c_str();
 Run(op);
 if(GetFileAttributesW(target.c_str())==INVALID_FILE_ATTRIBUTES)throw std::runtime_error("rename did not apply");
}
DeleteResult DeleteFiles(std::vector<std::wstring> const& paths,bool permanent,CancelFlag const& cancel,ProgressFn const& progress){
 DeleteResult r;
 long long const total=static_cast<long long>(paths.size());
 long long done=0;
 if(progress)progress(0,total,std::wstring());
 for(auto const& src:paths){
  if(cancel&&cancel->load()){r.cancelled=true;break;}
  bool keepGoing=true;
  // 请求回收站但所在卷不支持时，SHFileOperation 会在无提示下永久删除，必须先判定并如实告知
  bool undo=!permanent&&RecycleCapable(src);
  if(!permanent&&!undo)++r.nuked;
  auto from=Zipped({src});
  SHFILEOPSTRUCTW op{};
  op.wFunc=FO_DELETE;op.pFrom=from.c_str();
  if(undo)op.fFlags|=FOF_ALLOWUNDO;
  auto res=RunOp(op);
  if(res<0)++r.failed;
  else if(res==0){r.cancelled=true;keepGoing=false;}
  // 只有确认从磁盘消失才算删掉：仍存在的文件保留在分区里，避免"看起来丢了"的假象
  else if(GetFileAttributesW(src.c_str())==INVALID_FILE_ATTRIBUTES)r.gone.push_back(src);
  else ++r.failed;
  ++done;
  if(progress)progress(done,total,Name(src));
  if(!keepGoing)break;
 }
 if(progress&&done>=total)progress(total,total,std::wstring());
 return r;
}
}
