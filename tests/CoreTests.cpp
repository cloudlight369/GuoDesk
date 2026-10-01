#include "pch.h"
#include "Core.h"
#include "I18n.h"
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
 auto temp=std::filesystem::temp_directory_path()/NewId();Store store(temp);std::wstring warning;auto initial=store.Load(warning);expect(initial.zones.size()==1,"first run default zone");store.Save(l);auto newer=l;newer.zones[0].name=L"new";store.Save(newer);expect(store.Load(warning).zones[0].name==L"new","atomic save latest");{std::ofstream corrupt(temp/L"layout.json");corrupt<<"broken";}auto recovered=store.Load(warning);expect(recovered.zones[0].name==z.name,"backup recovery");bool preserved=false;for(auto const& f:std::filesystem::directory_iterator(temp))if(f.path().filename().wstring().starts_with(L"layout.corrupt."))preserved=true;expect(preserved,"corrupt original preserved");std::filesystem::remove_all(temp);
 Widgets w;AddTodo(w,L"  买牛奶  ");AddTodo(w,L"写周报");expect(w.todos.size()==2&&w.todos[0].text==L"买牛奶"&&w.todos[1].text==L"写周报","todo add trims");expect(!AddTodo(w,L"   "),"empty todo rejected");ToggleTodo(w,w.todos[1].id);expect(w.todos[1].done,"todo toggle");ToggleTodo(w,w.todos[1].id);expect(!w.todos[1].done,"todo untoggle");
 w.noteText=L"便签：中文 内容\n第二行";w.noteVisible=true;w.todoVisible=true;w.noteX=15;w.noteY=25;w.noteW=333;w.noteH=222;w.todoX=44;w.todoY=55;w.todoW=266;w.todoH=333;RemoveTodo(w,w.todos[0].id);expect(w.todos.size()==1,"todo remove");
 auto wr=Deserialize(Serialize(l));expect(wr.widgets.todos.empty()&&!wr.widgets.noteVisible,"default widgets empty");l.widgets=w;auto wr2=Deserialize(Serialize(l));expect(wr2.widgets.noteText==w.noteText&&wr2.widgets.noteVisible&&wr2.widgets.todoVisible&&wr2.widgets.todos.size()==1&&wr2.widgets.todos[0].text==L"写周报"&&wr2.widgets.noteW==333&&wr2.widgets.todoH==333,"widgets roundtrip");
 auto legacyW=Deserialize("{\"version\":1,\"zones\":[]}");expect(legacyW.widgets.todos.empty()&&!legacyW.widgets.noteVisible&&legacyW.widgets.noteW==300&&legacyW.widgets.todoW==300,"legacy missing widgets defaults");
 i18n::SetLanguage(L"zh-CN");expect(i18n::Tr(L"设置")==L"设置","tr zh passthrough");
 i18n::SetLanguage(L"en-US");expect(i18n::Tr(L"设置")==L"Settings"&&i18n::Tr(L"不存在的键")==L"不存在的键","tr en lookup with fallback");
 expect(i18n::TrF(L"应用整理（添加 {0} 个引用）",{L"3"})==L"Apply tidy (adds 3 references)","trf formats placeholder");
 expect(i18n::Fmt(L"{1}+{0}",{L"A",L"B"})==L"B+A"&&i18n::Fmt(L"a {0} b {9} c",{L"X"})==L"a X b {9} c","fmt reorder and unknown index");
 Layout langL;langL.settings.language=L"en-US";auto langR=Deserialize(Serialize(langL));expect(langR.settings.language==L"en-US","language roundtrip");
 expect(Deserialize("{\"version\":1,\"zones\":[]}").settings.language.empty(),"legacy missing language tolerated");
 Hotkey hk;expect(ParseHotkey(L"Ctrl+Alt+G",hk)&&hk.mods==(MOD_CONTROL|MOD_ALT)&&hk.vk==L'G',"parse hotkey basic");
 expect(ParseHotkey(L"Win+Z",hk)&&hk.mods==MOD_WIN&&hk.vk==L'Z',"parse hotkey win");
 expect(ParseHotkey(L"ctrl + f5",hk)&&hk.vk==VK_F5&&HotkeyToString(hk)==L"Ctrl+F5","parse hotkey case space");
 Hotkey bad;expect(!ParseHotkey(L"Ctrl+G+X",bad)&&!ParseHotkey(L"Alt+",bad)&&!ParseHotkey(L"G",bad)&&!ParseHotkey(L"G+X",bad),"parse hotkey rejects");
 expect(ParseHotkey(L"  ",bad)&&bad.mods==0&&bad.vk==0&&HotkeyToString(bad).empty(),"empty hotkey disabled");
 expect(ParseHotkey(L"Ctrl+Alt+G",hk)&&HotkeyToString(hk)==L"Ctrl+Alt+G"&&ParseHotkey(L"Win+Z",hk)&&HotkeyToString(hk)==L"Win+Z","hotkey string roundtrip");
 auto sdir=std::filesystem::temp_directory_path()/NewId();std::filesystem::create_directories(sdir/L"子目录");
 {std::ofstream f(sdir/L"a2.txt");f<<std::string(100,'x');}{std::ofstream f(sdir/L"a10.txt");f<<"xxxxx";}{std::ofstream f(sdir/L"z.doc");f<<"x";}
 auto touch=[](std::filesystem::path const& p,unsigned long long daysAgo){FILETIME now{};GetSystemTimeAsFileTime(&now);unsigned long long t=(static_cast<unsigned long long>(now.dwHighDateTime)<<32)|now.dwLowDateTime;t-=daysAgo*864000000000ULL;FILETIME ft{static_cast<DWORD>(t&0xffffffffULL),static_cast<DWORD>(t>>32)};HANDLE f=CreateFileW(p.c_str(),FILE_WRITE_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);if(f!=INVALID_HANDLE_VALUE){SetFileTime(f,nullptr,nullptr,&ft);CloseHandle(f);}};
 touch(sdir/L"z.doc",10);touch(sdir/L"a2.txt",1);touch(sdir/L"a10.txt",2);
 Zone sz;sz.id=NewId();
 AddEntry(sz,(sdir/L"bad\\none.xyz").wstring());AddEntry(sz,(sdir/L"a10.txt").wstring());AddEntry(sz,(sdir/L"z.doc").wstring());AddEntry(sz,(sdir/L"子目录").wstring());AddEntry(sz,(sdir/L"a2.txt").wstring());
 auto nm=[&](int i){return std::filesystem::path(sz.entries[i].path).filename().wstring();};
 SortEntries(sz,SortKey::Name,false);expect(nm(0)==L"子目录"&&nm(1)==L"a2.txt"&&nm(2)==L"a10.txt"&&nm(3)==L"z.doc"&&nm(4)==L"none.xyz","sort name natural dirs first bad last");
 SortEntries(sz,SortKey::Name,true);expect(nm(0)==L"子目录"&&nm(1)==L"z.doc"&&nm(2)==L"a10.txt"&&nm(3)==L"a2.txt"&&nm(4)==L"none.xyz","sort name desc keeps dirs first bad last");
 SortEntries(sz,SortKey::Size,false);expect(nm(0)==L"子目录"&&nm(1)==L"z.doc"&&nm(2)==L"a10.txt"&&nm(3)==L"a2.txt"&&nm(4)==L"none.xyz","sort size asc");
 SortEntries(sz,SortKey::Size,true);expect(nm(0)==L"子目录"&&nm(1)==L"a2.txt"&&nm(2)==L"a10.txt"&&nm(3)==L"z.doc"&&nm(4)==L"none.xyz","sort size desc bad last");
 SortEntries(sz,SortKey::Date,false);expect(nm(0)==L"子目录"&&nm(1)==L"z.doc"&&nm(2)==L"a10.txt"&&nm(3)==L"a2.txt","sort date asc");
 SortEntries(sz,SortKey::Date,true);expect(nm(1)==L"a2.txt"&&nm(2)==L"a10.txt"&&nm(3)==L"z.doc"&&nm(4)==L"none.xyz","sort date desc");
 SortEntries(sz,SortKey::Type,false);expect(nm(0)==L"子目录"&&nm(1)==L"z.doc"&&nm(2)==L"a2.txt"&&nm(3)==L"a10.txt"&&nm(4)==L"none.xyz","sort type asc");
 std::filesystem::remove_all(sdir);
 auto tdir=std::filesystem::temp_directory_path()/NewId();Store st(tdir);
 Zone qz;qz.id=NewId();qz.name=L"快照";Layout ql{{qz}};
 for(int i=0;i<25;++i){qz.name=L"快照"+std::to_wstring(i);ql.zones[0]=qz;st.Save(ql);}
 int snapCount=0;std::error_code sec;for(auto const& e:std::filesystem::directory_iterator(tdir/L"snapshots",sec))if(e.is_regular_file(sec))++snapCount;
 expect(snapCount>=1&&snapCount<=5,"snapshots throttled and capped");
 auto tdir2=std::filesystem::temp_directory_path()/NewId();Store st2(tdir2);st2.SetSnapshots(false);
 for(int i=0;i<25;++i)st2.Save(ql);
 expect(!std::filesystem::exists(tdir2/L"snapshots"),"snapshots disabled writes none");
 auto tdir3=std::filesystem::temp_directory_path()/NewId();Store st3(tdir3);
 Zone rz;rz.id=NewId();rz.name=L"快照恢复";Layout rl2{{rz}};st3.Save(rl2);
 {std::ofstream f(tdir3/L"layout.json",std::ios::binary);f<<"broken";}{std::ofstream f(tdir3/L"layout.backup.json",std::ios::binary);f<<"broken";}
 std::wstring warn3;auto rec=st3.Load(warn3);
 expect(rec.zones.size()==1&&rec.zones[0].name==L"快照恢复"&&!warn3.empty(),"load recovers from snapshot");
 std::filesystem::remove_all(tdir);std::filesystem::remove_all(tdir2);std::filesystem::remove_all(tdir3);
 l.zones[0].sortKey=L"date";l.zones[0].sortDescending=true;l.zones[0].tileSize=2;l.settings.hotkey=L"Win+Z";l.settings.snapshots=false;
 auto nr=Deserialize(Serialize(l));
 expect(nr.zones[0].sortKey==L"date"&&nr.zones[0].sortDescending&&nr.zones[0].tileSize==2&&nr.settings.hotkey==L"Win+Z"&&!nr.settings.snapshots,"v07 fields roundtrip");
 auto legacy7=Deserialize("{\"version\":1,\"zones\":[]}");
 expect(legacy7.settings.hotkey==L"Ctrl+Alt+G"&&legacy7.settings.snapshots,"legacy defaults for v07 settings");
 auto bad7=Deserialize("{\"version\":1,\"zones\":[{\"id\":\"a\",\"name\":\"n\",\"x\":0,\"y\":0,\"width\":280,\"height\":160,\"collapsed\":false,\"entries\":[],\"tileSize\":7,\"sortKey\":\"weird\"}],\"settings\":{\"hotkey\":\"Bad+\"}}");
 expect(bad7.zones[0].tileSize==1&&bad7.zones[0].sortKey.empty()&&bad7.settings.hotkey.empty(),"invalid v07 values fall back");
report<<"TOTAL "<<passed<<" passed\n";
}
}
