"""Compile the production notch and PCM hook under address/undefined sanitizers."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
out = root / '.pio/audio_native'
out.mkdir(parents=True, exist_ok=True)
subprocess.run(['c++', '-std=c++17', '-O1', '-g', '-fsanitize=address,undefined,float-cast-overflow',
                '-I', str(root / 'tools/native_audio'), '-I', str(root / 'include'),
                str(root / 'tools/native_audio/notch.cpp'), str(root / 'src/audio_filter.cpp'),
                '-o', str(out / 'notch')], check=True)
subprocess.run([str(out / 'notch')], check=True)

# Exercise the shipped Spotify feed method with an immediate in-memory I2S sink.
sink = (root/'components/radiohead_spotify/MAX98357AAudioSink.cpp').read_text()
header = (root/'components/radiohead_spotify/MAX98357AAudioSink.h').read_text()
fixture = r'''
#include "audio_notch.h"
#include "audio_tone.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
using i2s_chan_handle_t=void*;
using esp_err_t=int;
constexpr int ESP_OK=0;
#define ESP_LOGE(...) ((void)0)
struct AudioSink {
 bool softwareVolumeControl=false;
 virtual ~AudioSink()=default;
 virtual void feedPCMFrames(const uint8_t*,size_t)=0;
 virtual void volumeChanged(uint16_t)=0;
 virtual bool setParams(uint32_t,uint8_t,uint8_t)=0;
};
std::vector<int16_t> captured;
int64_t esp_timer_get_time(){return 0;}
int i2s_channel_write(void*,const uint8_t* data,size_t bytes,size_t* written,int){
 const auto count=captured.size();captured.resize(count+bytes/2);
 memcpy(captured.data()+count,data,bytes);*written=bytes;return ESP_OK;
}
'''
fixture += header[header.index('class MAX98357AAudioSink'):]
fixture += '\nMAX98357AAudioSink::MAX98357AAudioSink(){tx_=reinterpret_cast<void*>(1);}\nMAX98357AAudioSink::~MAX98357AAudioSink(){}\nbool MAX98357AAudioSink::setParams(uint32_t,uint8_t,uint8_t){return true;}\n'
fixture += sink[sink.index('void MAX98357AAudioSink::feedPCMFrames'):sink.index('bool MAX98357AAudioSink::setParams')]
fixture += r'''
int main(){
 MAX98357AAudioSink sink;
 std::vector<int16_t> input={INT16_MIN,INT16_MAX,1234,-2345};
 for(unsigned gain:{0,128,255}){
  captured.clear();sink.volumeChanged(gain);
  radioNotchConfiguration.store(3000);
  sink.feedPCMFrames(reinterpret_cast<const uint8_t*>(input.data()),input.size()*2);
  assert(captured.size()==input.size());
  for(size_t i=0;i<input.size();++i)assert(captured[i]==int32_t(input[i])*int(gain)/255);
 }
 captured.clear();sink.feedPCMFrames(reinterpret_cast<const uint8_t*>(input.data()),6);assert(captured.empty());
 sink.volumeChanged(255);input.resize(1024);
 for(unsigned endHz:{0,5100}){
  const unsigned startHz=endHz?5000:3000;
  radioNotchConfiguration.store(radioNotchConfigurationValue(true,startHz,endHz));
  const double pi=3.14159265358979323846;
  const double frequency=endHz?44100/pi*std::atan(std::sqrt(std::tan(pi*5000/44100)*std::tan(pi*5100/44100))):3000;
  double energy=0;unsigned count=0;
  for(unsigned block=0;block<100;++block){
   for(unsigned i=0;i<input.size();i+=2){input[i]=int16_t(10000*std::sin(2*pi*frequency*(block*512+i/2)/44100));input[i+1]=0;}
   captured.clear();sink.feedPCMFrames(reinterpret_cast<const uint8_t*>(input.data()),input.size()*2);
   assert(captured.size()==input.size());
   for(unsigned i=0;i<captured.size();i+=2){assert(captured[i+1]==0);if(block>50){energy+=double(captured[i])*captured[i];++count;}}
  }
  assert(std::sqrt(energy/count)<5);
 }
 puts("Production Spotify PCM feed: bit-exact disabled bypass at three gains, stereo, single-frequency and 5000-5100 Hz range rejection passed.");
}
'''
(out/'spotify.cpp').write_text(fixture)
subprocess.run(['c++','-std=c++17','-O1','-fsanitize=address,undefined,float-cast-overflow',
                '-I',str(root/'include'),str(out/'spotify.cpp'),'-o',str(out/'spotify')],check=True)
subprocess.run([str(out/'spotify')],check=True)
