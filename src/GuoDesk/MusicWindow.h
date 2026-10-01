#pragma once
#include "Core.h"
#include <winrt/Windows.Media.Playback.h>
#include <winrt/Windows.Media.Core.h>
namespace guodesk {
class Controller;
class MusicWindow {
 Controller& owner; bool closing=false;
 winrt::Microsoft::UI::Xaml::Window window{nullptr}; HWND hwnd{};
 winrt::Microsoft::UI::Xaml::Controls::Grid root{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBlock count{nullptr},track{nullptr},sub{nullptr},timeText{nullptr},empty{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ProgressBar progress{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::FontIcon playGlyph{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::StackPanel listPanel{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ScrollViewer listHost{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::Slider volume{nullptr};
 winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer tick{nullptr},dragTimer{nullptr},saveTimer{nullptr};
 std::vector<std::wstring> playlist; int current=-1;
 winrt::Windows::Media::Playback::MediaPlayer player;
 std::shared_ptr<bool> alive{std::make_shared<bool>(true)};
 winrt::Microsoft::UI::Xaml::Media::Brush ThemeBrush(wchar_t const* key,winrt::Windows::UI::Color fallback);
 void ScanFolder(); void RebuildList(); void PlayIndex(int index,bool autoplay);
 void UpdateTrackUi(); void UpdateProgress(); void SyncPlayGlyph();
 void SaveGeometry();
public:
 MusicWindow(Controller&);
 ~MusicWindow();
 void Show();
};
}
