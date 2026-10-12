from pathlib import Path
import plistlib
import shutil
import subprocess

ROOT = Path(__file__).resolve().parent
SDK = Path(subprocess.check_output(['xcrun', '--show-sdk-path'], text=True).strip())
HEADERS = SDK / 'System/Library/Frameworks/Kernel.framework/Headers'
WORK = ROOT / 'build'
WORK.mkdir(exist_ok=True)
IASL = shutil.which('iasl') or '/Applications/Hackintool.app/Contents/Resources/Utilities/iasl'
subprocess.run([IASL,
                '-p',str(ROOT/'SSDT-TXGFAN'),str(ROOT/'src/SSDT-TXGFAN.dsl')],check=True)
APP = ROOT / 'TXGamingMacFanControl.app'
MACOS = APP / 'Contents/MacOS'
RESOURCES = APP / 'Contents/Resources'
MACOS.mkdir(parents=True, exist_ok=True)
RESOURCES.mkdir(parents=True, exist_ok=True)

def run(*args):
    subprocess.run([str(a) for a in args], check=True)

run('clang++', '-std=c++17', '-arch', 'x86_64', '-mmacosx-version-min=10.15',
    '-fno-rtti', '-fno-exceptions', '-fapple-kext', '-nostdinc', '-DKERNEL',
    '-DKERNEL_PRIVATE', '-DAPPLE_KEXT_VTABLE_PADDING', '-I', HEADERS,
    '-I', Path(subprocess.check_output(['clang', '-print-resource-dir'],text=True).strip())/'include',
    '-c', ROOT/'src/TXGamingFanBridge.cpp', '-o', WORK/'bridge.o')
KEXT = ROOT / 'TXGamingFanBridge.kext'
(KEXT/'Contents/MacOS').mkdir(parents=True, exist_ok=True)
run('clang++','-arch','x86_64','-nostdlib','-Wl,-kext','-Wl,-undefined,dynamic_lookup',
    WORK/'bridge.o','-o',KEXT/'Contents/MacOS/TXGamingFanBridge')
info = {'CFBundleIdentifier':'local.TXGamingFanBridge', 'CFBundleExecutable':'TXGamingFanBridge',
        'CFBundleName':'TXGamingFanBridge','CFBundlePackageType':'KEXT',
        'CFBundleVersion':'0.4.0','CFBundleShortVersionString':'0.4.0',
        'OSBundleRequired':'Root','OSBundleLibraries':{
            'com.apple.kpi.iokit':'8.0.0','com.apple.kpi.libkern':'8.0.0',
            'com.apple.kpi.mach':'8.0.0','com.apple.kpi.bsd':'8.0.0',
            'com.apple.iokit.IOACPIFamily':'1.0.0d1',
            'com.apple.kpi.dsep':'12.0.0','com.apple.kpi.unsupported':'12.0.0'},
        'IOKitPersonalities':{'TXGamingFanBridge':{
            'CFBundleIdentifier':'local.TXGamingFanBridge',
            'IOClass':'TXGamingFanBridge','IOProviderClass':'IOACPIPlatformDevice',
            'IONameMatch':['PNP0C14','PNP0c14','ATK0100','ATKD'],
            'IOMatchCategory':'TXGamingFanBridge','IOProbeScore':1000,
            'EnableProfileChanges':False}}}
(KEXT/'Contents/Info.plist').write_bytes(plistlib.dumps(info,sort_keys=False))
run('clang','-O2','-Wall','-Wextra',ROOT/'src/statusreader.c',
    '-framework','IOKit','-framework','CoreFoundation','-o',RESOURCES/'statusreader')
run('clang','-O2','-fobjc-arc',ROOT/'src/txfanctl.m',
    '-framework','Foundation','-framework','IOKit','-o',RESOURCES/'txfanctl')
run('swiftc','-O','-parse-as-library','-target','x86_64-apple-macos13.0',
    '-sdk',SDK,'-module-cache-path',WORK/'ModuleCache',ROOT/'src/App.swift','-framework','SwiftUI','-framework','AppKit',
    '-o',MACOS/'TXGamingFanControl')
appinfo = {'CFBundleIdentifier':'local.TXGamingFanControl','CFBundleExecutable':'TXGamingFanControl',
           'CFBundleName':'TXGamingMacFanControl','CFBundleDisplayName':'TXGamingMacFanControl',
           'CFBundleIconFile':'AppIcon',
           'CFBundlePackageType':'APPL','CFBundleVersion':'0.4.0',
           'CFBundleShortVersionString':'0.4.0','LSMinimumSystemVersion':'13.0',
           'NSHighResolutionCapable':True}
(APP/'Contents/Info.plist').write_bytes(plistlib.dumps(appinfo,sort_keys=False))
for p in [RESOURCES/'statusreader',RESOURCES/'txfanctl',MACOS/'TXGamingFanControl']:
    p.chmod(0o755)
shutil.copyfile(ROOT/'assets/AppIcon.icns',RESOURCES/'AppIcon.icns')
for name in ['LICENSE','LICENSE-FanControl.txt','NOTICE.txt']:
    shutil.copyfile(ROOT/name,RESOURCES/name)
run('codesign','--force','--deep','--sign','-',APP)
run('plutil','-lint',APP/'Contents/Info.plist',KEXT/'Contents/Info.plist')
print('Built application and read-only-by-default ASUS bridge:',ROOT)
