#!/usr/bin/env python3
"""Export actual C PCM; independently check full samples, spectra and block seams."""
import argparse
import array
import hashlib
import json
import math
import subprocess
import sys
import tempfile
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RATE, SECONDS = 44100, 47
STAGES = [(5+7*i, 10+7*i, (-30 if i<3 else -24), ((250,), (1000,), (250,1000))[i%3]) for i in range(6)]

def expected():
    """Direct full-length sine calculation, independent of table lookup/fill C."""
    values = array.array('h', [0]) * (RATE * SECONDS)
    for start, end, db, freqs in STAGES:
        period = [sum(math.sin(2*math.pi*f*n/RATE) for f in freqs) for n in range(882)]
        gain = 32768*10**(db/20)/max(map(abs,period))
        for n in range(5*RATE):
            envelope = 1.0
            if n < 2205:
                envelope = .5-.5*math.cos(math.pi*n/2204)
            elif n >= 5*RATE-2205:
                envelope = .5+.5*math.cos(math.pi*(n-(5*RATE-2205))/2204)
            value = sum(math.sin(2*math.pi*f*n/RATE) for f in freqs)*gain*envelope
            values[start*RATE+n] = math.floor(value+.5) if value>=0 else math.ceil(value-.5)
    return values

def line_amplitude(samples, frequency):
    # Exactly one second, coherent DFT, no FFT/library dependency.
    c = sum(v*math.cos(2*math.pi*frequency*n/RATE) for n,v in enumerate(samples))
    s = sum(v*math.sin(2*math.pi*frequency*n/RATE) for n,v in enumerate(samples))
    return 2*math.hypot(c,s)/len(samples)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out',type=Path,default=ROOT/'artifacts')
    args=parser.parse_args()
    args.out.mkdir(parents=True,exist_ok=True)
    subprocess.run([sys.executable,str(ROOT/'tools/generate_pcm.py'),'--check'],check=True)
    mono = expected()
    report={'rate_hz':RATE,'format':'signed 16-bit little-endian interleaved [L,R]',
            'duration_seconds':SECONDS,'fade_ms':50,'reference_origin':'first I2S frame, after boot/setup',
            'checks':[], 'stages':[]}
    with tempfile.TemporaryDirectory() as tmp:
        executable=Path(tmp)/'export'
        subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(ROOT/'src'),str(ROOT/'tools/export_pcm.c'),str(ROOT/'src/sequence.c'),'-o',str(executable)],check=True)
        for left in (0,1):
            pcm=subprocess.check_output([str(executable),'882',str(left)])
            for chunk in (257,1024):
                assert subprocess.check_output([str(executable),str(chunk),str(left)])==pcm, f'buffer discontinuity at chunk {chunk}'
            packed=array.array('h'); packed.frombytes(pcm)
            if sys.byteorder!='little': packed.byteswap()
            active=packed[(0 if left else 1)::2]
            inactive=packed[(1 if left else 0)::2]
            assert not any(inactive), 'inactive channel not zero'
            assert active==mono, 'C PCM differs from independent direct sine/fade calculation'
            assert max(map(abs,active)) < 32767, 'clipping'
            name=f"reference-{'left' if left else 'right'}.wav"
            with wave.open(str(args.out/name),'wb') as w:
                w.setnchannels(2); w.setsampwidth(2); w.setframerate(RATE); w.writeframes(pcm)
            report[name]={'sha256':hashlib.sha256((args.out/name).read_bytes()).hexdigest(),'pcm_sha256':hashlib.sha256(pcm).hexdigest(),'active_slot':('L' if left else 'R')}
        for start,end,db,freqs in STAGES:
            section=mono[(start+1)*RATE:(start+2)*RATE]
            peak=max(map(abs,mono[start*RATE:end*RATE]))
            peak_db=20*math.log10(peak/32768)
            assert abs(peak_db-db)<.005
            amplitudes={str(f):line_amplitude(section,f) for f in freqs}
            # One-second coherent frequency scan: target must exceed adjacent bins by 50 dB.
            adjacent={str(f):max(line_amplitude(section,f-1),line_amplitude(section,f+1)) for f in freqs}
            for f in freqs: assert amplitudes[str(f)] > 316*max(adjacent[str(f)],1e-9)
            # Each quantized steady period repeats exactly, including across DMA seams.
            body=mono[start*RATE+2205:end*RATE-2205]
            assert body[882:]==body[:-882]
            assert mono[start*RATE]==0 and mono[end*RATE-1]==0
            report['stages'].append({'start_s':start,'end_s':end,'frequencies_hz':freqs,'target_peak_dbfs':db,'peak_counts':peak,'actual_peak_dbfs':peak_db,'coherent_amplitudes_counts':amplitudes,'largest_adjacent_bin_counts':adjacent})
        for low,high in zip(report['stages'][:3],report['stages'][3:]):
            assert abs(high['actual_peak_dbfs']-low['actual_peak_dbfs']-6)<.005
    report['checks']=['tables reproduce','full 47-second C output equals independent sine/envelope calculation','both channels exported; unused slot exactly zero','all silence and fades match','no clipping','all six peaks within 0.005 dB','6 dB step within 0.005 dB','250/1000 Hz coherent line and adjacent-bin checks','periodic body byte continuity','882/257/1024-frame output identical','post-sequence zeros']
    (args.out/'pcm-checks.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({'checks':'passed','references':[report[n] for n in ('reference-right.wav','reference-left.wav')],'stages':report['stages']},indent=2))

if __name__=='__main__': main()
