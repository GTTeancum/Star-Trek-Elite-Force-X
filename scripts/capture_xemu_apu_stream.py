"""Capture the pinned XEMU process's APU samples at its native SDL handoff.
No desktop input, host audio capture, guest writes, or output redirection.
This diagnostic Frida hook adds overhead: never use its run as FPS acceptance.
"""
import argparse
import hashlib
import json
from pathlib import Path
import time
import wave
import frida
import pefile

EXPECTED='f59a9df35f7d3be20da091d14066d4670e97ec9ee27ddf1d9d4b1abf99d2fe72'
HOOK=0x285210

def main():
    p=argparse.ArgumentParser();p.add_argument('--pid',type=int,required=True);p.add_argument('--exe',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True);p.add_argument('--seconds',type=int,default=300);a=p.parse_args()
    assert 1<=a.seconds<=450
    assert hashlib.sha256(a.exe.read_bytes()).hexdigest()==EXPECTED,'Unsupported executable'
    pe=pefile.PE(str(a.exe))
    # Matched to fc9980d monitor.c: SDL Put(stream, frame,1024), then clear frame.
    assert pe.get_data(0x2851fa,22)==bytes.fromhex('4889f2488b8bb867160041b800040000ff9078150000'), 'Handoff signature changed'
    assert pe.get_data(HOOK,9)==bytes.fromhex('488dbbbc63160031c0'),'Frame-clear signature changed'
    assert not a.output.exists()
    meta=dict(pid=a.pid,exe=str(a.exe),exe_sha256=EXPECTED,hook_rva=hex(HOOK),
              source='APU stereo S16LE immediately after native SDL handoff, before frame clear; pre stream gain',
              frames=0,accepted=0,rejected=0,errors=[],format='s16le',rate=48000,channels=2,
              limitations='Proves source samples and SDL acceptance, not speaker delivery, perceptual clarity or retail Xbox. Capture adds diagnostic overhead.')
    session=frida.attach(a.pid);closed=[]
    session.on('detached',lambda *args:closed.append(True))
    started=time.monotonic()
    try:
        with wave.open(str(a.output),'wb') as w:
            w.setnchannels(2);w.setsampwidth(2);w.setframerate(48000)
            def receive(message,data):
                if message.get('type')=='send':
                    payload=message['payload']
                    if payload['kind']=='audio':
                        w.writeframesraw(data);meta['frames']+=1
                        meta['accepted' if payload['accepted'] else 'rejected']+=1
                    else:meta['identity']=payload
                elif message.get('type')=='error':meta['errors'].append(message)
            js=r"""
            const m=Process.mainModule;
            if(m.path.toLowerCase().replace(/\\/g,'/')!==EXPECTED_PATH)throw new Error('Wrong target executable');
            send({kind:'identity',path:m.path,base:m.base.toString()});
            Interceptor.attach(m.base.add(0x285210),{onEnter(){
                send({kind:'audio',accepted:(this.context.rax.toUInt32()&255)!==0},this.context.rsi.readByteArray(1024));
            }});
            """
            js=js.replace('EXPECTED_PATH',json.dumps(str(a.exe.resolve()).lower().replace('\\','/')))
            script=session.create_script(js);script.on('message',receive);script.load()
            while time.monotonic()-started<a.seconds and not closed:time.sleep(0.2)
    finally:
        if not closed:session.detach()
        meta['host_seconds']=time.monotonic()-started
        a.output.with_suffix('.json').write_text(json.dumps(meta,indent=2))
    assert meta['frames'] and not meta['errors'],meta
    print(json.dumps(meta,indent=2))
if __name__=='__main__':main()
