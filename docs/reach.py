#!/usr/bin/env python3
"""依赖可达性分析：找出 .app 里从主程序出发不可达的 Frameworks dylib（瘦身候选）。

用法：python3 reach.py <path/to/X.app> [主程序名，默认 flare-client]

原理：otool -L 递归遍历 @executable_path/@rpath/@loader_path 依赖，从主程序 + 必需插件
     （platform / style / TLS 后端 / 图像格式）出发求可达集；不在集合内的即不可达。

结论（flare-client 实测）：homebrew Qt 的 ICU/OpenSSL/glib/dbus 全都是 QtCore/QtNetwork/
QtGui 的硬链接（otool 直接可见），删了 dyld 会拒绝启动——真正不可达的只有 ~3.6M。
所以「删文件」的天花板很低，要真正瘦身必须自建精简 Qt（-no-icu -no-openssl-runtime ...）。

注意：不可达 != 一定能删。经 QLibrary 动态加载的插件不会被 otool 记录，
     删前务必跑一次启动 + 功能验活（open / pgrep）。
"""
import os, re, subprocess, sys

APP = sys.argv[1] if len(sys.argv) > 1 else '/tmp/flare-trim.app'
EXE_NAME = sys.argv[2] if len(sys.argv) > 2 else 'flare-client'
files={}
for root,dirs,fs in os.walk(APP):
    for f in fs:
        p=os.path.join(root,f)
        if os.path.islink(p): continue
        files[os.path.basename(p)]=p
def deps(p):
    try: out=subprocess.run(['otool','-L',p],capture_output=True,text=True,timeout=25).stdout
    except Exception: return []
    return [os.path.basename(m.group(0)) for m in re.finditer(r'@(?:executable_path|rpath|loader_path)[^\s]*',out)]
exe=os.path.join(APP,'Contents/MacOS',EXE_NAME)
keep_plugins=['libqcocoa.dylib','libqmacstyle.dylib','libqsecuretransportbackend.dylib','libqjpeg.dylib','libqsvgicon.dylib','libqapplenetworkinformation.dylib']
roots=[exe]+[files[b] for b in keep_plugins if b in files]
seen=set(); stack=list(roots)
while stack:
    cur=stack.pop()
    b=os.path.basename(cur)
    if b in seen: continue
    seen.add(b)
    for d in deps(cur):
        if d in seen: continue
        if d in files: stack.append(files[d])
        else: seen.add(d)
unreach=[]
for b,p in files.items():
    if '/Frameworks/' not in p: continue
    if b in seen: continue
    try: sz=os.path.getsize(p)
    except OSError: continue
    unreach.append((sz,p))
unreach.sort(reverse=True)
tot=sum(s for s,_ in unreach)
for s,p in unreach[:20]: print('%7.1fM  %s'%(s/1048576,p.split('Frameworks/')[1]))
print('== 不可达合计 %.1fM / %d 个文件'%(tot/1048576,len(unreach)))
print('== 可达必需: '+', '.join(sorted(b for b in seen if b in files and '/Frameworks/' in files[b])))
