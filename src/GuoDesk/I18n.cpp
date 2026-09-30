#include "pch.h"
#include "I18n.h"
namespace guodesk::i18n {
namespace {
std::wstring g_lang;
struct KV { wchar_t const* key; wchar_t const* en; };
KV const table[]{
 {L"主配置缺失，已从备份恢复。",L"Main config missing — restored from backup."},
 {L"配置损坏，已保留原文件。",L"Config corrupt — original file preserved. "},
 {L"已从备份恢复。",L"Restored from backup."},
 {L"使用新的默认分区。",L"Starting with a new default zone."},
 {L"未命名规则",L"Untitled rule"},
 {L"常用",L"Frequent"},
 {L"文档",L"Documents"},
 {L"图片",L"Images"},
 {L"安装包",L"Installers"},
 {L"GuoDesk 启动失败",L"GuoDesk failed to start"},
 {L"添加文件夹入口",L"Add folder"},
 {L"添加文件或应用入口",L"Add file or app"},
 {L"分区名称",L"Zone name"},
 {L"点击重命名分区",L"Click to rename zone"},
 {L"未命名分区",L"Untitled zone"},
 {L"按住移动分区",L"Drag to move zone"},
 {L"添加入口",L"Add entry"},
 {L"折叠 / 展开",L"Collapse / Expand"},
 {L"更多操作",L"More actions"},
 {L"映射分区为只读视图，无法拖入。",L"Mapped zones are read-only — dropping is disabled."},
 {L"操作未完成，请检查路径或访问权限。",L"Operation failed — check the path or access permissions."},
 {L"操作未完成，请检查文件是否存在及访问权限。",L"Operation failed — check that the file exists and is accessible."},
 {L"映射分区为只读视图：请在资源管理器中修改文件夹后右键刷新。",L"Mapped zones are read-only: edit the folder in File Explorer, then right-click to refresh."},
 {L"添加文件 / 应用",L"Add file / app"},
 {L"添加文件夹",L"Add folder"},
 {L"取消文件夹映射",L"Remove folder mapping"},
 {L"映射文件夹…",L"Map folder…"},
 {L"已取消映射，恢复普通分区。",L"Mapping removed — zone restored to normal."},
 {L"已映射文件夹（只读视图）：修改请在资源管理器中完成，右键可刷新。",L"Folder mapped (read-only view): make changes in File Explorer, right-click to refresh."},
 {L"切换为图标视图",L"Switch to icon view"},
 {L"切换为列表视图",L"Switch to list view"},
 {L"文件名：{0}（点击切换）",L"File names: {0} (click to cycle)"},
 {L"隐藏",L"Hidden"},
 {L"一行",L"1 line"},
 {L"两行",L"2 lines"},
 {L"新增分区",L"New zone"},
 {L"立即刷新",L"Refresh now"},
 {L"切换普通窗口",L"Switch to normal windows"},
 {L"试验桌面嵌入",L"Try desktop embed"},
 {L"设置",L"Settings"},
 {L"删除分区（保留原文件）",L"Delete zone (keeps files)"},
 {L"打开",L"Open"},
 {L"定位原文件",L"Reveal in File Explorer"},
 {L"重新定位文件",L"Relocate file"},
 {L"移除入口（保留原文件）",L"Remove entry (keeps files)"},
 {L"此目标已在当前分区中。 ",L"This item is already in the zone. "},
 {L"无法打开目标，请右键重新定位。 ",L"Cannot open the target — right-click to relocate. "},
 {L"映射文件夹为空或不可访问",L"Mapped folder is empty or inaccessible"},
 {L"映射视图（只读）· {0} · 共 {1} 项",L"Mapped view (read-only) · {0} · {1} items"},
 {L" · 已显示前 {0} 项",L" · showing first {0}"},
 {L" · 双击打开 · 右键更多",L" · double-click to open · right-click for more"},
 {L"拖入文件、文件夹或应用快捷方式 · 原文件保持原位",L"Drop files, folders or app shortcuts · originals stay in place"},
 {L"双击打开 · 右键管理 · 拖拽排序",L"Double-click to open · right-click to manage · drag to reorder"},
 {L"无法添加拖入项目。",L"Could not add the dropped items."},
 {L"实验性桌面宿主 · 添加入口不会移动原文件",L"Experimental desktop host — entries never move the original files"},
 {L"嵌入失败，已保留普通窗口模式",L"Embed failed — kept normal window mode"},
 {L"普通窗口模式 · 可在菜单中试验桌面嵌入",L"Normal window mode — try desktop embed from the menu"},
 {L"GuoDesk · 桌面分区",L"GuoDesk · Desktop Zones"},
 {L"显示全部",L"Show all"},
 {L"整理桌面…",L"Tidy desktop…"},
 {L"便签",L"Note"},
 {L"待办",L"To-Do"},
 {L"退出",L"Exit"},
 {L"保存失败：请检查本地数据目录权限和剩余空间。 ",L"Save failed — check permissions and free space for the local data folder. "},
 {L"新分区",L"New zone"},
 {L"GuoDesk 配置恢复",L"GuoDesk config recovery"},
 {L"操作未完成。",L"Operation not completed."},
 {L"GuoDesk 设置",L"GuoDesk Settings"},
 {L"无法写入注册表，请检查权限。",L"Cannot write to the registry — check permissions."},
 {L"外观",L"Appearance"},
 {L"跟随系统",L"Follow system"},
 {L"浅色",L"Light"},
 {L"深色",L"Dark"},
 {L"紧凑磁贴",L"Compact tiles"},
 {L"常规",L"General"},
 {L"开机自动启动",L"Run at startup"},
 {L"整理规则",L"Tidy rules"},
 {L"按扩展名或文件名关键词，把桌面文件以引用方式归入分区——原文件始终保持在桌面。",L"Sort desktop files into zones by extension or filename keywords — as references; originals always stay on the desktop."},
 {L"添加规则",L"Add rule"},
 {L"预览整理…",L"Preview tidy…"},
 {L"关于",L"About"},
 {L"GuoDesk v0.6.0 · 桌面分区整理\n引用式入口：只存引用，不动原文件\n缺失入口可右键重新定位\n便签与待办：托盘右键开启，本地保存\n\nMIT License · cloudlight369",
  L"GuoDesk v0.6.0 · Desktop zone organizer\nReference-based entries: only links stored, originals untouched\nRight-click a missing entry to relocate it\nNote & To-Do: enable from the tray menu, stored locally\n\nMIT License · cloudlight369"},
 {L" · 关键词 ",L" · keywords "},
 {L"未设置匹配条件",L"No match condition set"},
 {L"未绑定分区",L"No zone bound"},
 {L"删除规则",L"Delete rule"},
 {L"暂无规则，点击下方“添加规则”。",L"No rules yet — click \"Add rule\" below."},
 {L"编辑规则",L"Edit rule"},
 {L"保存",L"Save"},
 {L"取消",L"Cancel"},
 {L"如：文档",L"e.g. Documents"},
 {L"规则名称",L"Rule name"},
 {L"doc, pdf, txt（逗号分隔，可留空）",L"doc, pdf, txt (comma separated, optional)"},
 {L"按扩展名匹配",L"Match by extension"},
 {L"如：简历, 报告（文件名包含即可）",L"e.g. resume, report (filename contains)"},
 {L"按文件名关键词匹配",L"Match by filename keywords"},
 {L"（未绑定）",L"(unbound)"},
 {L"整理到分区",L"Tidy into zone"},
 {L"新规则",L"New rule"},
 {L"语言",L"Language"},
 {L"语言将在重启 GuoDesk 后生效。",L"Language changes take effect after restarting GuoDesk."},
 {L"备份",L"Backup"},
 {L"导出配置",L"Export config"},
 {L"导入配置",L"Import config"},
 {L"已导出到 {0}",L"Exported to {0}"},
 {L"导入失败：文件不是有效的 GuoDesk 配置。",L"Import failed — the file is not a valid GuoDesk config."},
 {L"导入完成，界面已按新配置重建。",L"Import complete — windows rebuilt with the new config."},
 {L"导出失败：请检查目标位置是否可写。",L"Export failed — check that the destination is writable."},
 {L"选择要导入的 GuoDesk 配置",L"Choose a GuoDesk config to import"},
 {L"整理桌面",L"Tidy desktop"},
 {L"应用整理",L"Apply tidy"},
 {L"已添加 {0} 个引用，即将关闭",L"Added {0} references — closing soon"},
 {L"引用均已存在，即将关闭",L"All references already exist — closing soon"},
 {L"关闭",L"Close"},
 {L"另有 {0} 项未匹配规则，保持原位",L"{0} items matched no rules and stay in place"},
 {L"没有匹配项",L"No matches"},
 {L"应用整理（添加 {0} 个引用）",L"Apply tidy (adds {0} references)"},
 {L"桌面上没有匹配规则的新文件。",L"No new desktop files match any rule."},
 {L"以下 {0} 个桌面文件将作为引用加入分区，原文件保持原位。",L"These {0} desktop files will be added to zones as references — originals stay in place."},
 {L"GuoDesk 整理预览",L"GuoDesk Tidy Preview"},
 {L"GuoDesk 整理预览 · {0} 项",L"GuoDesk Tidy Preview · {0} items"},
 {L"GuoDesk 便签",L"GuoDesk Note"},
 {L"记点什么…",L"Jot something…"},
 {L"{0}/{1} 已完成",L"{0}/{1} done"},
 {L"还没有待办，从下方添加一条",L"No to-dos yet — add one below"},
 {L"GuoDesk 待办",L"GuoDesk To-Do"},
 {L"添加待办，回车确认",L"Add a to-do — press Enter"},
 {L"添加",L"Add"},
};
std::wstring Resolve(std::wstring const& setting){
 std::wstring lang=setting;
 if(lang.empty()||lang==L"system"){
  wchar_t name[LOCALE_NAME_MAX_LENGTH]{};
  if(!LCIDToLocaleName(MAKELCID(GetUserDefaultUILanguage(),SORT_DEFAULT),name,LOCALE_NAME_MAX_LENGTH,0))name[0]=0;
  lang=name;
 }
 return lang.rfind(L"zh",0)==0?std::wstring(L"zh-CN"):std::wstring(L"en-US");
}
}
void SetLanguage(std::wstring const& setting){g_lang=Resolve(setting);}
std::wstring CurrentLanguage(){if(g_lang.empty())g_lang=Resolve(L"");return g_lang;}
std::wstring Tr(std::wstring const& key){
 if(CurrentLanguage()!=L"en-US")return key;
 for(auto const& kv:table)if(key==kv.key)return kv.en;
 return key;
}
std::wstring Fmt(std::wstring const& text,std::initializer_list<std::wstring> args){
 std::wstring out;size_t pos=0;
 while(pos<text.size()){
  auto b=text.find(L'{',pos);
  if(b==std::wstring::npos){out.append(text,pos,std::wstring::npos);break;}
  out.append(text,pos,b-pos);
  if(b+1<text.size()&&text[b+1]==L'{'){out.push_back(L'{');pos=b+2;continue;}
  auto e=text.find(L'}',b+1);
  if(e==std::wstring::npos){out.append(text,b,std::wstring::npos);break;}
  wchar_t* end{};long n=wcstol(std::wstring(text,b+1,e-b-1).c_str(),&end,10);
  if(*end==0&&n>=0&&n<static_cast<long>(args.size()))out.append(*(args.begin()+n));
  else out.append(text,b,e-b+1);
  pos=e+1;
 }
 return out;
}
std::wstring TrF(std::wstring const& key,std::initializer_list<std::wstring> args){return Fmt(Tr(key),args);}
}
