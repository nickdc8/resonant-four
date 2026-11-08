#include "PluginProcessor.h"

juce::AudioProcessorValueTreeState::ParameterLayout ResonantFourProcessor::layout() {
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    p.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"mode",1}, "Mode", juce::StringArray{"Flange","Chorus","Filter Matrix","Double Track"},0));
    juce::NormalisableRange<float> rate(.1f,10.f,.001f); rate.setSkewForCentre(1.f);
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"rate",1},"Rate (Hz)",rate,.55f));
    for (auto v : std::array<std::pair<const char*,float>,3>{{{"width",35.f},{"tune",90.f},{"feedback",50.f}}})
        p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{v.first,1},juce::String(v.first).substring(0,1).toUpperCase()+juce::String(v.first).substring(1)+" (%)",juce::NormalisableRange<float>(0,100,.01f),v.second));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"sweep",1},"Sweep Filter (approximation)",false));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"wetOnly",1},"Blend: wet only",false));
    p.add(std::make_unique<juce::AudioParameterBool>(juce::ParameterID{"bypass",1},"Bypass",false));
    p.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"output",1},"Output (dB)",juce::NormalisableRange<float>(-24,6,.01f),-6.f));
    return p;
}
ResonantFourProcessor::ResonantFourProcessor()
 : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),
 parameters(*this,nullptr,"ResonantFourState",layout()) {
    const std::array<const char*,9> ids {"mode","rate","width","tune","feedback","sweep","wetOnly","bypass","output"};
    for(size_t i=0;i<ids.size();++i) values[i]=parameters.getRawParameterValue(ids[i]);
}
resonant::Controls ResonantFourProcessor::controls() const {
    resonant::Controls c;
    c.mode=static_cast<int>(values[0]->load()); c.rate=values[1]->load();
    c.width=values[2]->load()*.01f; c.tune=values[3]->load()*.01f; c.feedback=values[4]->load()*.01f;
    c.sweep=values[5]->load(); c.wetOnly=values[6]->load(); c.bypass=values[7]->load(); c.outputDb=values[8]->load();
    return c;
}
void ResonantFourProcessor::prepareToPlay(double sr,int) { engine.setControls(controls()); engine.prepare(sr); }
bool ResonantFourProcessor::isBusesLayoutSupported(const BusesLayout& b) const {
    return b.getMainInputChannelSet()==b.getMainOutputChannelSet() &&
        (b.getMainOutputChannelSet()==juce::AudioChannelSet::mono() || b.getMainOutputChannelSet()==juce::AudioChannelSet::stereo());
}
void ResonantFourProcessor::processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    engine.setControls(controls());
    if(b.getNumChannels()==0) return;
    auto* l=b.getWritePointer(0); auto* r=b.getNumChannels()>1?b.getWritePointer(1):nullptr;
    for(int i=0;i<b.getNumSamples();++i) { auto out=engine.process(l[i],r?r[i]:l[i]); l[i]=out[0]; if(r) r[i]=out[1]; }
}
namespace {
struct FactoryPreset { const char* name; std::array<float,9> values; };
// Values: mode, rate Hz, width %, tune %, feedback %, sweep, wetOnly, bypass, output dB.
// Every number is an estimated voicing; see research/RESEARCH.md for provenance.
// RFUS: the long-Tune reading of Kurt's sketch, chosen by A/B listening against the
// record's intro over short-ring and flange variants.
const std::array<FactoryPreset,3> factoryPresets {{
    {"Heart-Shaped Box (inspired)",             {0, .55f, 35, 90, 50, 0, 0, 0, -6}},
    {"Radio Friendly Unit Shifter (inspired)",  {1, 1.8f, 85, 98, 80, 0, 0, 0, -6}},
    {"Scentless Apprentice (inspired)",         {3, 1.8f, 85, 98, 80, 0, 0, 0, -6}},
}};
}
int ResonantFourProcessor::getNumPrograms() { return static_cast<int>(factoryPresets.size()); }
const juce::String ResonantFourProcessor::getProgramName(int i) {
    return factoryPresets[static_cast<size_t>(juce::jlimit(0,getNumPrograms()-1,i))].name;
}
void ResonantFourProcessor::setCurrentProgram(int i) {
    i=juce::jlimit(0,getNumPrograms()-1,i); program.store(i);
    const std::array<const char*,9> ids {"mode","rate","width","tune","feedback","sweep","wetOnly","bypass","output"};
    const auto& a=factoryPresets[static_cast<size_t>(i)].values;
    for(size_t n=0;n<ids.size();++n) if(auto* p=parameters.getParameter(ids[n])) p->setValueNotifyingHost(p->convertTo0to1(a[n]));
}
void ResonantFourProcessor::getStateInformation(juce::MemoryBlock& b) {
    auto state=parameters.copyState(); state.setProperty("program",program.load(),nullptr);
    if(auto xml=state.createXml()) copyXmlToBinary(*xml,b);
}
void ResonantFourProcessor::setStateInformation(const void* data,int bytes) {
    if(auto xml=getXmlFromBinary(data,bytes)) if(xml->hasTagName(parameters.state.getType())) {
        auto state=juce::ValueTree::fromXml(*xml);
        program.store(juce::jlimit(0,getNumPrograms()-1,static_cast<int>(state.getProperty("program",0))));
        parameters.replaceState(state);
    }
}
// Host-native parameter widgets retain keyboard and accessibility support.
// A compact preset strip adds explicit provenance without hiding parameters.
class ResonantEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit ResonantEditor(ResonantFourProcessor& p) : AudioProcessorEditor(p), owner(p), generic(p) {
        addAndMakeVisible(generic); addAndMakeVisible(presets); addAndMakeVisible(reload);
        for(int i=0;i<p.getNumPrograms();++i) presets.addItem(p.getProgramName(i),i+1);
        presets.setSelectedId(p.getCurrentProgram()+1,juce::dontSendNotification);
        presets.onChange=[this]{owner.setCurrentProgram(presets.getSelectedId()-1);};
        reload.setButtonText("Reload preset"); reload.onClick=[this]{owner.setCurrentProgram(presets.getSelectedId()-1);};
        setSize(690,540); startTimerHz(4);
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff171c1a)); g.setColour(juce::Colour(0xffbcdf9f));
        g.setFont(juce::FontOptions(28.f,juce::Font::bold)); g.drawText("RESONANT FOUR",24,14,640,40,juce::Justification::left);
        g.setFont(juce::FontOptions(14.f)); g.setColour(juce::Colours::whitesmoke);
        g.drawText("EchoFlanger / PolyChorus-inspired  |  Independent Audio",24,56,640,24,juce::Justification::left);
        g.setFont(juce::FontOptions(13.f));
        g.drawText("All song settings are estimated voicings. Sweep Filter is a modern approximation.",24,121,645,23,juce::Justification::left);
        g.drawText("Rate is inactive in Matrix / Double Track. Width also changes Tune range.",24,479,645,22,juce::Justification::left);
        g.drawText("Feedback above 87% can sustain oscillation. Output protection is always active.",24,503,645,22,juce::Justification::left);
    }
    void resized() override { presets.setBounds(24,88,468,28); reload.setBounds(506,88,160,28); generic.setBounds(12,153,666,315); }
private:
    void timerCallback() override { presets.setSelectedId(owner.getCurrentProgram()+1,juce::dontSendNotification); }
    ResonantFourProcessor& owner;
    juce::GenericAudioProcessorEditor generic;
    juce::ComboBox presets;
    juce::TextButton reload;
};
juce::AudioProcessorEditor* ResonantFourProcessor::createEditor() { return new ResonantEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ResonantFourProcessor; }
