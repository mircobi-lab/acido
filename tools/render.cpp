/*  Strumento di prova (non fa parte del plugin): suona gli 8 pattern di fabbrica col clock interno,
    salva un WAV per pattern e stampa picco, RMS, valori non validi e carico della CPU.
    uso: acido_render CARTELLA_USCITA [battute]                                                       */
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine.h"
#include <chrono>

namespace
{
struct Dummy : juce::AudioProcessor
{
    Dummy() : AudioProcessor (BusesProperties().withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
              state (*this, nullptr, "ACIDO", acido::createLayout()) {}
    const juce::String getName() const override { return "render"; }
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override { return 0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}
    juce::AudioProcessorValueTreeState state;
};

void setParam (Dummy& d, const char* id, float v)
{
    auto* p = d.state.getParameter (id);
    p->setValueNotifyingHost (p->convertTo0to1 (v));
}
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File outDir (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]) : juce::File::getCurrentWorkingDirectory());
    const int bars = argc > 2 ? juce::String (argv[2]).getIntValue() : 4;
    const juce::String solo = argc > 3 ? juce::String (argv[3]) : juce::String();   // bass1 | bass2 | drums
    outDir.createDirectory();

    const double sr = 48000; const int block = 256;
    Dummy dummy;
    acido::ParamReader reader; reader.attach (dummy.state);
    acido::PatternStore store;
    juce::WavAudioFormat wav;
    bool allOk = true;

    for (int p = 1; p <= acido::numPatterns; ++p)
    {
        acido::Engine eng (store);
        eng.prepare (sr, block);
        setParam (dummy, "PATTERN", (float) p);
        setParam (dummy, "RUN", 1);
        if (solo.isNotEmpty())
        {
            setParam (dummy, "B1MUTE", solo == "bass1" ? 0.f : 1.f);
            setParam (dummy, "B2MUTE", solo == "bass2" ? 0.f : 1.f);
            setParam (dummy, "DRMUTE", solo == "drums" ? 0.f : 1.f);
        }
        const double bpm = reader[acido::BPM];
        const int total = (int) (bars * 16 * 15.0 / bpm * sr);
        juce::AudioBuffer<float> all (2, total);
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        double busy = 0; int bad = 0;
        for (int pos = 0; pos < total; pos += block)
        {
            const int len = juce::jmin (block, total - pos);
            buf.setSize (2, len, false, false, true);
            buf.clear();
            const auto t0 = std::chrono::steady_clock::now();
            eng.process (buf, midi, reader, false, 0, 0);
            busy += std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            for (int c = 0; c < 2; ++c)
            {
                for (int i = 0; i < len; ++i) if (! std::isfinite (buf.getSample (c, i))) ++bad;
                all.copyFrom (c, pos, buf, c, 0, len);
            }
        }
        const float peak = all.getMagnitude (0, total);
        const float rms = .5f * (all.getRMSLevel (0, 0, total) + all.getRMSLevel (1, 0, total));
        const double load = busy / (total / sr) * 100.0;
        std::printf ("pattern %d  picco %.1f dBFS  RMS %.1f dBFS  non validi %d  CPU %.2f%%\n", p,
                     juce::Decibels::gainToDecibels (peak, -120.f), juce::Decibels::gainToDecibels (rms, -120.f), bad, load);
        if (bad > 0 || peak > 1.0f || (solo.isEmpty() && peak < 1e-3f)) allOk = false;
        auto f = outDir.getChildFile ("pattern_" + juce::String (p) + ".wav");
        f.deleteFile();
        if (auto os = std::unique_ptr<juce::OutputStream> (f.createOutputStream()))
            if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (os.get(), sr, 2, 24, {}, 0)))
            {
                os.release();
                w->writeFromAudioSampleBuffer (all, 0, total);
            }
        setParam (dummy, "RUN", 0);
    }
    // prova 2: clock della DAW (120 BPM, partenza a battuta 2) e MIDI (canale 1 basso, canale 10 cassa)
    {
        acido::Engine eng (store);
        eng.prepare (sr, block);
        setParam (dummy, "PATTERN", 1.f);
        juce::AudioBuffer<float> buf (2, block);
        double ppq = 4.0, energy = 0; int steps = 0, last = -1;
        for (int b = 0; b < (int) (sr * 4 / block); ++b)
        {
            buf.clear();
            juce::MidiBuffer midi;
            eng.process (buf, midi, reader, true, 120.0, ppq);
            ppq += block / sr * 2.0;
            energy += buf.getRMSLevel (0, 0, block);
            const int st = eng.currentStep (2);
            if (st != last) { ++steps; last = st; }
        }
        std::printf ("host 120 BPM, 4 s: passi visti %d (attesi circa 32), energia %s\n", steps, energy > 1 ? "ok" : "ZERO");
        if (steps < 28 || steps > 36 || energy <= 1) allOk = false;

        setParam (dummy, "B1MUTE", 1); setParam (dummy, "B2MUTE", 1); setParam (dummy, "DRMUTE", 1);
        eng.panic();
        float pk = 0;
        for (int b = 0; b < 40; ++b)
        {
            buf.clear();
            juce::MidiBuffer midi;
            if (b == 0) { midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 110), 0); midi.addEvent (juce::MidiMessage::noteOn (10, 36, (juce::uint8) 100), 10); }
            if (b == 30) midi.addEvent (juce::MidiMessage::noteOff (1, 48), 0);
            eng.process (buf, midi, reader, false, 0, 0);
            pk = juce::jmax (pk, buf.getMagnitude (0, block));
        }
        std::printf ("MIDI nota canale 1 + cassa canale 10: picco %.1f dBFS\n", juce::Decibels::gainToDecibels (pk, -120.f));
        if (pk < 0.01f) allOk = false;
    }
    std::printf (allOk ? "TUTTO OK\n" : "PROBLEMI\n");
    return allOk ? 0 : 1;
}
