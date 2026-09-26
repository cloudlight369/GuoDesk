#pragma once
#include "App.xaml.g.h"
#include "DeskWindow.h"
namespace winrt::GuoDesk::implementation {
struct App : AppT<App> {
 App();
 void OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const&);
 std::unique_ptr<guodesk::Controller> controller;
};
}
