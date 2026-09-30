#include "pch.h"
#include "Core.h"
#include <iostream>
namespace guodesk {
void RunTests(std::filesystem::path const& output){std::ofstream report(output);int passed=0;auto expect=[&](bool ok,char const* name){report<<(ok?"PASS ":"FAIL ")<<name<<'\n';report.flush();if(!ok)throw std::runtime_error(name);++passed;};
 Zone z;z.id=NewId();z.name=L"中文 分区";expect(AddEntry(z,L"C:\\测试 文件\\Demo.lnk"),"add unicode path");expect(!AddEntry(z,L"c:\\测试 文件\\DEMO.lnk"),"case insensitive duplicate");expect(!AddEntry(z,L"C:\\测试 文件\\sub\\..\\Demo.lnk"),"normalized duplicate");
 Layout l{{z}};l.settings.theme=L"Dark";l.settings.compact=true;auto round=Deserialize(Serialize(l));expect(round.zones[0].name==z.name && round.zones[0].entries[0].path==z.entries[0].path,"JSON unicode roundtrip");expect(round.settings.theme==L"Dark"&&round.settings.compact,"settings persisted");auto legacy=Deserialize("{\"version\":1,\"zones\":[]}");expect(legacy.settings.theme.empty()&&!legacy.settings.compact,"missing settings tolerated");expect(Deserialize(Serialize(Layout{})).zones.empty(),"empty layout stays empty");
 l.zones[0].mappedFolder=L"C:\\测 试 目录";l.zones[0].viewMode=L"list";l.zones[0].nameLines=1;auto rt=Deserialize(Serialize(l));expect(rt.zones[0].mappedFolder==L"C:\\测 试 目录"&&rt.zones[0].viewMode==L"list"&&rt.zones[0].nameLines==1,"mapped zone fields roundtrip");
 auto fallback=Deserialize("{\"version\":1,\"zones\":[{\"id\":\"a\",\"name\":\"n\",\"x\":0,\"y\":0,\"width\":280,\"height\":160,\"collapsed\":false,\"entries\":[],\"viewMode\":\"weird\",\"nameLines\":9}]}");expect(fallback.zones[0].viewMode==L"grid"&&fallback.zones[0].nameLines==2&&fallback.zones[0].mappedFolder.empty(),"invalid view fields fall back");
 auto dir=std::filesystem::temp_directory_path()/NewId();std::filesystem::create_directories(dir/L"子目录");std::ofstream(dir/L"b.txt");std::ofstream(dir/L"a.txt");
 Zone mz;mz.id=NewId();mz.mappedFolder=dir.wstring();SyncMapped(mz);expect(mz.entries.size()==3,"mapped sync lists children");expect(PathKey(mz.entries[0].path)==PathKey((dir/L"子目录").wstring()),"mapped sync folders first");
 std::filesystem::remove(dir/L"a.txt");SyncMapped(mz);expect(mz.entries.size()==2,"mapped sync reflects deletion");
 mz.mappedFolder=dir/L"missing";SyncMapped(mz);expect(mz.entries.size()==2,"missing folder keeps entries");
 std::filesystem::remove_all(dir);
 expect(Deserialize("{\"version\":1,\"zones\":[]}").rules.size()==3,"default rules injected");
 Rule dr;dr.id=NewId();dr.name=L"测试规则";dr.exts={L".PDF",L"doc"};dr.targetZone=z.id;Layout rl{{z}};rl.rules={dr};auto rr=Deserialize(Serialize(rl));expect(rr.rules.size()==1&&rr.rules[0].name==L"测试规则"&&rr.rules[0].targetZone==z.id,"rules roundtrip");
 auto rdir=std::filesystem::temp_directory_path()/NewId();std::filesystem::create_directories(rdir);std::ofstream(rdir/L"报告.pdf");std::ofstream(rdir/L"简历-v2.docx");std::ofstream(rdir/L"photo.png");std::ofstream(rdir/L"misc.xyz");std::ofstream(rdir/L"desktop.ini");std::filesystem::create_directories(rdir/L"文件夹");
 auto files=ListLooseFiles(rdir.wstring());expect(files.size()==4,"loose files exclude folders and ini");
 Rule docRule;docRule.id=NewId();docRule.name=L"文档";docRule.exts={L"pdf"};docRule.targetZone=z.id;
 Rule kwRule;kwRule.id=NewId();kwRule.name=L"简历";kwRule.keywords={L"简历"};kwRule.targetZone=z.id;
 std::vector<std::wstring> un;auto plan=BuildPlan({docRule,kwRule},l.zones,files,&un);expect(plan.size()==2&&un.size()==2,"plan matches ext and keyword");
 expect(plan[0].rule==L"文档"&&PathKey(plan[0].path)==PathKey((rdir/L"报告.pdf").wstring()),"extension rule hit");
 expect(plan[1].rule==L"简历","keyword rule hit");
 Rule nb=docRule;nb.id=NewId();nb.targetZone=L"";std::vector<std::wstring> un2;auto plan2=BuildPlan({nb},l.zones,files,&un2);expect(plan2.empty()&&un2.size()==files.size(),"unbound rule skipped");
 Zone az;az.id=z.id;Layout al{{az}};expect(ApplyPlan(al,plan)==2,"apply adds references");expect(al.zones[0].entries.size()==2,"applied entries correct");
 expect(std::filesystem::exists(rdir/L"报告.pdf"),"apply never moves files");
 expect(ApplyPlan(al,plan)==0,"reapply dedupes");
 std::filesystem::remove_all(rdir);
 bool invalid=false;try{Deserialize("{\"version\":99,\"zones\":[]}");}catch(...){invalid=true;}expect(invalid,"unsupported version rejected");
 z.x=-5000;z.y=9000;z.width=440;z.height=360;Clamp(z,RECT{0,0,1920,1080});expect(z.x==0 && z.y==720,"offscreen position corrected");z.width=800;z.height=600;Clamp(z,RECT{0,0,200,120});expect(z.width==200&&z.height==120,"small work area bounded");Clamp(z,RECT{-1920,0,0,1080});expect(z.x<0,"negative monitor coordinates");
 auto temp=std::filesystem::temp_directory_path()/NewId();Store store(temp);std::wstring warning;auto initial=store.Load(warning);expect(initial.zones.size()==1,"first run default zone");store.Save(l);auto newer=l;newer.zones[0].name=L"new";store.Save(newer);expect(store.Load(warning).zones[0].name==L"new","atomic save latest");{std::ofstream corrupt(temp/L"layout.json");corrupt<<"broken";}auto recovered=store.Load(warning);expect(recovered.zones[0].name==z.name,"backup recovery");bool preserved=false;for(auto const& f:std::filesystem::directory_iterator(temp))if(f.path().filename().wstring().starts_with(L"layout.corrupt."))preserved=true;expect(preserved,"corrupt original preserved");std::filesystem::remove_all(temp);report<<"TOTAL "<<passed<<" passed\n";
}
}
