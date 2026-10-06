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
 // 拖过一次就别再当成"单击"：OLE 结束后指针可能还停在原来那一格上，Tapped 会跟着把文件打开
 dragFired=true;
 auto guard=alive;auto* self=this;std::wstring h=path;
 try{board.DispatcherQueue().TryEnqueue([guard,self,h]{if(*guard&&IsWindow(self->hwnd))shell::DragOut(self->hwnd,{h});});}catch(...){}
}
// 拖出的距离判断挂在 root 上：光标一旦离开五十来像素的格子，格子自己就收不到 PointerMoved 了
void StackPeekWindow::PeekDragMoved(){
 if(!dragArmed)return;
 POINT cp{};GetCursorPos(&cp);long const dx=cp.x-dragStart.x,dy=cp.y-dragStart.y;
 if(dx*dx+dy*dy<=36)return;
 dragArmed=false;BeginDrag(dragPath);
}
void StackPeekWindow::OnKey(Input::KeyRoutedEventArgs const& a){
 if(a.Key()==Windows::System::VirtualKey::Escape){a.Handled(true);RequestClose();return;}
}
void StackPeekWindow::RequestClose(){
 // 全区共用这一块面板：排队中的关闭只作废它自己那一代，别把之后刚摊开的另一叠也一起带走
 auto guard=alive;auto* self=this;auto const gen=generation;
 try{window.DispatcherQueue().TryEnqueue([guard,self,gen]{if(*guard&&gen==self->generation)self->owner.ClosePeek();});}catch(...){}
}
StackPeekWindow::StackPeekWindow(Controller& c):owner(c){
 window=Window();window.Title(i18n::Tr(L"GuoDesk 叠放浮层"));hwnd=shell::Handle(window);
 window.SystemBackdrop(MakeBackdrop(owner.layout.settings.backdrop,owner.layout.settings.perfTier));
 try{auto presenter=window.AppWindow().Presenter().as<Microsoft::UI::Windowing::OverlappedPresenter>();presenter.SetBorderAndTitleBar(true,false);window.AppWindow().IsShownInSwitchers(false);}catch(...){}
 root=Grid();root.Padding(Thickness{12,10,12,10});root.Background(SolidColorBrush(Windows::UI::Colors::Transparent()));
 {
  GridLength rows[]{GridLength{0,GridUnitType::Auto},GridLength{1,GridUnitType::Star},GridLength{0,GridUnitType::Auto},GridLength{0,GridUnitType::Auto}};
  for(auto const& g:rows){RowDefinition d;d.Height(g);root.RowDefinitions().Append(d);}
 }
 head=TextBlock();head.FontSize(ScaledFont(owner.layout.settings.textSize,14));head.FontWeight(Windows::UI::Text::FontWeights::SemiBold());head.TextTrimming(TextTrimming::CharacterEllipsis);head.TextWrapping(TextWrapping::NoWrap);head.Margin(Thickness{0,0,0,8});Grid::SetRow(head,0);root.Children().Append(head);
 board=Grid();board.HorizontalAlignment(HorizontalAlignment::Center);Grid::SetRow(board,1);root.Children().Append(board);
 hint=TextBlock();hint.Text(i18n::Tr(L"单击打开 · 右键系统菜单 · 按住拖出去"));hint.FontSize(ScaledFont(owner.layout.settings.textSize,11));hint.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,140,140,140}));hint.TextWrapping(TextWrapping::Wrap);hint.TextAlignment(TextAlignment::Center);hint.HorizontalAlignment(HorizontalAlignment::Center);hint.Margin(Thickness{0,8,0,6});Grid::SetRow(hint,2);root.Children().Append(hint);
 StackPanel footer;footer.Orientation(Orientation::Horizontal);footer.Spacing(8);footer.HorizontalAlignment(HorizontalAlignment::Center);Grid::SetRow(footer,3);
 auto guard=this->alive;
 auto mk=[this,guard](std::wstring const& text,std::function<void()> run){Button b;b.Content(box_value(text));b.FontSize(ScaledFont(owner.layout.settings.textSize,12));b.Padding(Thickness{10,4,10,4});b.Click([run,guard](auto&&,auto&&){if(!*guard)return;try{run();}catch(...){}});return b;};
 footer.Children().Append(mk(i18n::Tr(L"在分区里展开"),[this]{
  // 切了标签页、或分区已经不在了，就别吹"已展开"：面板留在原地说明原因，比静默关掉诚实
  if(owner.ExpandStackInZone(zoneId,stackId)){RequestClose();return;}
  hint.Text(i18n::Tr(L"那一页已经不在了，展开没有生效；再点一次角标就能看到现在的内容。"));
 }));
 footer.Children().Append(mk(i18n::Tr(L"关闭"),[this]{RequestClose();}));
 root.Children().Append(footer);
 root.PreviewKeyDown([this,guard](auto&&,Input::KeyRoutedEventArgs const& a){if(*guard)OnKey(a);});
 root.PointerMoved([this,guard](auto&&,Input::PointerRoutedEventArgs const&){if(*guard)PeekDragMoved();});
 window.Content(root);
 window.Closed([this,guard](auto&&,auto&&){if(!*guard||closing)return;closing=true;RequestClose();});
}
// anchor 是来源分区在屏幕上的矩形：贴着它下边缘摆，放不下就翻到上边缘
void StackPeekWindow::Open(std::wstring zone,std::wstring stack,std::wstring const& title,std::vector<std::wstring> const& items,RECT const& anchor,int anchorDpi){
 zoneId=std::move(zone);stackId=std::move(stack);++generation;
 int const tier=owner.layout.settings.textSize<0?0:(owner.layout.settings.textSize>2?2:owner.layout.settings.textSize);
 int const side=StackPeekSide(static_cast<int>(items.size()));
 int const cell=52+6*tier;
 head.Text(title);
 // 每次点开都重画一遍：面板开着的时候这一叠可能被归档、文件可能被改名，留快照就会谎报"这项还在"
 board.Children().Clear();board.RowDefinitions().Clear();board.ColumnDefinitions().Clear();
 for(int k=0;k<side;++k){RowDefinition rd;rd.Height(GridLength{1,GridUnitType::Star});board.RowDefinitions().Append(rd);ColumnDefinition cd;cd.Width(GridLength{1,GridUnitType::Star});board.ColumnDefinitions().Append(cd);}
 bool const wantIcons=PerfMosaic(owner.layout.settings.perfTier);
 for(size_t n=0;n<items.size();++n){
  auto const path=items[n];
  bool const exists=GetFileAttributesW(path.c_str())!=INVALID_FILE_ATTRIBUTES;
  Border c;c.Width(cell-6);c.Height(cell-6);c.CornerRadius(CornerRadius{6,6,6,6});c.Padding(Thickness{2,2,2,2});
  c.Background(SolidColorBrush(Windows::UI::Colors::Transparent()));// 空 Background 在 XAML 里不参与命中，点击和拖拽都会掉地上
  StackPanel box;box.Spacing(1);box.HorizontalAlignment(HorizontalAlignment::Center);
  // 取缩略图是同步跑在 UI 线程上的（LoadIcon 里一个 await 都没有），省电档就跟磁贴宫格一样不画，只留一个文件图标
  if(wantIcons){Image ic;ic.Width(28);ic.Height(28);ic.HorizontalAlignment(HorizontalAlignment::Center);box.Children().Append(ic);shell::LoadIcon(path,ic);}
  else{FontIcon g;g.FontFamily(FontFamily(L"Segoe Fluent Icons"));g.Glyph(L"\uE8A5");g.FontSize(24);g.HorizontalAlignment(HorizontalAlignment::Center);box.Children().Append(g);}
  TextBlock t;t.Text((exists?L"":L"⚠ ")+shell::Name(path));t.FontSize(ScaledFont(owner.layout.settings.textSize,10));t.TextWrapping(TextWrapping::NoWrap);t.TextTrimming(TextTrimming::CharacterEllipsis);t.HorizontalAlignment(HorizontalAlignment::Center);t.MaxWidth(cell-8);t.Foreground(exists?ThemeBrush(L"TextFillColorPrimary",Windows::UI::Color{255,24,24,24}):ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,140,140,140}));box.Children().Append(t);
  c.Child(box);
  ToolTipService::SetToolTip(c,box_value(path));
  auto guard=alive;auto* self=this;
  c.Tapped([guard,self,path](auto&&,auto&&){
   if(!*guard||self->dragFired)return;
   // shell::Open 打不开时是抛异常的：从 XAML 回调里逃出去就是整个进程一起没，必须就地咽下来说明原因
   try{shell::Open(self->hwnd,path);self->RequestClose();}
   catch(...){self->hint.Text(i18n::TrF(L"打不开「{0}」，它可能已经被移走或改名。",{shell::Name(path)}));}
  });
  c.RightTapped([guard,self,path](auto&&,auto&&){if(!*guard)return;try{shell::EntryContextMenu(self->hwnd,path,{});}catch(...){}});
  c.PointerPressed([guard,self,path](auto&&,Input::PointerRoutedEventArgs const&){if(!*guard)return;POINT sp{};GetCursorPos(&sp);self->dragStart=sp;self->dragPath=path;self->dragArmed=true;self->dragFired=false;});
  c.PointerMoved([guard,self](auto&&,Input::PointerRoutedEventArgs const&){if(*guard)self->PeekDragMoved();});
  c.PointerReleased([guard,self](auto&&,auto&&){if(*guard)self->dragArmed=false;});
  c.PointerCaptureLost([guard,self](auto&&,auto&&){if(*guard)self->dragArmed=false;});
  Grid::SetRow(c,static_cast<int>(n/side));Grid::SetColumn(c,static_cast<int>(n%side));
  board.Children().Append(c);
 }
 hint.Text(i18n::Tr(L"单击打开 · 右键系统菜单 · 按住拖出去"));
 // 尺寸要按分区所在显示器的 DPI 算：这块面板自己还没显示过的时候，GetDpiForWindow 报的是
 // "创建它的那块显示器"，混 DPI 双屏下就会大出一圈或者把内容裁掉，所以由发起它的分区传进来
 int dpi=anchorDpi;
 if(dpi<=96)dpi=96;
 int const span=std::max(side*cell,236+12*tier);// 提示行和底部按钮比 2×2 的格子宽，面板不能只按格子算
 int const w=MulDiv(span+28,dpi,96),h=MulDiv(side*cell+150+18*tier,dpi,96);
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
 try{if(IsWindow(hwnd))window.Close();}catch(...){}
}
}
