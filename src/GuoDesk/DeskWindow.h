#pragma once
#include "Core.h"
#include "SettingsWindow.h"
namespace guodesk {
class Controller;
struct already_running {};
class DeskWindow {
 Controller& owner; std::wstring id; bool desktop=false,closing=false,dragging=false;
 POINT dragStart{}; RECT dragOrigin{};
 winrt::Microsoft::UI::Xaml::Controls::Grid root{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBox title{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBlock status{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::GridView grid{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ScrollViewer listHost{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::StackPanel listPanel{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::Button chevron{nullptr};
 winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer embedTimer{nullptr};
 static LRESULT CALLBACK Subclass(HWND,UINT,WPARAM,LPARAM,UINT_PTR,DWORD_PTR);
 std::vector<RECT> Peers(); void SnapResize(RECT&,int); void DragUpdate();
 winrt::fire_and_forget Drop(winrt::Microsoft::UI::Xaml::DragEventArgs args,size_t position);
 void Menu(winrt::Microsoft::UI::Xaml::FrameworkElement const& target);
public:
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
 HWND messageWindow{}; NOTIFYICONDATAW tray{}; UINT taskbarCreated{}; HANDLE mutex{}; bool quitting=false; std::map<std::wstring,std::wstring> mappedStamp;
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
 void MoveEntry(std::wstring const& entry,std::wstring const& target,size_t index);
 std::unique_ptr<class SettingsWindow> settings; std::unique_ptr<class TidyWindow> tidy;
 std::unique_ptr<class NoteWindow> note; std::unique_ptr<class TodoWindow> todo;
 std::unique_ptr<class ClockWindow> clockW; std::unique_ptr<class GuideWindow> guide;
};
}
