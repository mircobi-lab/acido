/*  Strumento di prova (non fa parte del plugin): suona gli 8 pattern di fabbrica col clock interno,
    salva un WAV per pattern e stampa picco, RMS, valori non validi e carico della CPU.
    uso: acido_render CARTELLA_USCITA [battute]                                                       */
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine.h"
#include "Generator.h"
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
    // prova 3: generatore casuale, 200 pattern per stile
    {
        juce::Random rng (1234);
        acido::PatternStore ps;
        for (int style = 0; style < acido::gen::numStyles; ++style)
        {
            int outOfScale = 0, notes1 = 0, notes2 = 0, kicks = 0, snares = 0, slides = 0, empty1 = 0;
            for (int i = 0; i < 200; ++i)
            {
                const auto res = acido::gen::generate (ps, 0, 3, style, rng);
                const auto& sc = acido::gen::scaleSteps (res.scale);
                int n1 = 0;
                for (int t = 0; t < 2; ++t)
                    for (int st = 0; st < 16; ++st)
                    {
                        const auto b = ps.bass (0, t, st);
                        if (! b.gate) continue;
                        (t == 0 ? notes1 : notes2)++;
                        if (t == 0) ++n1;
                        if (b.slide) ++slides;
                        const int pc = (((b.note + 12 * b.octave - res.root) % 12) + 12) % 12;
                        if (std::find (sc.begin(), sc.end(), pc) == sc.end()) ++outOfScale;
                    }
                if (n1 == 0) ++empty1;
                kicks += juce::countNumberOfBits (ps.drumRow (0, acido::BD));
                snares += juce::countNumberOfBits (ps.drumRow (0, acido::SD) | ps.drumRow (0, acido::CP));
                // la rigenerazione di una sola traccia resta nella tonalita'
                const auto k = ps.key (0);
                const auto r2 = acido::gen::generate (ps, 0, 0, style, rng);
                if (r2.root != k.root || r2.scale != k.scale) ++outOfScale;
            }
            std::printf ("stile %-8s note fuori scala %d  medie per pattern: BASS1 %.1f  BASS2 %.1f  slide %.1f  casse %.1f  rullanti/clap %.1f  BASS1 vuoto %d\n",
                         acido::gen::styleName (style), outOfScale, notes1 / 200.0, notes2 / 200.0, slides / 200.0, kicks / 200.0, snares / 200.0, empty1);
            if (outOfScale > 0 || empty1 > 0) allOk = false;
        }
        // un esempio per stile in WAV
        for (int style = 1; style < acido::gen::numStyles; ++style)
        {
            acido::PatternStore one;
            const auto res = acido::gen::generate (one, 0, 3, style, rng);
            setParam (dummy, "PATTERN", 1.f);
            setParam (dummy, "BPM", res.bpm);
            setParam (dummy, "SWING", res.swing);
            int bad = 0;
            for (const auto& [pid, v] : acido::gen::sounds (res.style, 3, rng))
            {
                const auto& d = acido::paramDefs()[(size_t) pid];
                if (v < d.min || v > d.max) ++bad;
                setParam (dummy, d.key, v);
            }
            if (bad) { std::printf ("suoni fuori intervallo: %d\n", bad); allOk = false; }
            setParam (dummy, "B1MUTE", 0); setParam (dummy, "B2MUTE", 0); setParam (dummy, "DRMUTE", 0);
            setParam (dummy, "RUN", 1);
            acido::Engine eng (one);
            eng.prepare (sr, block);
            const int total = (int) (4 * 16 * 15.0 / res.bpm * sr);
            juce::AudioBuffer<float> all (2, total), buf (2, block);
            juce::MidiBuffer midi;
            for (int pos = 0; pos < total; pos += block)
            {
                const int len = juce::jmin (block, total - pos);
                buf.setSize (2, len, false, false, true); buf.clear();
                eng.process (buf, midi, reader, false, 0, 0);
                for (int c = 0; c < 2; ++c) all.copyFrom (c, pos, buf, c, 0, len);
            }
            // BASS 1 da solo, per controllare che il suono dello stile non sparisca
            float soloRms = 0;
            {
                setParam (dummy, "B2MUTE", 1); setParam (dummy, "DRMUTE", 1);
                setParam (dummy, "RUN", 0);
                acido::Engine solo (one);
                solo.prepare (sr, block);
                eng.process (buf, midi, reader, false, 0, 0);
                setParam (dummy, "RUN", 1);
                juce::AudioBuffer<float> sb (2, total);
                for (int pos = 0; pos < total; pos += block)
                {
                    const int len = juce::jmin (block, total - pos);
                    buf.setSize (2, len, false, false, true); buf.clear();
                    solo.process (buf, midi, reader, false, 0, 0);
                    for (int c = 0; c < 2; ++c) sb.copyFrom (c, pos, buf, c, 0, len);
                }
                soloRms = sb.getRMSLevel (0, 0, total);
                setParam (dummy, "B2MUTE", 0); setParam (dummy, "DRMUTE", 0);
            }
            std::printf ("   BASS 1 da solo: RMS %.1f dBFS\n", juce::Decibels::gainToDecibels (soloRms, -120.f));
            if (soloRms < juce::Decibels::decibelsToGain (-30.f)) allOk = false;
            const float peak = all.getMagnitude (0, total);
            std::printf ("esempio %-8s %s %s  %.1f BPM  swing %.1f%%  picco %.1f dBFS\n", acido::gen::styleName (style),
                         acido::gen::keyName (res.root).toRawUTF8(), acido::gen::scaleName (res.scale), res.bpm, res.swing,
                         juce::Decibels::gainToDecibels (peak, -120.f));
            if (peak > 1.f || peak < 1e-3f) allOk = false;
            auto f = outDir.getChildFile (juce::String ("random_") + juce::String (acido::gen::styleName (style)).removeCharacters (" &") + ".wav");
            f.deleteFile();
            if (auto os = std::unique_ptr<juce::OutputStream> (f.createOutputStream()))
                if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (os.get(), sr, 2, 24, {}, 0)))
                { os.release(); w->writeFromAudioSampleBuffer (all, 0, total); }
            setParam (dummy, "RUN", 0);
        }
    }
    std::printf (allOk ? "TUTTO OK\n" : "PROBLEMI\n");
    return allOk ? 0 : 1;
}
