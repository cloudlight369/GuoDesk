#pragma once
#include "Core.h"
namespace guodesk {
class Controller;
class NoteWindow {
 Controller& owner; bool closing=false;
 winrt::Microsoft::UI::Xaml::Window window{nullptr}; HWND hwnd{};
 winrt::Microsoft::UI::Xaml::Controls::Grid root{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBox body{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::Border paper{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBlock pager{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::Button pin{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::FontIcon pinGlyph{nullptr};
 winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer saveTimer{nullptr};
 winrt::Microsoft::UI::Xaml::Media::Brush ThemeBrush(wchar_t const* key,winrt::Windows::UI::Color fallback);
 winrt::Microsoft::UI::Xaml::Media::Brush AccentBrush();
 winrt::Microsoft::UI::Xaml::Media::Brush PaperBrush(int color);
 void ApplyPaper(); void LoadPage(); void ApplyTop();
public:
 NoteWindow(Controller&);
 ~NoteWindow();
 void Show();
};
}
