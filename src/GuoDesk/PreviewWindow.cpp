#include "pch.h"
#include <shlwapi.h>
#include <limits>
#include "PreviewWindow.h"
#include "DeskWindow.h"
#include "Shell.h"
#include "I18n.h"
using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
using namespace Microsoft::UI::Xaml::Media::Imaging;
namespace guodesk {
static size_t const kPreviewReadBytes=512*1024;
static size_t const kPreviewMaxChars=20000;
static size_t const kPreviewFolderRows=300;
static Brush ResolvePreviewBrush(wchar_t const* key,Windows::UI::Color fallback,bool dark){
 try{return Application::Current().Resources().Lookup(box_value(key)).as<Brush>();}catch(...){}
 struct Entry{wchar_t const* key;Windows::UI::Color light;Windows::UI::Color dark;};
 static const Entry palette[]{
  {L"TextFillColorPrimary",{255,28,28,28},{255,236,236,236}},
  {L"TextFillColorSecondary",{255,97,97,97},{255,199,199,199}},
  {L"CardBackgroundFillColorDefault",{255,248,248,248},{255,43,43,43}},
 };
 for(auto const& e:palette)if(!wcscmp(e.key,key))return SolidColorBrush(dark?e.dark:e.light);
 return SolidColorBrush(fallback);
}
Brush PreviewWindow::ThemeBrush(wchar_t const* key,Windows::UI::Color fallback){bool dark=false;try{dark=root.ActualTheme()==ElementTheme::Dark;}catch(...){}return ResolvePreviewBrush(key,fallback,dark);}
static std::wstring PreviewStamp(std::wstring const& path){
 WIN32_FILE_ATTRIBUTE_DATA d{};if(!GetFileAttributesExW(path.c_str(),GetFileExInfoStandard,&d))return L"";
 SYSTEMTIME st{};FILETIME local{};
 if(!FileTimeToLocalFileTime(&d.ftLastWriteTime,&local)||!FileTimeToSystemTime(&local,&st))return L"";
 wchar_t buf[40]{};swprintf_s(buf,40,L"%04d-%02d-%02d %02d:%02d",st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute);return buf;
}
static long long PreviewBytes(std::wstring const& path){WIN32_FILE_ATTRIBUTE_DATA d{};if(!GetFileAttributesExW(path.c_str(),GetFileExInfoStandard,&d))return -1;return (static_cast<long long>(d.nFileSizeHigh)<<32)|static_cast<long long>(d.nFileSizeLow);}
static std::vector<char> PreviewReadHead(std::wstring const& path){
 std::ifstream f(std::filesystem::path(path),std::ios::binary);if(!f)return {};
 std::vector<char> buf(kPreviewReadBytes);
 auto n=static_cast<size_t>(f.read(buf.data(),static_cast<std::streamsize>(kPreviewReadBytes)).gcount());
 buf.resize(n);return buf;
}
static BitmapImage PreviewPictureImage(std::wstring const& path){
 try{
  wchar_t url[1024]{};DWORD c=1024;
  if(FAILED(UrlCreateFromPathW(path.c_str(),url,&c,0)))return nullptr;
  BitmapImage img;img.UriSource(winrt::Windows::Foundation::Uri(url));
  return img;
 }catch(...){return nullptr;}
}
static size_t const kPreviewFolderScan=2000;
static void PreviewFolderBody(std::wstring const& folder,std::wstring& body,long long& count){
 count=0;std::vector<std::wstring> names;std::error_code ec;
 std::filesystem::directory_iterator it(std::filesystem::path(folder),std::filesystem::directory_options::skip_permission_denied,ec);
 if(ec){body=i18n::Tr(L"读取失败：文件夹不可访问。");return;}
 bool scanned=false;
 // 显式 increment(ec)：枚举中途遇到权限或目录被删时不会抛异常打断界面线程
 for(;it!=std::filesystem::directory_iterator();it.increment(ec)){
  if(ec){ec.clear();break;}
  ++count;
  if(count>static_cast<long long>(kPreviewFolderScan)){scanned=true;break;}
  if(names.size()>=kPreviewFolderRows)continue;
  std::error_code de;auto line=it->path().filename().wstring();
  if(line.empty())continue;
  if(it->is_directory(de))line+=L'\\';
  names.push_back(std::move(line));
 }
 std::sort(names.begin(),names.end(),[](std::wstring const& a,std::wstring const& b){bool da=a.back()==L'\\',db=b.back()==L'\\';if(da!=db)return da;return a<b;});
 for(auto const& n:names){body+=n;body+=L'\n';}
 if(body.empty()){body=count?i18n::Tr(L"读取失败：文件夹不可访问。"):i18n::Tr(L"文件夹为空");}
 else if(count>static_cast<long long>(names.size())){body+=L'\n';body+=i18n::TrF(L"（仅显示前 {0} 项）",{std::to_wstring(names.size())});}
 if(scanned){body+=L'\n';body+=i18n::TrF(L"（内容过多，只扫描了前 {0} 项）",{std::to_wstring(kPreviewFolderScan)});}
}
void PreviewWindow::Render(){
 if(closing||!IsWindow(hwnd)||index>=paths.size())return;
 auto const& path=paths[index];
 std::error_code dec;bool dir=false;
 try{dir=std::filesystem::is_directory(std::filesystem::path(path),dec);}catch(...){}
 std::wstring kind=i18n::Tr(L"文件"),body;
 long long count=0,bytes=dir?-1:PreviewBytes(path);
 bool showPicture=false;BitmapImage source{nullptr};
 if(dir){kind=i18n::Tr(L"文件夹");PreviewFolderBody(path,body,count);}
 else if(PreviewKind(path)==1){
  kind=i18n::Tr(L"图片文件");
  source=PreviewPictureImage(path);
  showPicture=static_cast<bool>(source);
  if(!showPicture)body=i18n::Tr(L"无法预览此图片，按 Enter 用默认程序打开。");
 }else if(PreviewKind(path)==2){
  kind=i18n::Tr(L"文本文件");
  auto raw=PreviewReadHead(path);
  if(raw.empty())body=bytes==0?i18n::Tr(L"空文件"):i18n::Tr(L"读取失败：文件可能被占用。");
  else{
   auto text=DecodeNeutralText(raw);
   if(text.empty())body=i18n::Tr(L"无法预览此文件，按 Enter 用默认程序打开。");
   else{
    auto cut=ClampPreviewText(text,kPreviewMaxChars);
    if(cut.size()<text.size()){body=cut;body+=L'\n';body+=i18n::TrF(L"（内容过长，仅显示前 {0} 字）",{std::to_wstring(kPreviewMaxChars)});}
    else body=std::move(text);
   }
  }
 }else body=i18n::Tr(L"无法预览此文件，按 Enter 用默认程序打开。");
 std::wstring line=kind;
 if(dir){line+=L" · ";line+=i18n::TrF(L"共 {0} 项",{std::to_wstring(count)});}
 else if(bytes>=0){line+=L" · ";line+=PreviewSizeText(bytes);}
 auto stamp=PreviewStamp(path);
 if(!stamp.empty()){line+=L" · ";line+=stamp;}
 line+=L" · ";line+=std::to_wstring(index+1);line+=L"/";line+=std::to_wstring(paths.size());
 head.Text(shell::Name(path));
 meta.Text(line);
 picture.Source(showPicture?source:BitmapImage{nullptr});
 bodyText.Text(body);
 bodyText.Visibility(body.empty()?Visibility::Collapsed:Visibility::Visible);
 picture.Visibility(showPicture?Visibility::Visible:Visibility::Collapsed);
 hint.Text(i18n::Tr(L"空格或 Esc 关闭 · Enter 打开 · ↑↓ 切换条目"));
 try{if(showPicture)picture.StartBringIntoView();else if(!body.empty())bodyText.StartBringIntoView();}catch(...){}
}
void PreviewWindow::Step(int delta){
 if(closing||!IsWindow(hwnd)||paths.empty())return;
 int n=NavStep(static_cast<int>(index),static_cast<int>(paths.size()),delta);
 if(n<0)return;
 index=static_cast<size_t>(n);
 Render();
}
void PreviewWindow::RequestClose(){
 auto guard=alive;auto* self=this;
 try{window.DispatcherQueue().TryEnqueue([guard,self]{if(*guard)self->owner.ClosePreview();});}catch(...){}
}
void PreviewWindow::OnKey(Input::KeyRoutedEventArgs const& a){
 auto key=a.Key();
 if(key==Windows::System::VirtualKey::Escape||key==Windows::System::VirtualKey::Space||key==Windows::System::VirtualKey::Back){a.Handled(true);RequestClose();return;}
 if(key==Windows::System::VirtualKey::Down||key==Windows::System::VirtualKey::Right){a.Handled(true);Step(1);return;}
 if(key==Windows::System::VirtualKey::Up||key==Windows::System::VirtualKey::Left){a.Handled(true);Step(-1);return;}
 if(key==Windows::System::VirtualKey::PageUp||key==Windows::System::VirtualKey::PageDown){
  a.Handled(true);
  try{
   float const nan=std::numeric_limits<float>::quiet_NaN();
   double const page=scroller.ViewportHeight()*0.8;
   double next=scroller.VerticalOffset()+(key==Windows::System::VirtualKey::PageUp?-page:page);
   scroller.ChangeView(nan,static_cast<float>(std::max(0.0,next)),nan);
  }catch(...){}
  return;
 }
 if(key==Windows::System::VirtualKey::Enter){a.Handled(true);if(index<paths.size())try{shell::Open(hwnd,paths[index]);}catch(...){}return;}
}
PreviewWindow::PreviewWindow(Controller& c):owner(c){
 window=Window();window.Title(i18n::Tr(L"GuoDesk 快速预览"));hwnd=shell::Handle(window);
 window.SystemBackdrop(MakeBackdrop(owner.layout.settings.backdrop));
 try{auto presenter=window.AppWindow().Presenter().as<Microsoft::UI::Windowing::OverlappedPresenter>();presenter.SetBorderAndTitleBar(true,false);window.AppWindow().IsShownInSwitchers(false);}catch(...){}
 root=Grid();root.Padding(Thickness{0,0,0,0});
 {
  GridLength rows[]{GridLength{0,GridUnitType::Auto},GridLength{0,GridUnitType::Auto},GridLength{1,GridUnitType::Star},GridLength{0,GridUnitType::Auto}};
  for(auto const& g:rows){RowDefinition d;d.Height(g);root.RowDefinitions().Append(d);}
 }
 head=TextBlock();head.FontSize(ScaledFont(owner.layout.settings.textSize,15));head.FontWeight(Windows::UI::Text::FontWeights::SemiBold());head.TextTrimming(TextTrimming::CharacterEllipsis);head.TextWrapping(TextWrapping::NoWrap);head.Margin(Thickness{16,14,16,2});Grid::SetRow(head,0);root.Children().Append(head);
 meta=TextBlock();meta.FontSize(ScaledFont(owner.layout.settings.textSize,11));meta.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,140,140,140}));meta.TextTrimming(TextTrimming::CharacterEllipsis);meta.TextWrapping(TextWrapping::NoWrap);meta.Margin(Thickness{16,0,16,8});Grid::SetRow(meta,1);root.Children().Append(meta);
 card=Border();card.Margin(Thickness{14,0,14,0});card.CornerRadius(CornerRadius{8,8,8,8});card.Background(ThemeBrush(L"CardBackgroundFillColorDefault",Windows::UI::Color{255,60,60,60}));
 scroller=ScrollViewer();scroller.Padding(Thickness{14,12,14,12});scroller.HorizontalScrollBarVisibility(ScrollBarVisibility::Auto);scroller.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
 auto inner=Grid();
 picture=Image();picture.Stretch(Stretch::Uniform);picture.HorizontalAlignment(HorizontalAlignment::Center);picture.VerticalAlignment(VerticalAlignment::Center);
 picture.ImageFailed([this](auto&&,ExceptionRoutedEventArgs const&){try{picture.Source(BitmapImage{nullptr});picture.Visibility(Visibility::Collapsed);bodyText.Text(i18n::Tr(L"无法预览此图片，按 Enter 用默认程序打开。"));bodyText.Visibility(Visibility::Visible);}catch(...){}});
 bodyText=TextBlock();bodyText.FontFamily(FontFamily(L"Consolas"));bodyText.FontSize(ScaledFont(owner.layout.settings.textSize,12.5));bodyText.Foreground(ThemeBrush(L"TextFillColorPrimary",Windows::UI::Color{255,24,24,24}));bodyText.TextWrapping(TextWrapping::NoWrap);bodyText.IsTextSelectionEnabled(true);
 inner.Children().Append(picture);inner.Children().Append(bodyText);
 scroller.Content(inner);
 card.Child(scroller);Grid::SetRow(card,2);root.Children().Append(card);
 hint=TextBlock();hint.FontSize(ScaledFont(owner.layout.settings.textSize,11));hint.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,140,140,140}));hint.HorizontalAlignment(HorizontalAlignment::Center);hint.Margin(Thickness{0,8,0,12});Grid::SetRow(hint,3);root.Children().Append(hint);
 root.PreviewKeyDown([this](auto&&,Input::KeyRoutedEventArgs const& a){OnKey(a);});
 window.Content(root);
 window.Closed([this](auto&&,auto&&){if(closing)return;closing=true;RequestClose();});
 POINT cur{};GetCursorPos(&cur);
 MONITORINFOEXW mi{sizeof(mi)};
 if(!GetMonitorInfoW(MonitorFromPoint(cur,MONITOR_DEFAULTTONEAREST),&mi))mi.rcWork=RECT{0,0,1920,1080};
 int const dpi=GetDpiForWindow(hwnd)?GetDpiForWindow(hwnd):96;
 int aw=mi.rcWork.right-mi.rcWork.left,ah=mi.rcWork.bottom-mi.rcWork.top;
 int capW=MulDiv(860,dpi,96),capH=MulDiv(640,dpi,96);
 int floorW=std::min(MulDiv(360,dpi,96),aw),floorH=std::min(MulDiv(260,dpi,96),ah);
 int w=std::max(floorW,std::min(capW,aw*3/5)),h=std::max(floorH,std::min(capH,ah*3/5));
 SetWindowPos(hwnd,nullptr,mi.rcWork.left+(aw-w)/2,mi.rcWork.top+(ah-h)/2,w,h,SWP_NOZORDER);
}
void PreviewWindow::Open(std::vector<std::wstring> const& list,size_t start){
 paths=list;
 index=start<list.size()?start:0;
 Render();
 SetWindowPos(hwnd,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW);
 window.Activate();
}
PreviewWindow::~PreviewWindow(){
 *alive=false;closing=true;
 try{window.Closed(nullptr);}catch(...){}
 if(IsWindow(hwnd))window.Close();
}
}
