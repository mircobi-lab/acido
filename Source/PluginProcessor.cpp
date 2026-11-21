#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace acido;

AcidoProcessor::AcidoProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "ACIDO", createLayout()),
      midiMap (*this, "acido")
{
    // banchi per i profili dei controller: A BASS 1, B BASS 2, C livelli della batteria, D tempo, delay e master
    midiMap.setBanks ({ juce::StringArray { "B1CUT", "B1RES", "B1ENV", "B1DEC", "B1ACC", "B1DRIVE", "B1TUNE", "B1LEVEL" },
                        juce::StringArray { "B2CUT", "B2RES", "B2ENV", "B2DEC", "B2ACC", "B2DRIVE", "B2TUNE", "B2LEVEL" },
                        juce::StringArray { "BDLEV", "SDLEV", "CPLEV", "CHLEV", "OHLEV", "LTLEV", "RSLEV", "DRLEVEL" },
                        juce::StringArray { "BPM", "SWING", "DLFB", "DLRET", "B1SEND", "B2SEND", "AC", "MASTER" } });
    reader.attach (state);
}

bool AcidoProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void AcidoProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, juce::jmax (samplesPerBlock, 32));
}

void AcidoProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midiMap.process (midi);
    double bpm = 0, ppq = 0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto p = pos->getPpqPosition()) ppq = *p;
            playing = pos->getIsPlaying() && pos->getPpqPosition().hasValue() && bpm > 0;
        }
    if (panicRequested.exchange (false)) engine.panic();
    for (int c = getTotalNumInputChannels(); c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, buffer.getNumSamples());
    buffer.clear();
    engine.process (buffer, midi, reader, playing, bpm, ppq);
}

float AcidoProcessor::paramValue (P p) const
{
    return state.getRawParameterValue (paramDefs()[(size_t) p].key)->load();
}

void AcidoProcessor::setParamValue (P p, float value)
{
    if (auto* prm = state.getParameter (paramDefs()[(size_t) p].key))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (value));
        prm->endChangeGesture();
    }
}

void AcidoProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto st = state.copyState();
    for (auto name : { "MIDIMAP", "PATTERNS" }) st.removeChild (st.getChildWithName (name), nullptr);
    st.appendChild (midiMap.toValueTree(), nullptr);
    st.appendChild (patterns.toValueTree(), nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}

void AcidoProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (! xml || ! xml->hasTagName (state.state.getType())) return;
    auto st = juce::ValueTree::fromXml (*xml);
    if (auto mm = st.getChildWithName ("MIDIMAP"); mm.isValid())
    {
        midiMap.fromValueTree (mm);
        st.removeChild (mm, nullptr);
    }
    if (auto pt = st.getChildWithName ("PATTERNS"); pt.isValid())
    {
        patterns.fromValueTree (pt);
        st.removeChild (pt, nullptr);
    }
    state.replaceState (st);
}

juce::AudioProcessorEditor* AcidoProcessor::createEditor() { return new AcidoEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AcidoProcessor(); }
