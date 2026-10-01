#pragma once
#include "Core.h"
#include "SettingsWindow.h"
namespace guodesk {
class Controller;
struct already_running {};
class DeskWindow {
 Controller& owner; bool desktop=false,closing=false,dragging=false,capsuleNow=false;
 std::wstring expandedStack;
 POINT dragStart{}; RECT dragOrigin{};
 winrt::Microsoft::UI::Xaml::Controls::Grid root{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::StackPanel tabsPanel{nullptr}; winrt::Microsoft::UI::Xaml::Controls::Grid barGrid{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBox title{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBlock status{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::GridView grid{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ScrollViewer listHost{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::StackPanel listPanel{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::Button chevron{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::Border pill{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBlock pillName{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::Image pillIcon{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::FontIcon pillGlyph{nullptr};
 winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer embedTimer{nullptr};
 winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer hoverTimer{nullptr};
 static LRESULT CALLBACK Subclass(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
 std::vector<RECT> Peers(); void SnapResize(RECT&,int); void DragUpdate();
 winrt::fire_and_forget Drop(winrt::Microsoft::UI::Xaml::DragEventArgs args,size_t position,std::wstring stackId={});
 void Menu(winrt::Microsoft::UI::Xaml::FrameworkElement const& target);
 void EntryMenu(std::wstring const& path,std::wstring const& key,std::wstring const& stackId);
 void RebuildTabs(); void SwitchTab(int index);
 void SetCapsule(bool on); void ExpandCapsule(); void ShrinkCapsule();
 void ApplyPerformance();
public:
 std::wstring id; std::wstring viewId;
 Zone& View();
 winrt::Microsoft::UI::Xaml::Window window{nullptr}; HWND hwnd{};
 DeskWindow(Controller&,std::wstring);
 ~DeskWindow();
 Zone& Model();
 void Refresh(); void Place(); void Show(); void SetDesktop(bool enabled); void Capture();
 void ApplySettings();
 void Notify(std::wstring const& text);
 void Pick(bool folder=false);
 winrt::Microsoft::UI::Xaml::Media::Brush ThemeBrush(wchar_t const* key,winrt::Windows::UI::Color fallback);
};
class SettingsWindow;
class Controller {
 HWND messageWindow{}; NOTIFYICONDATAW tray{}; UINT taskbarCreated{}; HANDLE mutex{}; bool quitting=false; std::map<std::wstring,std::wstring> mappedStamp; std::shared_ptr<bool> syncAlive{std::make_shared<bool>(true)};
 static LRESULT CALLBACK MessageProc(HWND,UINT,WPARAM,LPARAM);
 void AddTray();
public:
 Store store; Layout layout; std::vector<std::unique_ptr<DeskWindow>> windows; bool desktopMode=false; HWND host{};
 explicit Controller(std::filesystem::path root={}); ~Controller();
 void Start(); void Save(); void ImportLayout(Layout&& next); void Add(); void Remove(std::wstring const& id); void Refresh(); void Show(); void HideAll(); void ToggleAll(); bool ApplyHotkey(); void Quit(); void ToggleDesktop();
 void ShowSettings(); void CloseSettings(); void ApplySettings();
 void ShowTidy(); void CloseTidy();
 void ShowNote(); void CloseNote(); void ShowTodo(); void CloseTodo();
 void ShowClock(); void CloseClock(); void ShowGuide(); void CloseGuide(); void CheckReminders();
 void ShowMusic(); void CloseMusic(); void ShowSearch(); void CloseSearch(); void ShowWeather(); void CloseWeather();
 void SyncUploadAuto();
 void MoveEntry(std::wstring const& entry,std::wstring const& target,size_t index);
 void SyncWindows(); void MergeInto(std::wstring const& self,std::wstring const& other); void Ungroup(std::wstring const& zoneId); void AddToGroup(std::wstring const& anchorId);
 std::unique_ptr<class SettingsWindow> settings; std::unique_ptr<class TidyWindow> tidy;
 std::unique_ptr<class NoteWindow> note; std::unique_ptr<class TodoWindow> todo;
 std::unique_ptr<class ClockWindow> clockW; std::unique_ptr<class GuideWindow> guide;
 std::unique_ptr<class MusicWindow> music; std::unique_ptr<class SearchWindow> search; std::unique_ptr<class WeatherWindow> weather;
};
}
