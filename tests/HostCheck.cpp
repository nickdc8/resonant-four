#include <JuceHeader.h>
#include <iostream>
#include <cstdlib>
static void require(bool ok,const char* s) { if(!ok){std::cerr<<"FAIL: "<<s<<'\n';std::exit(1);} }
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    require(argc>=2,"VST3 path argument");
    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> found;
    format.findAllTypesForFile(found,argv[1]);
    require(found.size()==1,"scan exactly one VST3");
    juce::String error;
    auto p=format.createInstanceFromDescription(*found[0],48000,512,error);
    if(!p) std::cerr<<error<<'\n';
    require(p!=nullptr,"load actual built VST3");
    require(p->getNumPrograms()==3,"three named presets");
    juce::MemoryBlock saved;
    for(int preset=0;preset<p->getNumPrograms();++preset) {
        p->setCurrentProgram(preset);
        p->getStateInformation(saved);
        require(saved.getSize()>0,"state data exists");
        auto q=format.createInstanceFromDescription(*found[0],48000,512,error);
        require(q!=nullptr,"second instance");
        q->setStateInformation(saved.getData(),static_cast<int>(saved.getSize()));
        require(q->getCurrentProgram()==preset,"program state restored");
        require(q->getParameters().size()==p->getParameters().size(),"parameter counts");
        for(int j=0;j<p->getParameters().size();++j) require(std::abs(p->getParameters()[j]->getValue()-q->getParameters()[j]->getValue())<1e-6,"parameter state restored");
        for(int channels : {1,2}) {
            auto layout=p->getBusesLayout();
            layout.inputBuses.set(0,channels==1?juce::AudioChannelSet::mono():juce::AudioChannelSet::stereo());
            layout.outputBuses.set(0,layout.inputBuses[0]);
            require(p->setBusesLayout(layout)&&q->setBusesLayout(layout),"mono/stereo bus support");
            p->prepareToPlay(48000,512);q->prepareToPlay(48000,512);
            p->setProcessingPrecision(juce::AudioProcessor::singlePrecision); q->setProcessingPrecision(juce::AudioProcessor::singlePrecision);
            double energy=0;
            int sample=0;
            for(int block=0;block<300;++block) {
                int n=std::array<int,5>{1,31,64,257,512}[static_cast<size_t>(block%5)];
                juce::AudioBuffer<float> a(channels,n),b(channels,n);juce::MidiBuffer midi;
                for(int c=0;c<channels;++c) for(int i=0;i<n;++i) a.setSample(c,i,.2f*std::sin(static_cast<float>(sample+i)*.04f));
                b.makeCopyOf(a);p->processBlock(a,midi);q->processBlock(b,midi);sample+=n;
                for(int c=0;c<channels;++c) for(int i=0;i<n;++i){float x=a.getSample(c,i); require(std::isfinite(x)&&std::abs(x)<=.980001f,"host render finite and bounded");require(std::abs(x-b.getSample(c,i))<1e-6,"restored instance renders identically");energy+=x*x;}
            }
            require(energy>.01,"preset produces signal");p->releaseResources();q->releaseResources();
        }
        std::cout<<"PASS preset/state/render: "<<p->getProgramName(preset)<<'\n';
    }
    // Explicit stereo mismatch must be rejected.
    auto bad=p->getBusesLayout();bad.inputBuses.set(0,juce::AudioChannelSet::mono());bad.outputBuses.set(0,juce::AudioChannelSet::stereo());
    require(!p->checkBusesLayoutSupported(bad),"mismatched buses rejected");
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(p->createEditorAndMakeActive());
        require(editor!=nullptr,"editor creates");
        auto preview=editor->createComponentSnapshot(editor->getLocalBounds(),true,1.f);
        if(argc>2){juce::File out(argv[2]);auto stream=out.createOutputStream();require(stream!=nullptr,"preview opens");juce::PNGImageFormat png;require(png.writeImageToStream(preview,*stream),"editor preview renders");}
    }
    std::cout<<"PASS: actual VST3 scan, load, presets, state recall, mono/stereo variable-block rendering, editor and bus rejection.\n";
}
