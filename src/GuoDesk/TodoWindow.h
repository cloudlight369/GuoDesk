#pragma once
#include "Core.h"
namespace guodesk {
class Controller;
class TodoWindow {
 Controller& owner; bool closing=false;
 winrt::Microsoft::UI::Xaml::Window window{nullptr}; HWND hwnd{};
 winrt::Microsoft::UI::Xaml::Controls::Grid root{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::StackPanel list{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBlock count{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBox input{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::ComboBox filter{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::Button clearDone{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBox query{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::Border selBar{nullptr};
 winrt::Microsoft::UI::Xaml::Controls::TextBlock selCount{nullptr};
 std::vector<std::wstring> selected;
 int filterKind=0;
 winrt::Microsoft::UI::Xaml::Media::Brush ThemeBrush(wchar_t const* key,winrt::Windows::UI::Color fallback);
 void Rebuild();
 std::wstring CountText();
 void Add();
public:
 TodoWindow(Controller&);
 ~TodoWindow();
 void Show();
 void RefreshList(){Rebuild();}
};
}
