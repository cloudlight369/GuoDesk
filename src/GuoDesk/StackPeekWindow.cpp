#include "pch.h"
#include "StackPeekWindow.h"
#include "DeskWindow.h"
#include "Shell.h"
#include "I18n.h"
using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
using namespace Windows::Foundation;
namespace guodesk {
static Brush ResolvePeekBrush(wchar_t const* key,Windows::UI::Color fallback,bool dark){
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
Brush StackPeekWindow::ThemeBrush(wchar_t const* key,Windows::UI::Color fallback){bool dark=false;try{dark=root.ActualTheme()==ElementTheme::Dark;}catch(...){}return ResolvePeekBrush(key,fallback,dark);}
void StackPeekWindow::BeginDrag(std::wstring const& path){
 auto guard=alive;auto* self=this;std::wstring h=path;
 try{board.DispatcherQueue().TryEnqueue([guard,self,h]{if(*guard&&IsWindow(self->hwnd))shell::DragOut(self->hwnd,{h});});}catch(...){}
}
void StackPeekWindow::OnKey(Input::KeyRoutedEventArgs const& a){
 if(a.Key()==Windows::System::VirtualKey::Escape){a.Handled(true);RequestClose();return;}
}
void StackPeekWindow::RequestClose(){
 auto guard=alive;auto* self=this;
 try{window.DispatcherQueue().TryEnqueue([guard,self]{if(*guard)self->owner.ClosePeek();});}catch(...){}
}
StackPeekWindow::StackPeekWindow(Controller& c):owner(c){
 window=Window();window.Title(i18n::Tr(L"GuoDesk 叠放浮层"));hwnd=shell::Handle(window);
 window.SystemBackdrop(MakeBackdrop(owner.layout.settings.backdrop,owner.layout.settings.perfTier));
 try{auto presenter=window.AppWindow().Presenter().as<Microsoft::UI::Windowing::OverlappedPresenter>();presenter.SetBorderAndTitleBar(true,false);window.AppWindow().IsShownInSwitchers(false);}catch(...){}
 root=Grid();root.Padding(Thickness{12,10,12,10});
 {
  GridLength rows[]{GridLength{0,GridUnitType::Auto},GridLength{1,GridUnitType::Star},GridLength{0,GridUnitType::Auto},GridLength{0,GridUnitType::Auto}};
  for(auto const& g:rows){RowDefinition d;d.Height(g);root.RowDefinitions().Append(d);}
 }
 head=TextBlock();head.FontSize(ScaledFont(owner.layout.settings.textSize,14));head.FontWeight(Windows::UI::Text::FontWeights::SemiBold());head.TextTrimming(TextTrimming::CharacterEllipsis);head.TextWrapping(TextWrapping::NoWrap);head.Margin(Thickness{0,0,0,8});Grid::SetRow(head,0);root.Children().Append(head);
 board=Grid();board.HorizontalAlignment(HorizontalAlignment::Center);Grid::SetRow(board,1);root.Children().Append(board);
 hint=TextBlock();hint.Text(i18n::Tr(L"单击打开 · 右键系统菜单 · 按住拖出去"));hint.FontSize(ScaledFont(owner.layout.settings.textSize,11));hint.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,140,140,140}));hint.HorizontalAlignment(HorizontalAlignment::Center);hint.Margin(Thickness{0,8,0,6});Grid::SetRow(hint,2);root.Children().Append(hint);
 StackPanel footer;footer.Orientation(Orientation::Horizontal);footer.Spacing(8);footer.HorizontalAlignment(HorizontalAlignment::Center);Grid::SetRow(footer,3);
 auto mk=[this](std::wstring const& text,std::function<void()> run){Button b;b.Content(box_value(text));b.FontSize(ScaledFont(owner.layout.settings.textSize,12));b.Padding(Thickness{10,4,10,4});b.Click([run](auto&&,auto&&){try{run();}catch(...){}});return b;};
 footer.Children().Append(mk(i18n::Tr(L"在分区里展开"),[this]{owner.ExpandStackInZone(zoneId,stackId);RequestClose();}));
 footer.Children().Append(mk(i18n::Tr(L"关闭"),[this]{RequestClose();}));
 root.Children().Append(footer);
 root.PreviewKeyDown([this](auto&&,Input::KeyRoutedEventArgs const& a){OnKey(a);});
 window.Content(root);
 window.Closed([this](auto&&,auto&&){if(closing)return;closing=true;RequestClose();});
}
// anchor 是来源分区在屏幕上的矩形：贴着它下边缘摆，放不下就翻到上边缘
void StackPeekWindow::Open(std::wstring zone,std::wstring stack,std::wstring const& title,std::vector<std::wstring> const& items,RECT const& anchor){
 zoneId=std::move(zone);stackId=std::move(stack);
 int const side=StackPeekSide(static_cast<int>(items.size()));
 int const cell=58;
 head.Text(title);
 board.Children().Clear();board.RowDefinitions().Clear();board.ColumnDefinitions().Clear();
 for(int k=0;k<side;++k){RowDefinition rd;rd.Height(GridLength{1,GridUnitType::Star});board.RowDefinitions().Append(rd);ColumnDefinition cd;cd.Width(GridLength{1,GridUnitType::Star});board.ColumnDefinitions().Append(cd);}
 for(size_t n=0;n<items.size();++n){
  auto const path=items[n];
  bool const exists=GetFileAttributesW(path.c_str())!=INVALID_FILE_ATTRIBUTES;
  Border c;c.Width(cell-6);c.Height(cell-6);c.CornerRadius(CornerRadius{6,6,6,6});c.Padding(Thickness{2,2,2,2});
  c.Background(SolidColorBrush(Windows::UI::Colors::Transparent()));// 空 Background 在 XAML 里不参与命中，点击和拖拽都会掉地上
  StackPanel box;box.Spacing(1);box.HorizontalAlignment(HorizontalAlignment::Center);
  Image ic;ic.Width(28);ic.Height(28);ic.HorizontalAlignment(HorizontalAlignment::Center);box.Children().Append(ic);
  TextBlock t;t.Text((exists?L"":L"⚠ ")+shell::Name(path));t.FontSize(ScaledFont(owner.layout.settings.textSize,10));t.TextWrapping(TextWrapping::NoWrap);t.TextTrimming(TextTrimming::CharacterEllipsis);t.HorizontalAlignment(HorizontalAlignment::Center);t.MaxWidth(cell-8);t.Foreground(exists?ThemeBrush(L"TextFillColorPrimary",Windows::UI::Color{255,24,24,24}):ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,140,140,140}));box.Children().Append(t);
  c.Child(box);
  ToolTipService::SetToolTip(c,box_value(path));
  auto guard=alive;auto* self=this;
  c.Tapped([guard,self,path](auto&&,auto&&){if(!*guard)return;shell::Open(self->hwnd,path);self->RequestClose();});
  c.RightTapped([guard,self,path](auto&&,auto&&){if(!*guard)return;shell::EntryContextMenu(self->hwnd,path,{});});
  c.PointerPressed([guard,self,path](auto&&,Input::PointerRoutedEventArgs const&){if(!*guard)return;POINT sp{};GetCursorPos(&sp);self->dragStart=sp;self->dragPath=path;self->dragArmed=true;});
  c.PointerMoved([guard,self](auto&&,Input::PointerRoutedEventArgs const&){if(!*guard||!self->dragArmed)return;POINT cp{};GetCursorPos(&cp);long const dx=cp.x-self->dragStart.x,dy=cp.y-self->dragStart.y;if(dx*dx+dy*dy<=36)return;self->dragArmed=false;self->BeginDrag(self->dragPath);});
  c.PointerReleased([guard,self](auto&&,auto&&){if(*guard)self->dragArmed=false;});
  c.PointerCaptureLost([guard,self](auto&&,auto&&){if(*guard)self->dragArmed=false;});
  Grid::SetRow(c,static_cast<int>(n/side));Grid::SetColumn(c,static_cast<int>(n%side));
  board.Children().Append(c);
  shell::LoadIcon(path,ic);
 }
 int const dpi=GetDpiForWindow(hwnd)?GetDpiForWindow(hwnd):96;
 // 提示行和底部两个按钮比 2×2 的格子宽，面板不能只按格子算宽
 int const span=std::max(side*cell,236);
 int const w=MulDiv(span+28,dpi,96),h=MulDiv(side*cell+150,dpi,96);
 MONITORINFOEXW mi{sizeof(mi)};
 if(!GetMonitorInfoW(MonitorFromRect(&anchor,MONITOR_DEFAULTTONEAREST),&mi))mi.rcWork=RECT{0,0,1920,1080};
 int x=anchor.left+MulDiv(10,dpi,96),y=anchor.bottom+MulDiv(6,dpi,96);
 if(y+h>mi.rcWork.bottom)y=std::max(mi.rcWork.top,anchor.top-h-MulDiv(6,dpi,96));
 if(x+w>mi.rcWork.right)x=std::max(mi.rcWork.left,mi.rcWork.right-w-MulDiv(8,dpi,96));
 SetWindowPos(hwnd,HWND_TOPMOST,x,y,w,h,SWP_SHOWWINDOW);
 window.Activate();
}
StackPeekWindow::~StackPeekWindow(){
 *alive=false;closing=true;
 try{window.Closed(nullptr);}catch(...){}
 if(IsWindow(hwnd))window.Close();
}
}
