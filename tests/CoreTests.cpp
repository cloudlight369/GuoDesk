#include "pch.h"
#include "Core.h"
#include <iostream>
namespace guodesk {
void RunTests(std::filesystem::path const& output){std::ofstream report(output);int passed=0;auto expect=[&](bool ok,char const* name){report<<(ok?"PASS ":"FAIL ")<<name<<'\n';report.flush();if(!ok)throw std::runtime_error(name);++passed;};
 Zone z;z.id=NewId();z.name=L"中文 分区";expect(AddEntry(z,L"C:\\测试 文件\\Demo.lnk"),"add unicode path");expect(!AddEntry(z,L"c:\\测试 文件\\DEMO.lnk"),"case insensitive duplicate");expect(!AddEntry(z,L"C:\\测试 文件\\sub\\..\\Demo.lnk"),"normalized duplicate");
 Layout l{{z}};l.settings.theme=L"Dark";l.settings.compact=true;auto round=Deserialize(Serialize(l));expect(round.zones[0].name==z.name && round.zones[0].entries[0].path==z.entries[0].path,"JSON unicode roundtrip");expect(round.settings.theme==L"Dark"&&round.settings.compact,"settings persisted");auto legacy=Deserialize("{\"version\":1,\"zones\":[]}");expect(legacy.settings.theme.empty()&&!legacy.settings.compact,"missing settings tolerated");expect(Deserialize(Serialize(Layout{})).zones.empty(),"empty layout stays empty");
 bool invalid=false;try{Deserialize("{\"version\":99,\"zones\":[]}");}catch(...){invalid=true;}expect(invalid,"unsupported version rejected");
 z.x=-5000;z.y=9000;z.width=440;z.height=360;Clamp(z,RECT{0,0,1920,1080});expect(z.x==0 && z.y==720,"offscreen position corrected");z.width=800;z.height=600;Clamp(z,RECT{0,0,200,120});expect(z.width==200&&z.height==120,"small work area bounded");Clamp(z,RECT{-1920,0,0,1080});expect(z.x<0,"negative monitor coordinates");
 auto temp=std::filesystem::temp_directory_path()/NewId();Store store(temp);std::wstring warning;auto initial=store.Load(warning);expect(initial.zones.size()==1,"first run default zone");store.Save(l);auto newer=l;newer.zones[0].name=L"new";store.Save(newer);expect(store.Load(warning).zones[0].name==L"new","atomic save latest");{std::ofstream corrupt(temp/L"layout.json");corrupt<<"broken";}auto recovered=store.Load(warning);expect(recovered.zones[0].name==z.name,"backup recovery");bool preserved=false;for(auto const& f:std::filesystem::directory_iterator(temp))if(f.path().filename().wstring().starts_with(L"layout.corrupt."))preserved=true;expect(preserved,"corrupt original preserved");std::filesystem::remove_all(temp);report<<"TOTAL "<<passed<<" passed\n";
}
}
