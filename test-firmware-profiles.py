from pathlib import Path
import datetime
import json
import os
import subprocess
import time

root = Path(__file__).resolve().parent
helper = root/'TXGamingMacFanControl.app/Contents/Resources/txfanctl'
assert os.geteuid() == 0, 'Administrator privilege is required'

def status():
    r = subprocess.run([str(helper),'status'],capture_output=True,text=True,timeout=15,check=True)
    return json.loads(r.stdout)

def select(mode):
    r = subprocess.run([str(helper),'profile',str(mode)],capture_output=True,text=True,timeout=15)
    if r.returncode:
        raise RuntimeError(r.stderr.strip())
    reply = json.loads(r.stdout)
    assert reply.get('ok') and reply.get('profile') == mode
    s = status()
    assert s['profile'] == mode, 'Independent mode readback differs'
    return s

initial = status()
assert initial['bridgeConnected'] and initial['profileSupported']
assert initial['profile'] in (0,1,2), 'Cannot safely restore an unknown initial mode'
report = {'started_at':datetime.datetime.now().isoformat(),
          'initial':initial, 'steps':[], 'original_mode_restored':False}
destination = root/'profile-test-latest.json'
try:
    for mode in (0,1):
        current = select(mode)
        time.sleep(5)
        later = status()
        assert later['profile'] == mode, 'Firmware mode did not persist'
        report['steps'].append({'requested':mode,'immediate':current,'after_5_seconds':later})
        destination.write_text(json.dumps(report,ensure_ascii=False,indent=2))
finally:
    try:
        restored = select(initial['profile'])
        time.sleep(2)
        report['restored'] = status()
        report['original_mode_restored'] = report['restored']['profile'] == initial['profile']
    except BaseException as e:
        report['restore_error'] = str(e)
    report['finished_at'] = datetime.datetime.now().isoformat()
    destination.write_text(json.dumps(report,ensure_ascii=False,indent=2))
    print(json.dumps(report,ensure_ascii=False,indent=2))
assert report['original_mode_restored'], 'Original firmware mode could not be restored'
