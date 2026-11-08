#include "../Source/ResonantEngine.h"
#include <iostream>
#include <limits>
#include <cstdlib>
#include <fstream>
#include <cstdint>
using namespace resonant;
static void check(bool b,const char* message) { if(!b) { std::cerr<<"FAIL: "<<message<<'\n'; std::exit(1); } }
static double energy(Controls c, float rate, bool compare, double& diff) {
    Engine a,b; c.rate=rate; a.setControls(c); a.prepare(48000); c.rate=9; b.setControls(c); b.prepare(48000);
    double e=0; diff=0;
    for(int i=0;i<96000;++i) { float x=.2f*std::sin(static_cast<float>(i)*.071f); auto y=a.process(x,x); auto z=b.process(x,x); e+=y[0]*y[0]; if(compare) diff+=std::abs(y[0]-z[0]); }
    return e;
}
int main() {
    size_t count=0; float peak=0;
    for(double sr : {44100.,48000.,96000.,192000.}) {
        for(int mode=0;mode<4;++mode) for(int mask=0;mask<32;++mask) {
            Engine e; Controls c; c.mode=mode; c.rate=(mask&1)?10.f:.1f; c.width=(mask&2)?1.f:0.f;
            c.tune=(mask&4)?1.f:0.f; c.feedback=(mask&8)?1.f:0.f; c.sweep=(mask&16)?1.f:0.f; c.outputDb=6;
            e.setControls(c); e.prepare(sr);
            const int n=static_cast<int>(sr*.22);
            for(int i=0;i<n;++i) {
                float x=i<n/2?4.f*std::sin(static_cast<float>(i)*.123f):0.f;
                if(i==11) x=std::numeric_limits<float>::infinity();
                if(i==13) x=std::numeric_limits<float>::quiet_NaN();
                auto y=e.process(x,-x);
                for(float v:y) { check(std::isfinite(v),"finite stress output"); check(std::abs(v)<=.980001f,"sample ceiling"); peak=std::max(peak,std::abs(v)); }
                check(std::abs(y[0]+y[1])<.00001f,"stereo isolation and symmetric processing"); ++count;
            }
        }
    }
    for(int m : {2,3}) { Controls c; c.mode=m; double diff; energy(c,.1f,true,diff); check(diff<1e-6,"Rate must be inactive in static modes"); }
    for(int m : {0,1}) { Controls c; c.mode=m; c.width=0; double diff; energy(c,.1f,true,diff); check(diff<1e-6,"zero Width must remove modulation"); c.width=1; energy(c,.1f,true,diff); check(diff>1,"modulated modes must respond to Rate"); }
    // Pitch cap: in modulated modes, |d(delay)/dt| never exceeds Width x 1 semitone,
    // across Rate, Tune, Width and Sweep extremes.
    for(int m : {0,1}) for(float rate : {.1f,1.8f,10.f}) for(float tune : {0.f,.5f,.98f,1.f})
        for(float width : {.35f,.85f,1.f}) for(float sweep : {0.f,1.f}) {
            Controls c; c.mode=m; c.rate=rate; c.tune=tune; c.width=width; c.sweep=sweep;
            const int N=20000; double prev=Engine::delayAt(c,m,0), worst=0;
            for(int i=1;i<=N;++i){ double d=Engine::delayAt(c,m,double(i)/N); worst=std::max(worst,std::abs(d-prev)*N*rate); prev=d; }
            check(worst<=width*Engine::maxPitchRatio*1.001,"pitch deviation capped at Width x 1 semitone");
        }
    // Impulse arrival tests cover mode-specific delay direction and distinct ranges.
    const auto arrival = [](int mode,float tune) {
        Engine e; Controls c; c.mode=mode;c.tune=tune;c.width=1;c.feedback=0;c.wetOnly=1;c.outputDb=0;
        e.setControls(c);e.prepare(48000);float peak=0;int at=0;
        for(int i=0;i<10000;++i){auto y=e.process(i==0?.2f:0,0);if(std::abs(y[0])>peak){peak=std::abs(y[0]);at=i;}}
        return at;
    };
    check(arrival(2,0)>arrival(2,1),"Matrix Tune clockwise shortens delay");
    check(arrival(3,0)<arrival(3,1),"Double Track Tune clockwise lengthens delay");
    check(arrival(3,1)>arrival(2,0)*4,"long and short delay families distinct");
    Engine a,b; Controls c; c.feedback=1; c.width=1; a.setControls(c); b.setControls(c); a.prepare(48000); b.prepare(48000);
    for(int i=0;i<48000*10;++i) {
        if(i%997==0) { c.mode=(i/997)%4; c.tune=(i%1994)?0:1; c.bypass=(i%2991)?0:1; a.setControls(c); b.setControls(c); }
        float x=.3f*std::sin(static_cast<float>(i)*.1f); auto y=a.process(x,x),z=b.process(x,x);
        check(y==z,"deterministic automation"); check(std::isfinite(y[0])&&std::abs(y[0])<=.980001f,"automation remains bounded");
    }
    c=Controls{}; c.bypass=1; a.setControls(c); a.prepare(48000);
    for(int i=0;i<10000;++i) check(std::abs(a.process(.2f,-.2f)[0]-.2f)<1e-7,"low-level bypass unity");
    c.bypass=0;c.feedback=1; a.setControls(c);a.prepare(48000);
    double tail=0;
    for(int i=0;i<48000*30;++i) { auto y=a.process(i<1000?.1f:0,0); check(std::isfinite(y[0])&&std::abs(y[0])<=.980001f,"30 second maximum feedback tail"); if(i>48000*29) tail+=y[0]*y[0]; }
    check(tail>1e-8,"feedback can sustain oscillation after excitation");
    a.reset();b.setControls(c);b.prepare(48000);
    for(int i=0;i<1000;++i) check(a.process(.1f,.1f)==b.process(.1f,.1f),"reset deterministic");
    std::cout<<"PASS: "<<count<<" stress frames; peak="<<peak<<"; static-mode semantics, automation, stereo, bypass, reset and sustained bounded feedback verified.\n";
}
