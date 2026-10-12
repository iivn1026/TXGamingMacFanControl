from pathlib import Path
import hashlib
import shutil
import subprocess

ROOT = Path(__file__).resolve().parent
DIST = ROOT / 'dist'
DIST.mkdir(exist_ok=True)
STAGE = ROOT / 'build/dmg-stage'
if STAGE.exists():
    shutil.rmtree(STAGE)
STAGE.mkdir()
APP = ROOT / 'TXGamingMacFanControl.app'
subprocess.run(['codesign', '--verify', '--deep', '--strict', str(APP)], check=True)
shutil.copytree(APP, STAGE / APP.name)
(STAGE / 'Applications').symlink_to('/Applications')
driver = STAGE / 'EFI Driver'
driver.mkdir()
shutil.copytree(ROOT / 'TXGamingFanBridge.kext', driver / 'TXGamingFanBridge.kext')
shutil.copyfile(ROOT / 'SSDT-TXGFAN.aml', driver / 'SSDT-TXGFAN.aml')
shutil.copyfile(ROOT / 'examples/OpenCore-fan-entries.plist', driver / 'OpenCore-fan-entries.plist')
for name in ['INSTALL.md', 'README.md', 'LICENSE', 'LICENSE-FanControl.txt', 'NOTICE.txt']:
    shutil.copyfile(ROOT / name, STAGE / name)
shutil.copytree(ROOT / 'docs', STAGE / 'docs')
shutil.copytree(ROOT / 'assets', STAGE / 'assets')
manifest = []
for path in sorted(STAGE.rglob('*')):
    if path.is_file() and not path.is_symlink():
        manifest.append(hashlib.sha256(path.read_bytes()).hexdigest() + '  ' + path.relative_to(STAGE).as_posix())
(STAGE / 'PACKAGE-SHA256.txt').write_text('\n'.join(manifest) + '\n')
dmg = DIST / 'TXGamingMacFanControl-0.4.0-Intel.dmg'
subprocess.run(['hdiutil','create','-ov','-volname','TXGamingMacFanControl','-srcfolder',str(STAGE),'-format','UDZO',str(dmg)],check=True)
(DIST / 'SHA256SUMS.txt').write_text(hashlib.sha256(dmg.read_bytes()).hexdigest() + '  ' + dmg.name + '\n')
print('Created:', dmg)
