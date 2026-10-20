#include "audio_suite.h"
#include "audio_controls.h"
#include "audio_write.h"
#include <cassert>
#include <cstring>
#include <iomanip>
#include <fstream>
#include <iostream>
#include <vector>
using namespace audio_test;

int main(int argc,char**argv) {
    assert(argc==2 || argc==3);
    if(argc==3 && std::strcmp(argv[2],"--sheet")==0) {
        std::cout<<"case_id,name,waveform,frequency_hz,source_peak_dbfs,gain_db,curve_index,sample_rate,slot_bits,channel,eq,trim_db,distortion_0_to_3,static_0_to_3,supply,capacitors,output_errors,notes\n";
        const char* waves[]={"zero","stopped","sine","music","multitone","music transitions"};
        for(int i=0;i<kCaseCount;++i) {
            const Case& c=kCases[i];
            float gain=1;
            if(c.mapping==Mapping::Radio || c.mapping==Mapping::MatchedDb) gain=radioGain(c.index);
            if(c.mapping==Mapping::Spotify) gain=spotifyGain(c.index);
            const float gainDb=20*std::log10(gain)+(c.eqReference?-12:0);
            std::cout<<i+1<<','<<c.name<<','<<waves[static_cast<int>(c.wave)]<<',';
            if(c.wave==Wave::Sine) std::cout<<c.frequency;
            if(c.wave==Wave::Multi) std::cout<<"250+1000+4000";
            std::cout<<',';
            if(c.wave==Wave::Silence || c.wave==Wave::Stopped) std::cout<<"-inf";
            else std::cout<<std::fixed<<std::setprecision(5)<<c.peakDb;
            std::cout<<',';
            if(c.wave==Wave::Transition) std::cout<<"indices15/16/17/16 each12s";
            else std::cout<<gainDb;
            std::cout<<',';
            if(c.mapping!=Mapping::Direct) std::cout<<c.index;
            std::cout<<','<<c.rate<<','<<c.bits<<','<<(c.channel==0?"both":c.channel==1?"left":"right")<<',';
            if(c.eqBand<0) std::cout<<"bypass";
            else std::cout<<"peaking band"<<c.eqBand<<" "<<c.eqDb<<"dB";
            std::cout<<",0,,,,,pending,\n";
        }
        return 0;
    }
    std::ifstream in(argv[1],std::ios::binary|std::ios::ate);
    assert(in.good()); const size_t bytes=in.tellg();in.seekg(0);
    std::vector<int16_t> pcm(bytes/2);in.read(reinterpret_cast<char*>(pcm.data()),bytes);
    assert(in.good() && pcm.size()%2==0 && pcm.size()<44100*12*2);
    int peak=0;double dc[2]{};
    for (size_t i=0;i<pcm.size();++i) { peak=std::max(peak,std::abs(static_cast<int>(pcm[i])));dc[i%2]+=pcm[i]; }
    assert(peak==8231 && pcm[0]==0 && pcm[1]==0 && pcm[pcm.size()-1]==0);
    for (double v:dc) assert(std::abs(v/(pcm.size()/2))<1);
    for (int v=-32768;v<=32767;++v) {
        const int32_t wide=pack32(static_cast<int16_t>(v));
        assert(wide/65536==v && (static_cast<uint32_t>(wide)&65535)==0);
    }
    assert(radioGain(0)==0 && radioGain(21)==1);
    assert(std::abs(radioDb(11)+46.874927)<.001 && std::abs(radioDb(16)+20.729004)<.002);
    for(int i=0;i<=21;++i) {
        assert(spotifyGain(i)==(kCurve[i]*255/55)/255.f);
        if(i) assert(radioGain(i)>=radioGain(i-1));
    }
    // Render each actual suite case over loop boundaries and both EQ sample rates.
    // Includes inactive-channel exact zeros, phase/frequency periods and peak headroom.
    for (int id=1;id<=kCaseCount;++id) {
        Synth s;s.configure(kCases[id-1],pcm.data(),pcm.size()/2);
        const Case& c=s.test;
        const int frames=(c.wave==Wave::Music ? static_cast<int>(pcm.size()/2)+2000 : c.rate*2);
        int16_t l=0,r=0;
        for(int i=0;i<frames;++i) {
            if(i%kBlock==0) s.beginBlock();
            s.next(l,r);
            if(c.channel==1) assert(r==0);
            if(c.channel==2) assert(l==0);
            if(c.wave==Wave::Silence || c.wave==Wave::Stopped) assert(!l&&!r);
        }
        assert(!s.clips);
        if(c.wave==Wave::Multi && c.eqBand<0) {
            const float target=32767*dbGain(c.peakDb)*(c.eqReference?dbGain(-12):1);
            assert(std::abs(s.peak-target)<=1);
        }
        if(c.wave==Wave::Sine && c.frequency!=100) {
            const double expected=32767*dbGain(c.peakDb);
            assert(std::abs(s.peak-expected)<=4);
        }
        std::cout<<"case "<<id<<" peak="<<s.peak<<" clips="<<s.clips<<"\n";
    }
    for (int rate:{44100,48000}) {
        for(int hz:{250,1000,4000}) {
            double energy=0;int crossings=0;float previous=0;
            for(int i=0;i<rate;++i) {
                float v=sine(i,hz,rate); energy+=v*v;
                if(previous<=0 && v>0) ++crossings; previous=v;
                assert(std::abs(v-sine(i+rate,hz,rate))<1e-7);
            }
            assert(crossings==hz && std::abs(energy/rate-.5)<.0001);
        }
        for(int band=0;band<3;++band) for(int db:{-6,0,6}) {
            Eq eq;eq.configure(band,db,rate);
            const int hz[]={250,1000,4000}; double ei=0,eo=0;
            for(int i=0;i<rate*2;++i) {
                const float x=sine(i,hz[band],rate)*.1f,y=eq.tick(x,0);
                if(!db) assert(x==y);
                if(i>=rate) {ei+=x*x;eo+=y*y;}
            }
            assert(std::abs(10*std::log10(eo/ei)-db)<.015);
        }
    }
    // True bypass and the real flat-filter path are sample-exact.
    Synth bypass,flat;bypass.configure(kCases[47],pcm.data(),pcm.size()/2);flat.configure(kCases[48],pcm.data(),pcm.size()/2);
    for(int i=0;i<88200;++i) { int16_t a,b,c,d;bypass.next(a,b);flat.next(c,d);assert(a==c&&b==d); }
    // Radio and explicit direct-dB routes use the exact same source sequence/gain.
    for(int j=0;j<7;++j) {
        Synth a,b;a.configure(kCases[24+j],pcm.data(),pcm.size()/2);b.configure(kCases[31+j],pcm.data(),pcm.size()/2);
        for(int i=0;i<44100;++i) { int16_t l,r,x,y;a.next(l,r);b.next(x,y);assert(l==x&&r==y); }
    }
    for(int pair:{19,21}) {
        Synth a,b;a.configure(kCases[pair],pcm.data(),pcm.size()/2);b.configure(kCases[pair+1],pcm.data(),pcm.size()/2);
        for(int i=0;i<96000;++i) {int16_t l,r,x,y;a.next(l,r);b.next(x,y);assert(l==x&&r==y&&pack32(l)/65536==x);}
    }
    // Ordinary output mute/attenuation consumes samples without restarting source phase.
    Synth reference,controlled;
    reference.configure(kCases[2],pcm.data(),pcm.size()/2);
    controlled.configure(kCases[2],pcm.data(),pcm.size()/2);
    for(int i=0;i<88200;++i) {
        int16_t l,r,x,y;
        reference.next(l,r);controlled.next(x,y,(i>=20000&&i<30000)?0.f:1.f);
        assert(reference.frame==controlled.frame);
        if(i>=20000&&i<30000) assert(!x&&!y);else assert(l==x&&r==y);
    }
    // Transition is selected at the first buffer boundary at/after 12 s. It never resets phase.
    for(int id:{46,47}) {
        Synth s;s.configure(kCases[id-1],pcm.data(),pcm.size()/2);
        for(uint64_t i=0;i<44100*48ULL;i+=kBlock) {
            s.frame=i;s.beginBlock();
            const int indices[]={15,16,17,16};
            assert(s.gain.target==radioGain(indices[(i/(44100*12ULL))%4]));
            assert(s.gain.left<=(id==46?0:1764));
        }
    }
    Ramp ramp;ramp.set(1,1764);for(int i=0;i<1764;++i) ramp.tick();assert(ramp.value==1&&!ramp.left);
    for(int id=1;id<=kCaseCount;++id) {
        const uint32_t n=cueFrames(id,44100);assert(n>=44100);
        for(uint32_t i=0;i<n;++i) assert(std::abs(static_cast<int>(cueSample(i,id,44100)))<=520);
        assert(cueSample(0,id,44100)==0 && cueSample(n-1,id,44100)==0);
    }
    // Simulated driver accepts a prefix on timeout; verify no prefix is replayed or lost.
    const uint8_t data[32]{}; size_t calls=0,total=0;
    bool ok=writeAll(data,32,4,[&](const uint8_t* p,size_t remaining) {
        assert(p==data+total && remaining==32-total);++calls;
        const size_t n= calls==1?8: calls==2?0:remaining;
        return WriteResult{n,calls<=2?1:0,true};
    },[&](size_t n,bool){total+=n;});
    assert(ok&&total==32&&calls==3);
    calls=0;assert(!writeAll(data,32,4,[&](const uint8_t*,size_t){++calls;return WriteResult{0,1,true};},[](size_t,bool){}));assert(calls==3);
    assert(!writeAll(data,32,4,[](const uint8_t*,size_t){return WriteResult{3,0,false};},[](size_t,bool){assert(false);}));
    assert(!writeAll(data,32,4,[](const uint8_t*,size_t){return WriteResult{36,0,false};},[](size_t,bool){assert(false);}));
    assert(!writeAll(data,32,4,[](const uint8_t*,size_t){return WriteResult{4,9,false};},[](size_t,bool){}));
    // Stopped-only UART snapshots use the same accepted-prefix logic with byte
    // alignment, bounded FIFO chunks and explicit failure after partial output.
    const uint8_t statusBytes[400]{};size_t statusTotal=0,statusCalls=0;
    assert(writeAll(statusBytes,sizeof(statusBytes),1,[&](const uint8_t* ptr,size_t remain) {
        assert(ptr==statusBytes+statusTotal&&remain==sizeof(statusBytes)-statusTotal);
        ++statusCalls;return WriteResult{std::min(remain,size_t{64}),0,false};
    },[&](size_t n,bool){statusTotal+=n;}));
    assert(statusTotal==400&&statusCalls==7);
    statusTotal=0;
    assert(!writeAll(statusBytes,sizeof(statusBytes),1,[](const uint8_t*,size_t) {
        return WriteResult{17,1,false};
    },[&](size_t n,bool){statusTotal+=n;}));assert(statusTotal==17);
    Controls c;uint64_t time=0;
    auto poll=[&](uint64_t dt,bool pressed,uint8_t ab=3){time+=dt;return c.poll(time,pressed,ab);};
    poll(1000,true);assert(!poll(19000,true).muted);assert(poll(1000,true).muted);
    poll(20000,false);assert(poll(20000,false).muted); // short click mutes
    poll(1000,true);poll(20000,true);poll(20000,false);assert(!poll(20000,false).muted);
    poll(1000,true);poll(20000,true);poll(1500000,true);poll(1,false);
    auto request=poll(20000,false);assert(request.id==2&&!request.muted&&request.epoch==2);
    poll(1000,true);poll(20000,true);poll(4000000,true);poll(1,false);
    request=poll(20000,false);assert(request.id==2&&!request.muted&&request.epoch==3);
    poll(1000,true);poll(20000,true);
    for(int ab:{1,0,2,3}) poll(1000,true,ab);
    assert(c.state.id==2&&c.state.muted);poll(1000,false);request=poll(20000,false);
    assert(request.id==3&&request.epoch==4&&!request.muted);
    for(int i=0;i<70;++i) for(int ab:{2,0,1,3}) poll(1000,false,ab);
    assert(c.state.trimDb==-60); // bounded and turning never unmutes
    // A held press fires only on release; exact boundaries and bounce are deterministic.
    for(uint64_t held:{1499999ULL,1500000ULL,3999999ULL,4000000ULL}) {
        Controls hold;
        hold.poll(0,true,3);hold.poll(20000,true,3);
        hold.poll(20000+held-20000,false,3);
        const auto end=hold.poll(20000+held,false,3);
        assert(end.id==((held>=1500000&&held<4000000)?2:1));
        assert(end.epoch==(held>=1500000?2:1));
        for(int i=1;i<20;++i) assert(hold.poll(20000+held+i*1000,false,3).epoch==end.epoch);
    }
    Controls bounce;
    for(int i=0;i<20;++i) bounce.poll(i*1000,i%2,3);
    assert(bounce.state.epoch==1&&!bounce.state.muted);
    // Hands-free holds begin after matching cue acknowledgment, stop at the end,
    // never advance on a stale epoch or fault, and keep music attenuated.
    HandsFree automatic;
    auto scheduled=automatic.poll(100000000,1,1,false,false);assert(scheduled.id==1&&!automatic.started);
    scheduled=automatic.poll(100000000,1,0,true,false);assert(scheduled.id==1&&!automatic.started);
    uint64_t now=100000000;
    constexpr uint64_t holds[]={10000000,10000000,15000000,30000000};
    constexpr int sequence[]={1,2,3,4,2};
    for(int stage=0;stage<4;++stage) {
        scheduled=automatic.poll(now,sequence[stage],stage+1,true,false);
        assert(scheduled.id==sequence[stage]);
        assert(automatic.poll(now+holds[stage]-1,sequence[stage],stage+1,true,false).id==sequence[stage]);
        assert(automatic.poll(now+holds[stage],sequence[stage],stage+1,true,true).id==sequence[stage]);
        scheduled=automatic.poll(now+holds[stage],sequence[stage],stage+1,true,false);
        assert(scheduled.id==sequence[stage+1]&&scheduled.epoch==static_cast<uint32_t>(stage+2));
        assert(scheduled.trimDb==(stage==2?-12:0));
        now+=holds[stage]+2000000;
    }
    assert(automatic.poll(now+1000000000,2,5,true,false).id==2&&automatic.stage==4);
    // Matched music format profile: correct epoch gating, unchanged PCM and gain,
    // 16/32/16 A/B/A, stage cues, and a final indefinite clock-stopped case.
    MusicFormats formats;
    assert(formats.poll(0,4,0,true,false).epoch==1&&!formats.started);
    assert(formats.poll(0,4,1,false,false).epoch==1&&!formats.started);
    Request first=formats.state;
    Request requests[3];
    uint64_t stamp=100;
    for(int stage=0;stage<3;++stage) {
        requests[stage]=formats.state;
        assert(requests[stage].id==4&&requests[stage].trimDb==-12&&requests[stage].cueId==stage+1);
        assert(selectedCase(requests[stage]).bits==(stage==1?32:16));
        formats.poll(stamp,4,stage+1,true,false);
        assert(formats.poll(stamp+29999999,4,stage+1,true,false).epoch==static_cast<uint32_t>(stage+1));
        assert(formats.poll(stamp+30000000,4,stage+1,true,true).epoch==static_cast<uint32_t>(stage+1));
        assert(formats.poll(stamp+30000000,4,stage+1,true,false).epoch==static_cast<uint32_t>(stage+2));
        stamp+=32000000;
    }
    assert(first.slotBits==16&&formats.state.id==2&&formats.state.cueId==4);
    assert(formats.poll(stamp+999999999,2,4,true,false).epoch==4);
    Synth music16,music32,musicAgain;
    music16.configure(selectedCase(requests[0]),pcm.data(),pcm.size()/2);
    music32.configure(selectedCase(requests[1]),pcm.data(),pcm.size()/2);
    musicAgain.configure(selectedCase(requests[2]),pcm.data(),pcm.size()/2);
    Ramp startEnvelope;startEnvelope.set(dbGain(-12),44100*30/1000);
    for(size_t i=0;i<pcm.size()/2*2;++i) {
        if(i%kBlock==0) {music16.beginBlock();music32.beginBlock();musicAgain.beginBlock();}
        int16_t l,r,x,y,a,b;const float e=startEnvelope.tick();
        music16.next(l,r,e);music32.next(x,y,e);musicAgain.next(a,b,e);
        assert(l==x&&r==y&&l==a&&r==b);
        assert(pack32(x)/65536==l&&pack32(y)/65536==r);
    }
    assert(!music16.clips&&!music32.clips&&music16.peak==2068&&music32.peak==2068);
    // Requested volume 18/21 uses the corrected radio mapping in every stage,
    // replacing the previous trim rather than accidentally stacking attenuation.
    MusicFormats radio18(18);
    Request high[3];
    for(int stage=0;stage<3;++stage) {
        high[stage]=radio18.state;
        const Case test=selectedCase(high[stage]);
        assert(high[stage].trimDb==0&&test.mapping==Mapping::Radio&&test.index==18);
        assert(test.bits==(stage==1?32:16));
        const uint64_t start=stage*32000000ULL;
        radio18.poll(start,4,stage+1,true,false);
        radio18.poll(start+30000000,4,stage+1,true,false);
    }
    assert(radio18.state.id==2&&radio18.state.radioIndex==-1);
    assert(std::abs(radioDb(18)+9.57425094f)<.0001f);
    Synth high16,high32,direct18;
    high16.configure(selectedCase(high[0]),pcm.data(),pcm.size()/2);
    high32.configure(selectedCase(high[1]),pcm.data(),pcm.size()/2);
    direct18.configure(kCases[3],pcm.data(),pcm.size()/2);
    for(size_t i=0;i<pcm.size()/2*2;++i) {
        if(i%kBlock==0) {high16.beginBlock();high32.beginBlock();direct18.beginBlock();}
        int16_t l,r,x,y,a,b;high16.next(l,r);high32.next(x,y);direct18.next(a,b,radioGain(18));
        assert(l==x&&r==y&&l==a&&r==b&&pack32(x)/65536==l);
    }
    assert(!high16.clips&&!high32.clips&&high16.peak==2734&&high32.peak==2734);
    // Channel-load music uses the same waveform, phase, rate and per-channel
    // gain on each active output. Inactive samples are exact zeros, not reduced gain.
    MusicChannels channelProfile;
    Request channelRequests[3];
    assert(channelProfile.poll(0,4,0,true,false).epoch==1&&!channelProfile.started);
    assert(channelProfile.poll(0,4,1,false,false).epoch==1&&!channelProfile.started);
    constexpr int channelOrder[]={1,2,0};
    for(int stage=0;stage<3;++stage) {
        channelRequests[stage]=channelProfile.state;
        const Case c=selectedCase(channelRequests[stage]);
        assert(c.channel==channelOrder[stage]&&c.monoLeft&&c.bits==16&&c.rate==44100);
        assert(c.mapping==Mapping::Radio&&c.index==18&&channelRequests[stage].trimDb==0);
        const uint64_t start=stage*32000000ULL;
        channelProfile.poll(start,4,stage+1,true,false);
        assert(channelProfile.poll(start+29999999,4,stage+1,true,false).epoch==static_cast<uint32_t>(stage+1));
        assert(channelProfile.poll(start+30000000,4,stage+1,true,true).epoch==static_cast<uint32_t>(stage+1));
        channelProfile.poll(start+30000000,4,stage+1,true,false);
    }
    assert(channelProfile.state.id==2&&channelProfile.state.cueId==4&&!channelProfile.state.monoLeft);
    assert(channelProfile.poll(999999999,2,4,true,false).epoch==4);
    Synth onlyLeft,onlyRight,bothChannels;
    onlyLeft.configure(selectedCase(channelRequests[0]),pcm.data(),pcm.size()/2);
    onlyRight.configure(selectedCase(channelRequests[1]),pcm.data(),pcm.size()/2);
    bothChannels.configure(selectedCase(channelRequests[2]),pcm.data(),pcm.size()/2);
    Ramp channelEnvelope;channelEnvelope.set(1,44100*30/1000);
    for(size_t i=0;i<pcm.size()/2*2;++i) {
        if(i%kBlock==0) {onlyLeft.beginBlock();onlyRight.beginBlock();bothChannels.beginBlock();}
        int16_t l,r,x,y,a,b;const float e=channelEnvelope.tick();
        onlyLeft.next(l,r,e);onlyRight.next(x,y,e);bothChannels.next(a,b,e);
        assert(r==0&&x==0&&l==y&&l==a&&l==b);
    }
    assert(!onlyLeft.clips&&!onlyRight.clips&&!bothChannels.clips);
    assert(onlyLeft.peak==onlyRight.peak&&onlyLeft.peak==bothChannels.peak);
    std::cout<<"PASS: fixture, all 55 cases, EQ, clocks/PCM equivalence, gain, partial writes, debounce, controls and hands-free scheduling and matched music formats and isolated music channels at radio index 18\n";
}
