/*  Prove automatiche di WRITE (scrittura in tempo reale) e CHAIN (catena dei pattern) di acido.
    Esce con 1 se una prova fallisce.  Uso: acido_writetest                                   */
#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine.h"

namespace
{
struct Dummy : juce::AudioProcessor
{
    Dummy() : AudioProcessor (BusesProperties().withOutput ("Out", juce::AudioChannelSet::stereo(), true)),
              state (*this, nullptr, "ACIDO", acido::createLayout()) {}
    const juce::String getName() const override { return "test"; }
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

int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { std::printf ("  FALLITA: %s  (riga %d)\n", msg, __LINE__); ++failures; } } while (0)

constexpr double sr = 48000;
constexpr int block = 64;

// fa girare il motore per 'samples' campioni; 'events' = (campione assoluto, messaggio)
struct Runner
{
    Dummy& d; acido::ParamReader& pr; acido::Engine& eng;
    long long clock = 0;
    float peak = 0;
    void run (long long samples, std::vector<std::pair<long long, juce::MidiMessage>> events = {})
    {
        juce::AudioBuffer<float> buf (2, block);
        for (long long pos = 0; pos < samples; pos += block)
        {
            const int n = (int) std::min<long long> (block, samples - pos);
            juce::MidiBuffer midi;
            for (auto& e : events)
            {
                const long long at = e.first - (clock + pos);
                if (at >= 0 && at < n) midi.addEvent (e.second, (int) at);
            }
            juce::AudioBuffer<float> b (buf.getArrayOfWritePointers(), 2, n);
            b.clear();
            eng.process (b, midi, pr, false, 0, 0);
            peak = std::max (peak, b.getMagnitude (0, n));
        }
        clock += samples;
    }
};
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    Dummy d;
    acido::ParamReader pr; pr.attach (d.state);
    setParam (d, "BPM", 120);
    const long long step = 6000;        // un sedicesimo a 120 BPM, 48 kHz

    std::printf ("1. WRITE della batteria: colpo in anticipo e in ritardo\n");
    {
        acido::PatternStore st;
        for (int p = 0; p < acido::numPatterns; ++p) st.clearTrack (p, 2);
        acido::Engine eng (st); eng.prepare (sr, block);
        Runner r { d, pr, eng };
        setParam (d, "PATTERN", 1); setParam (d, "WRITE", 1); setParam (d, "RUN", 1);
        r.run (step * 2 + 1000, { { step * 2 + 800, juce::MidiMessage::noteOn (10, 38, (juce::uint8) 90) } });   // poco dopo il passo 3
        CHECK (st.drum (0, acido::SD, 2), "un colpo appena dopo il passo 3 va sul passo 3");
        r.run (step * 3, { { step * 4 - 500, juce::MidiMessage::noteOn (10, 36, (juce::uint8) 120) } });  // poco prima del passo 5
        CHECK (st.drum (0, acido::BD, 4), "un colpo appena prima del passo 5 va sul passo 5");
        CHECK (st.drum (0, acido::accentRow, 4), "velocity alta = accento");
        CHECK (! st.drum (0, acido::BD, 3), "non sporca il passo prima");
        setParam (d, "WRITE", 0);
        r.run (step, { { r.clock + 100, juce::MidiMessage::noteOn (10, 42, (juce::uint8) 90) } });
        bool anyCH = false;
        for (int k = 0; k < 16; ++k) anyCH = anyCH || st.drum (0, acido::CH, k);
        CHECK (! anyCH, "senza WRITE il colpo suona ma non si scrive");
        setParam (d, "RUN", 0);
        r.run (block);
    }

    std::printf ("2. WRITE dei bassi: nota, ottava, accento, slide\n");
    {
        acido::PatternStore st;
        st.clearTrack (0, 0);
        acido::Engine eng (st); eng.prepare (sr, block);
        Runner r { d, pr, eng };
        setParam (d, "PATTERN", 1); setParam (d, "WRITE", 1); setParam (d, "B1TRANS", 0); setParam (d, "RUN", 1);
        r.run (step / 4);
        // passo 1: G2 (43) tenuto, passo 2: C3 (48) legato, poi rilascio
        r.run (step * 3, { { r.clock + 10, juce::MidiMessage::noteOn (1, 43, (juce::uint8) 110) },
                           { r.clock + step, juce::MidiMessage::noteOn (1, 48, (juce::uint8) 80) },
                           { r.clock + step + 2000, juce::MidiMessage::noteOff (1, 43) },
                           { r.clock + step + 3000, juce::MidiMessage::noteOff (1, 48) } });
        const auto s1 = st.bass (0, 0, 0), s2 = st.bass (0, 0, 1);
        CHECK (s1.gate && s1.note == 7 && s1.octave == 0 && s1.accent, "passo 1: G, ottava 0, accento");
        CHECK (s1.slide, "la nota legata accende lo slide sul passo prima");
        CHECK (s2.gate && s2.note == 12 && s2.octave == 0 && ! s2.accent, "passo 2: C sopra");
        // il prossimo passo che parte è il 5 (campione 24000): la nota arriva 200 campioni dopo
        r.run (step * 2, { { step * 4 + 200, juce::MidiMessage::noteOn (1, 26, (juce::uint8) 90) }, { step * 4 + 900, juce::MidiMessage::noteOff (1, 26) } });
        const auto s4 = st.bass (0, 0, 4);
        CHECK (s4.gate && s4.octave == -1 && s4.note == 2, "D1 (26) diventa D ottava giù");
        setParam (d, "RUN", 0); setParam (d, "WRITE", 0);
        r.run (block);
    }

    std::printf ("3. CHAIN: un pattern per battuta, nell'ordine della catena\n");
    {
        acido::PatternStore st;
        st.chainClear();
        st.chainAppend (2); st.chainAppend (0); st.chainAppend (5);
        acido::Engine eng (st); eng.prepare (sr, block);
        Runner r { d, pr, eng };
        setParam (d, "PATTERN", 1); setParam (d, "CHAIN", 1); setParam (d, "RUN", 1);
        std::vector<int> seen;
        for (int bar = 0; bar < 4; ++bar)
        {
            r.run (step * 8);
            seen.push_back (eng.playingPattern());
            CHECK (eng.chainPosition() == bar % 3, "posizione nella catena");
            r.run (step * 8);
        }
        CHECK (seen == std::vector<int> ({ 2, 0, 5, 2 }), "battute 1-4 suonano 3, 1, 6, poi la catena ricomincia");
        setParam (d, "CHAIN", 0);
        r.run (step * 16);
        CHECK (eng.playingPattern() == 0 && eng.chainPosition() == -1, "senza CHAIN torna al pattern scelto");
        setParam (d, "RUN", 0);
        r.run (block);
        CHECK (r.peak < 1.01f, "uscita sotto 0 dBFS");
    }

    std::printf ("4. salvataggio della catena\n");
    {
        acido::PatternStore a, b;
        a.chainClear(); a.chainAppend (7); a.chainAppend (3);
        b.fromValueTree (a.toValueTree());
        CHECK (b.chainLength() == 2 && b.chainAt (0) == 7 && b.chainAt (1) == 3, "la catena torna uguale");
        acido::PatternStore c;
        for (int i = 0; i < 20; ++i) c.chainAppend (1);
        CHECK (c.chainLength() == acido::PatternStore::maxChain, "massimo 16 voci");
    }

    std::printf (failures == 0 ? "TUTTE LE PROVE SUPERATE\n" : "%d PROVE FALLITE\n", failures);
    return failures == 0 ? 0 : 1;
}
