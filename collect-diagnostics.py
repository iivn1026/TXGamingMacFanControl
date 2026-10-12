from pathlib import Path
import datetime
import hashlib
import json
import plistlib
import subprocess

root = Path(__file__).resolve().parent
helper = root / 'TXGamingMacFanControl.app/Contents/Resources/txfanctl'
result = subprocess.run([str(helper), 'status'], capture_output=True, text=True, timeout=20)
report = {'recorded_at': datetime.datetime.now().isoformat(), 'helper_exit_code': result.returncode}
if result.returncode == 0:
    report['status'] = json.loads(result.stdout)
else:
    report['error'] = result.stderr.strip()
caps = subprocess.run([str(helper), 'capabilities'], capture_output=True, text=True, timeout=20)
if caps.returncode == 0:
    report['capabilities'] = json.loads(caps.stdout)
loaded = subprocess.run(['kmutil','showloaded','--list-only'],capture_output=True,text=True,timeout=20)
report['driver_query_exit_code'] = loaded.returncode
report['bridge_loaded'] = (any('local.TXGamingFanBridge' in line for line in loaded.stdout.splitlines())
                          if loaded.returncode == 0 else None)
if loaded.returncode:
    report['driver_query_error'] = loaded.stderr.strip()
registry = subprocess.run(['ioreg','-a','-r','-c','TXGamingFanBridge'],capture_output=True,timeout=20)
if registry.returncode == 0:
    try:
        nodes = plistlib.loads(registry.stdout)
        report['bridge_properties'] = [{key: node[key] for key in ['BridgeVersion','TargetModel','ProfileChangesEnabled'] if key in node} for node in nodes]
    except Exception:
        report['bridge_properties'] = []
report['registry_query_exit_code'] = registry.returncode
report['packaged_driver_sha256'] = hashlib.sha256((root/'TXGamingFanBridge.kext/Contents/MacOS/TXGamingFanBridge').read_bytes()).hexdigest()
output = root / 'diagnostics-latest.json'
output.write_text(json.dumps(report,ensure_ascii=False,indent=2))
print(json.dumps(report,ensure_ascii=False,indent=2))
print('Saved:',output)
