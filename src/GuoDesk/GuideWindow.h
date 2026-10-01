#pragma once
#include "Core.h"
namespace guodesk {
class Controller;
class GuideWindow {
 Controller& owner; bool closing=false;
 winrt::Microsoft::UI::Xaml::Window window{nullptr}; HWND hwnd{};
public:
 GuideWindow(Controller&);
 ~GuideWindow();
 void Show();
};
}
