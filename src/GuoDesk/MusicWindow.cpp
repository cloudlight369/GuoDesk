#include "pch.h"
#include "MusicWindow.h"
#include "DeskWindow.h"
#include "Shell.h"
#include "I18n.h"
using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;
namespace guodesk {
static std::wstring MusicExePath(){wchar_t buf[MAX_PATH]{};GetModuleFileNameW(nullptr,buf,MAX_PATH);return buf;}
static Brush ResolveMusicBrush(wchar_t const* key,Windows::UI::Color fallback,bool dark){
 try{return Application::Current().Resources().Lookup(box_value(key)).as<Brush>();}catch(...){}
 struct Entry{wchar_t const* key;Windows::UI::Color light;Windows::UI::Color dark;};
 static const Entry palette[]{{L"TextFillColorSecondary",{255,97,97,97},{255,199,199,199}}};
 for(auto const& e:palette)if(!wcscmp(e.key,key))return SolidColorBrush(dark?e.dark:e.light);
 return SolidColorBrush(fallback);
}
Brush MusicWindow::ThemeBrush(wchar_t const* key,Windows::UI::Color fallback){bool dark=false;try{dark=root.ActualTheme()==ElementTheme::Dark;}catch(...){}return ResolveMusicBrush(key,fallback,dark);}
static std::wstring TrimExt(std::wstring name){auto p=std::filesystem::path(name);auto stem=p.stem().wstring();return stem.empty()?name:stem;}
static std::wstring FileUri(std::wstring const& path){
 std::string utf8=winrt::to_string(path);
 std::wstring out=L"file:///";
 for(unsigned char c:utf8){
  if((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~')out+=static_cast<wchar_t>(c);
  else if(c=='/')out+=L'/';
  else{wchar_t b[4]{};swprintf_s(b,4,L"%%%02X",c);out+=b;}
 }
 return out;
}
static std::wstring TimeText(std::chrono::seconds s){
 long long t=s.count();if(t<0)t=0;
 wchar_t buf[16]{};swprintf_s(buf,16,L"%lld:%02lld",t/60,t%60);return buf;
}
void MusicWindow::ScanFolder(){
 auto& w=owner.layout.widgets;
 playlist=MusicPlaylist(w.musicFolder);
 if(current>=static_cast<int>(playlist.size()))current=playlist.empty()?-1:0;
}
void MusicWindow::UpdateTrackUi(){
 if(current<0||playlist.empty()){
  track.Text(i18n::Tr(L"未选择文件夹"));
  sub.Text(i18n::Tr(L"点击右上角文件夹图标选择音乐目录"));
  empty.Visibility(Visibility::Visible);
  count.Text(L"");
 }else{
  auto name=TrimExt(std::filesystem::path(playlist[current]).filename().wstring());
  track.Text(name);
  sub.Text(i18n::TrF(L"{0} 首 · 第 {1} 首",{std::to_wstring(playlist.size()),std::to_wstring(current+1)}));
  empty.Visibility(Visibility::Collapsed);
  count.Text(std::to_wstring(playlist.size()));
 }
}
void MusicWindow::RebuildList(){
 listPanel.Children().Clear();
 for(size_t i=0;i<playlist.size();++i){
  auto index=static_cast<int>(i);
  Button row;row.HorizontalAlignment(HorizontalAlignment::Stretch);row.HorizontalContentAlignment(HorizontalAlignment::Stretch);row.Padding(Thickness{10,6,10,6});row.BorderThickness(Thickness{0});row.Background(SolidColorBrush(Windows::UI::Colors::Transparent()));
  Grid g;ColumnDefinition c0;c0.Width(GridLength{0,GridUnitType::Auto});g.ColumnDefinitions().Append(c0);ColumnDefinition c1;c1.Width(GridLength{1,GridUnitType::Star});g.ColumnDefinitions().Append(c1);
  FontIcon mark;mark.FontFamily(FontFamily(L"Segoe Fluent Icons"));mark.FontSize(ScaledFont(owner.layout.settings.textSize,11));mark.VerticalAlignment(VerticalAlignment::Center);mark.Margin(Thickness{0,0,8,0});
  if(index==current){mark.Glyph(L"\uE768");mark.Foreground(SolidColorBrush(Windows::UI::Color{255,0,120,212}));}
  else{mark.Glyph(L"");mark.Margin(Thickness{19,0,0,0});}
  Grid::SetColumn(mark,0);g.Children().Append(mark);
  TextBlock name;name.Text(TrimExt(std::filesystem::path(playlist[i]).filename().wstring()));name.FontSize(ScaledFont(owner.layout.settings.textSize,12));name.TextTrimming(TextTrimming::CharacterEllipsis);
  if(index==current)name.Foreground(SolidColorBrush(Windows::UI::Color{255,0,120,212}));
  Grid::SetColumn(name,1);g.Children().Append(name);
  row.Content(g);
  row.Click([this,index](auto&&,auto&&){PlayIndex(index,true);});
  listPanel.Children().Append(row);
 }
}
void MusicWindow::PlayIndex(int index,bool autoplay){
 ScanFolder();
 auto& w=owner.layout.widgets;
 if(playlist.empty()){current=-1;w.musicIndex=0;try{player.Source(nullptr);}catch(...){}UpdateTrackUi();RebuildList();UpdateProgress();owner.Save();return;}
 if(index<0)index=0;if(index>=static_cast<int>(playlist.size()))index=static_cast<int>(playlist.size())-1;
 current=index;w.musicIndex=current;
 try{player.Source(Windows::Media::Core::MediaSource::CreateFromUri(Windows::Foundation::Uri(FileUri(playlist[current]))));if(autoplay)player.Play();else player.Pause();}catch(...){}
 UpdateTrackUi();RebuildList();UpdateProgress();SyncPlayGlyph();
 saveTimer.Start();
}
void MusicWindow::UpdateProgress(){
 if(current<0){progress.Value(0);timeText.Text(L"0:00 / 0:00");return;}
 std::chrono::seconds pos{0},dur{0};
 try{auto p=player.Position();pos=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::duration<int64_t,std::ratio<1,10000000>>(p.count()));}catch(...){}
 try{auto d=player.NaturalDuration();if(d.count()>0)dur=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::duration<int64_t,std::ratio<1,10000000>>(d.count()));}catch(...){}
 if(dur.count()>0){progress.Maximum(1);progress.Value(static_cast<double>(pos.count())/static_cast<double>(dur.count()));}
 else{progress.Maximum(1);progress.Value(0);}
 timeText.Text(TimeText(pos)+L" / "+TimeText(dur));
}
void MusicWindow::SyncPlayGlyph(){
 bool playing=false;
 try{playing=player.CurrentState()==Windows::Media::Playback::MediaPlayerState::Playing;}catch(...){}
 playGlyph.Glyph(playing?L"\uE769":L"\uE768");
}
void MusicWindow::SaveGeometry(){
 if(!IsWindow(hwnd))return;
 RECT r{};GetWindowRect(hwnd,&r);
 auto& w=owner.layout.widgets;
 if(w.musicX==r.left&&w.musicY==r.top&&w.musicW==r.right-r.left&&w.musicH==r.bottom-r.top)return;
 w.musicX=r.left;w.musicY=r.top;w.musicW=r.right-r.left;w.musicH=r.bottom-r.top;
 MONITORINFOEXW mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&mi);
 w.musicMon=mi.szDevice;w.musicMX=r.left-mi.rcWork.left;w.musicMY=r.top-mi.rcWork.top;
 owner.Save();
}
MusicWindow::MusicWindow(Controller& c):owner(c){
 window=Window();window.Title(i18n::Tr(L"GuoDesk 音乐"));hwnd=shell::Handle(window);
 try{auto dir=std::filesystem::path(MusicExePath()).parent_path();window.AppWindow().SetIcon((dir/L"guodesk.ico").wstring());}catch(...){}
 window.SystemBackdrop(MicaBackdrop());
 try{auto presenter=window.AppWindow().Presenter().as<Microsoft::UI::Windowing::OverlappedPresenter>();presenter.SetBorderAndTitleBar(true,false);window.AppWindow().IsShownInSwitchers(false);}catch(...){}
 auto& w=owner.layout.widgets;
 root=Grid();
 GridLength rows[]={GridLength{0,GridUnitType::Auto},GridLength{0,GridUnitType::Auto},GridLength{0,GridUnitType::Auto},GridLength{0,GridUnitType::Auto},GridLength{1,GridUnitType::Star}};
 for(auto& r:rows){RowDefinition rd;rd.Height(r);root.RowDefinitions().Append(rd);}
 Border header;header.Padding(Thickness{12,8,8,6});header.Background(SolidColorBrush(Windows::UI::Colors::Transparent()));
 Grid headGrid;ColumnDefinition hc1;hc1.Width(GridLength{1,GridUnitType::Star});headGrid.ColumnDefinitions().Append(hc1);ColumnDefinition hc2;hc2.Width(GridLength{0,GridUnitType::Auto});headGrid.ColumnDefinitions().Append(hc2);
 StackPanel headRow;headRow.Orientation(Orientation::Horizontal);headRow.Spacing(8);
 FontIcon grip;grip.FontFamily(FontFamily(L"Segoe Fluent Icons"));grip.Glyph(L"\uE7C2");grip.FontSize(ScaledFont(owner.layout.settings.textSize,12));grip.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,120,120,120}));headRow.Children().Append(grip);
 TextBlock title;title.Text(i18n::Tr(L"音乐"));title.FontSize(ScaledFont(owner.layout.settings.textSize,12));title.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,120,120,120}));headRow.Children().Append(title);
 count=TextBlock();count.FontSize(ScaledFont(owner.layout.settings.textSize,11));count.VerticalAlignment(VerticalAlignment::Center);count.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,140,140,140}));headRow.Children().Append(count);
 Grid::SetColumn(headRow,0);headGrid.Children().Append(headRow);
 auto glyphBtn=[this](wchar_t const* glyph,int size){Button b;b.Background(SolidColorBrush(Windows::UI::Colors::Transparent()));b.BorderThickness(Thickness{0});b.Padding(Thickness{6,2,6,2});b.MinWidth(0);FontIcon ic;ic.FontFamily(FontFamily(L"Segoe Fluent Icons"));ic.Glyph(glyph);ic.FontSize(ScaledFont(owner.layout.settings.textSize,size));ic.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,140,140,140}));b.Content(ic);return b;};
 auto folderBtn=glyphBtn(L"\uE8B7",13);
 folderBtn.Click([this](auto&&,auto&&){
  auto picked=shell::Pick(hwnd,true,i18n::Tr(L"选择音乐文件夹"));
  if(picked.empty())return;
  owner.layout.widgets.musicFolder=picked.front();current=0;
  PlayIndex(owner.layout.widgets.musicIndex,true);
 });
 Grid::SetColumn(folderBtn,1);headGrid.Children().Append(folderBtn);
 header.Child(headGrid);root.Children().Append(header);
 auto dragHeader=[&](FrameworkElement const& el){
  auto dragging=std::make_shared<bool>(false);auto dragStart=std::make_shared<POINT>();auto dragOrigin=std::make_shared<RECT>();
  dragTimer=root.DispatcherQueue().CreateTimer();dragTimer.Interval(std::chrono::milliseconds(16));
  dragTimer.Tick([this,dragging,dragStart,dragOrigin](auto&&,auto&&){
   if(!*dragging)return;
   if(!(GetAsyncKeyState(VK_LBUTTON)&0x8000)){*dragging=false;this->dragTimer.Stop();SaveGeometry();return;}
   POINT p{};GetCursorPos(&p);
   SetWindowPos(hwnd,nullptr,dragOrigin->left+p.x-dragStart->x,dragOrigin->top+p.y-dragStart->y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
  });
  el.PointerPressed([this,dragging,dragStart,dragOrigin,el](auto&&,Input::PointerRoutedEventArgs const& a){*dragging=true;GetCursorPos(&*dragStart);GetWindowRect(hwnd,&*dragOrigin);el.CapturePointer(a.Pointer());a.Handled(true);dragTimer.Start();});
  el.PointerReleased([this,dragging](auto&&,auto&&){if(*dragging){*dragging=false;dragTimer.Stop();SaveGeometry();}});
  el.PointerCaptureLost([this,dragging](auto&&,auto&&){if(*dragging){*dragging=false;dragTimer.Stop();SaveGeometry();}});
 };
 dragHeader(header);
 StackPanel now;now.Margin(Thickness{14,2,14,0});now.Spacing(2);
 track=TextBlock();track.FontSize(ScaledFont(owner.layout.settings.textSize,15));track.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());track.TextTrimming(TextTrimming::CharacterEllipsis);
 sub=TextBlock();sub.FontSize(ScaledFont(owner.layout.settings.textSize,11));sub.TextWrapping(TextWrapping::Wrap);sub.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,120,120,120}));
 now.Children().Append(track);now.Children().Append(sub);
 Grid::SetRow(now,1);root.Children().Append(now);
 StackPanel progRow;progRow.Margin(Thickness{14,8,14,0});progRow.Spacing(4);
 progress=ProgressBar();progress.Minimum(0);progress.Maximum(1);
 timeText=TextBlock();timeText.FontSize(ScaledFont(owner.layout.settings.textSize,11));timeText.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,140,140,140}));
 progRow.Children().Append(progress);progRow.Children().Append(timeText);
 Grid::SetRow(progRow,2);root.Children().Append(progRow);
 StackPanel controls;controls.Orientation(Orientation::Horizontal);controls.Spacing(6);controls.Margin(Thickness{14,6,14,6});controls.HorizontalAlignment(HorizontalAlignment::Center);
 auto prevBtn=glyphBtn(L"\uE892",14);prevBtn.Click([this](auto&&,auto&&){if(current>0)PlayIndex(current-1,true);});
 auto playBtn=glyphBtn(L"\uE768",16);playGlyph=playBtn.Content().as<FontIcon>();
 playBtn.Click([this](auto&&,auto&&){
  if(current<0||playlist.empty()){if(!owner.layout.widgets.musicFolder.empty())PlayIndex(0,true);return;}
  try{if(player.CurrentState()==Windows::Media::Playback::MediaPlayerState::Playing)player.Pause();else player.Play();}catch(...){}
 });
 auto nextBtn=glyphBtn(L"\uE893",14);nextBtn.Click([this](auto&&,auto&&){if(current+1<static_cast<int>(playlist.size()))PlayIndex(current+1,true);else if(!playlist.empty())PlayIndex(0,true);});
 auto volIcon=FontIcon();volIcon.FontFamily(FontFamily(L"Segoe Fluent Icons"));volIcon.Glyph(L"\uE767");volIcon.FontSize(ScaledFont(owner.layout.settings.textSize,12));volIcon.VerticalAlignment(VerticalAlignment::Center);volIcon.Margin(Thickness{10,0,2,0});volIcon.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,140,140,140}));
 volume=Slider();volume.Minimum(0);volume.Maximum(100);volume.StepFrequency(1);volume.Width(90);volume.VerticalAlignment(VerticalAlignment::Center);volume.Value(w.musicVol);
 volume.ValueChanged([this](auto&&,auto&&){try{player.Volume(volume.Value()/100.0);}catch(...){}if(saveTimer)saveTimer.Start();});
 controls.Children().Append(prevBtn);controls.Children().Append(playBtn);controls.Children().Append(nextBtn);controls.Children().Append(volIcon);controls.Children().Append(volume);
 Grid::SetRow(controls,3);root.Children().Append(controls);
 StackPanel listArea;listArea.Margin(Thickness{6,0,6,8});
 empty=TextBlock();empty.Text(i18n::Tr(L"还没有歌曲，选择一个音乐文件夹开始播放。"));empty.FontSize(ScaledFont(owner.layout.settings.textSize,12));empty.Margin(Thickness{8,14,8,10});empty.TextWrapping(TextWrapping::Wrap);empty.Foreground(ThemeBrush(L"TextFillColorSecondary",Windows::UI::Color{255,120,120,120}));
 listPanel=StackPanel();
 listHost=ScrollViewer();listHost.Content(listPanel);listHost.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);listHost.Padding(Thickness{4,0,4,0});
 listArea.Children().Append(empty);listArea.Children().Append(listHost);
 Grid::SetRow(listArea,4);root.Children().Append(listArea);
 window.Content(root);
 window.Closed([this](auto&&,auto&&){
  if(closing)return;closing=true;
  SaveGeometry();
  try{player.Pause();}catch(...){}
  window.DispatcherQueue().TryEnqueue([this]{owner.CloseMusic();});
 });
 try{
  auto weakAlive=std::weak_ptr<bool>(alive);auto dq=window.DispatcherQueue();
  player.MediaEnded([this,weakAlive,dq](auto&&,auto&&){if(weakAlive.lock())dq.TryEnqueue([this,weakAlive]{if(!weakAlive.lock()||closing)return;if(current+1<static_cast<int>(playlist.size()))PlayIndex(current+1,true);else if(!playlist.empty())PlayIndex(0,true);});});
  player.MediaFailed([this,weakAlive,dq](auto&&,auto&&){if(weakAlive.lock())dq.TryEnqueue([this,weakAlive]{if(!weakAlive.lock()||closing)return;sub.Text(i18n::Tr(L"无法播放此文件，试试下一首。"));});});
 }catch(...){}
 try{player.Volume(w.musicVol/100.0);}catch(...){}
 int x=w.musicX,y=w.musicY,wd=w.musicW,ht=w.musicH;SetWindowPos(hwnd,nullptr,x,y,wd,ht,SWP_NOZORDER);
 MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&mi);
 if(y>mi.rcWork.bottom-40||y<mi.rcWork.top-20||x>mi.rcWork.right-60||x<mi.rcWork.left-40){x=std::clamp(x,(int)mi.rcWork.left,(int)mi.rcWork.right-100);y=std::clamp(y,(int)mi.rcWork.top,(int)mi.rcWork.bottom-60);SetWindowPos(hwnd,nullptr,x,y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);}
 ScanFolder();
 if(!playlist.empty()){current=std::clamp(w.musicIndex,0,static_cast<int>(playlist.size())-1);}
 UpdateTrackUi();RebuildList();UpdateProgress();SyncPlayGlyph();
 saveTimer=root.DispatcherQueue().CreateTimer();saveTimer.Interval(std::chrono::milliseconds(600));saveTimer.Tick([this](auto&&,auto&&){saveTimer.Stop();owner.Save();});
 tick=root.DispatcherQueue().CreateTimer();tick.Interval(std::chrono::seconds(1));tick.Tick([this](auto&&,auto&&){UpdateProgress();SyncPlayGlyph();});tick.Start();
 window.Activate();
}
void MusicWindow::Show(){window.Activate();}
MusicWindow::~MusicWindow(){
 closing=true;*alive=false;
 if(tick)tick.Stop();if(dragTimer)dragTimer.Stop();if(saveTimer)saveTimer.Stop();
 try{player.Pause();player.Source(nullptr);}catch(...){}
 try{window.Closed(nullptr);}catch(...){}
 if(IsWindow(hwnd))window.Close();
}
}
