#pragma once
#include "MBStudioMidi.h"
#include "Engine.h"

class AcidoProcessor : public juce::AudioProcessor
{
public:
    AcidoProcessor();
    ~AcidoProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "acido"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // parametri: comodità per il pannello (thread dei messaggi)
    float paramValue (acido::P p) const;
    void setParamValue (acido::P p, float value);

    juce::AudioProcessorValueTreeState state;
    acido::PatternStore patterns;
    acido::Engine engine { patterns };
    mbstudio::midi::MidiMapper midiMap;               // MIDI Learn e profili controller
    void requestPanic() noexcept { panicRequested = true; }   // MIDI PANIC, eseguito all'inizio del blocco audio

private:
    acido::ParamReader reader;
    std::atomic<bool> panicRequested { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AcidoProcessor)
};
