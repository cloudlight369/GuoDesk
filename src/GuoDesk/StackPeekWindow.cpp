#include "pch.h"
#include <algorithm>
#include <winrt/Microsoft.UI.Input.h>// PointerPoint::Properties 要这个头，只 include Xaml.Input 会编成 C3779
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
// 选中集只在这块面板里存在：Ctrl 单击累加，拖出时如果这一格在选中里就整批带走
void StackPeekWindow::TogglePick(std::wstring const& path){
 auto const it=std::find(picked.begin(),picked.end(),path);
 if(it==picked.end())picked.push_back(path);else picked.erase(it);
 Repick();
}
void StackPeekWindow::Repick(){
 for(size_t i=0;i<cells.size();++i){
  bool const on=std::find(picked.begin(),picked.end(),cellPaths[i])!=picked.end();
  cells[i].Background(on?SolidColorBrush(Windows::UI::Color{48,0,120,212}):SolidColorBrush(Windows::UI::Colors::Transparent()));
  cells[i].BorderThickness(on?Thickness{1,1,1,1}:Thickness{0});
  cells[i].BorderBrush(SolidColorBrush(Windows::UI::Color{255,0,120,212}));
 }
 // 计数条一直占着位置：一出现"挤掉一行格子"的跳动，用户瞄准的那一格就会点错
 if(selInfo)selInfo.Text(picked.empty()?std::wstring(1,L' '):i18n::TrF(L"已选 {0} 项 · 拖出其中一格即全部带走",{std::to_wstring(picked.size())}));
}
void StackPeekWindow::BeginDrag(std::wstring const& path){
 std::vector<std::wstring> out;
 if(picked.size()>1&&std::find(picked.begin(),picked.end(),path)!=picked.end())for(auto const& p:picked)if(GetFileAttributesW(p.c_str())!=INVALID_FILE_ATTRIBUTES)out.push_back(p);
 else if(GetFileAttributesW(path.c_str())!=INVALID_FILE_ATTRIBUTES)out.push_back(path);
 if(out.empty())return;
 // 真拖起来了才作废"单击"：一件都没拖成时把 dragFired 立起来，会让下一次点击被自己吞掉
 dragFired=true;
 auto guard=alive;auto* self=this;
 try{board.DispatcherQueue().TryEnqueue([guard,self,out]{if(*guard&&IsWindow(self->hwnd))shell::DragOut(self->hwnd,out);});}catch(...){}
}
// 拖出的距离判断挂在 root 上：光标一旦离开五十来像素的格子，格子自己就收不到 PointerMoved 了
void StackPeekWindow::PeekDragMoved(){
 // 光标没被面板抓住：松过手的那次移动不算拖拽，否则走神一下就把上一格的文件拖了出去
 if(!dragArmed)return;
 if((GetAsyncKeyState(VK_LBUTTON)&0x8000)==0){dragArmed=false;return;}
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
 hint=TextBlock();hint.FontSize(ScaledFont(owner.layout.settings.textSize,11));hint.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,140,140,140}));hint.TextWrapping(TextWrapping::Wrap);hint.TextAlignment(TextAlignment::Center);hint.HorizontalAlignment(HorizontalAlignment::Center);hint.Margin(Thickness{0,8,0,6});Grid::SetRow(hint,2);root.Children().Append(hint);
 // 选中计数和两个按钮不同排：横向排时计数一长就把「关闭」挤出面板，鼠标点不到也读不到
 StackPanel footer;footer.Orientation(Orientation::Vertical);footer.Spacing(6);footer.HorizontalAlignment(HorizontalAlignment::Center);Grid::SetRow(footer,3);
 selInfo=TextBlock();selInfo.FontSize(ScaledFont(owner.layout.settings.textSize,11));selInfo.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,140,140,140}));selInfo.TextAlignment(TextAlignment::Center);selInfo.TextWrapping(TextWrapping::NoWrap);selInfo.TextTrimming(TextTrimming::CharacterEllipsis);
 auto guard=this->alive;
 auto mk=[this,guard](std::wstring const& text,std::function<void()> run){Button b;b.Content(box_value(text));b.FontSize(ScaledFont(owner.layout.settings.textSize,12));b.Padding(Thickness{10,4,10,4});b.Click([run,guard](auto&&,auto&&){if(!*guard)return;try{run();}catch(...){}});return b;};
 StackPanel actions;actions.Orientation(Orientation::Horizontal);actions.Spacing(8);actions.HorizontalAlignment(HorizontalAlignment::Center);
 actions.Children().Append(mk(i18n::Tr(L"在分区里展开"),[this]{
  // 切了标签页、或分区已经不在了，就别吹"已展开"：面板留在原地说明原因，比静默关掉诚实
  if(owner.ExpandStackInZone(zoneId,stackId)){RequestClose();return;}
  hint.Text(i18n::Tr(L"那一页已经不在了，展开没有生效；再点一次角标就能看到现在的内容。"));
 }));
 actions.Children().Append(mk(i18n::Tr(L"关闭"),[this]{RequestClose();}));
 footer.Children().Append(actions);
 footer.Children().Append(selInfo);
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
 int dpi=anchorDpi;if(dpi<=96)dpi=96;
 MONITORINFOEXW mi{sizeof(mi)};
 if(!GetMonitorInfoW(MonitorFromRect(&anchor,MONITOR_DEFAULTTONEAREST),&mi))mi.rcWork=RECT{0,0,1920,1080};
 // 面板比屏幕还高时「关闭」会被顶到屏幕外，所以先把格子按工作区能装下的尺寸收一遍
 int const chrome=172+18*tier,roomY=MulDiv(mi.rcWork.bottom-mi.rcWork.top,96,dpi)-16;
 int cell=std::max(34,std::min(52+6*tier,(roomY-chrome)/side));
 head.Text(title);
 // 每次点开都重画一遍：面板开着的时候这一叠可能被归档、文件可能被改名，留快照就会谎报"这项还在"
 board.Children().Clear();board.RowDefinitions().Clear();board.ColumnDefinitions().Clear();
 cells.clear();cellPaths.clear();picked.clear();
 dragArmed=false;dragFired=false;pickArmed=false;dragPath.clear();
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
   // Ctrl 那一下归 PointerReleased 管：Tapped 是手势事件，连点同一格时第二次根本不发，取消选中就会点了没反应
   if((GetAsyncKeyState(VK_CONTROL)&0x8000)!=0)return;
   // shell::Open 打不开时是抛异常的：从 XAML 回调里逃出去就是整个进程一起没，必须就地咽下来说明原因
   try{shell::Open(self->hwnd,path);self->RequestClose();}
   catch(...){self->hint.Text(i18n::TrF(L"打不开「{0}」，它可能已经被移走或改名。",{shell::Name(path)}));}
  });
  c.RightTapped([guard,self,path](auto&&,auto&&){if(!*guard)return;try{shell::EntryContextMenu(self->hwnd,path,{});}catch(...){}});
  c.PointerPressed([guard,self,path](auto&&,Input::PointerRoutedEventArgs const& a){
   if(!*guard)return;
   POINT sp{};GetCursorPos(&sp);self->dragStart=sp;self->dragPath=path;self->dragArmed=true;self->dragFired=false;
   // 右键那一下只归系统菜单：Ctrl+右键不该顺手把这一格改成选中
   self->pickArmed=a.GetCurrentPoint(self->root).Properties().IsLeftButtonPressed();
  });
  c.PointerMoved([guard,self](auto&&,Input::PointerRoutedEventArgs const&){if(*guard)self->PeekDragMoved();});
  c.PointerReleased([guard,self,path](auto&&,auto&&){
   if(!*guard)return;
   self->dragArmed=false;
   bool const was=self->pickArmed;self->pickArmed=false;
   // 真拖出去过一次就别再算一次点选：OLE 结束后指针可能还停在原来那一格上
   if(!was||self->dragFired||(GetAsyncKeyState(VK_CONTROL)&0x8000)==0)return;
   NotePointerActivity();// 开着"双击 Ctrl"热键的人，Ctrl 连点两下不该把所有窗口一起藏掉
   self->TogglePick(path);
  });
  c.PointerCaptureLost([guard,self](auto&&,auto&&){if(*guard){self->dragArmed=false;self->pickArmed=false;}});
  Grid::SetRow(c,static_cast<int>(n/side));Grid::SetColumn(c,static_cast<int>(n%side));
  board.Children().Append(c);
  cells.push_back(c);cellPaths.push_back(path);
 }
 hint.Text(i18n::Tr(L"单击打开 · 右键系统菜单 · 按住拖出去 · Ctrl 单击多选"));
 Repick();
 // 尺寸按分区所在显示器的 DPI 算：这块面板自己还没显示过的时候 GetDpiForWindow 报的是"创建它的那块显示器"，
 // 混 DPI 双屏下就会大出一圈或者把内容裁掉，所以由发起它的分区把 anchorDpi 传进来（mi/roomY 在上面已经算好）
 int const span=std::max(side*cell,236+12*tier);// 提示行和底部按钮比 2×2 的格子宽，面板不能只按格子算
 selInfo.MaxWidth(span);// 计数行限宽，长文案只会省略号，不会把按钮挤出面板
 int const roomX=MulDiv(mi.rcWork.right-mi.rcWork.left,96,dpi)-16;
 int const w=MulDiv(std::min(span+28,roomX),dpi,96),h=MulDiv(std::min(side*cell+chrome,roomY),dpi,96);
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
