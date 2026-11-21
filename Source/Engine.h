#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "Params.h"
#include "Pattern.h"
#include <array>
#include <atomic>
#include <vector>

/*  Motore di acido.
    - AcidVoice: voce monofonica di basso acido (dente di sega o quadra, filtro passa-basso a 4 poli con risonanza,
      inviluppo del filtro con DECAY, accento con "sweep" che si accumula su accenti consecutivi, slide).
    - Drum voices: 11 strumenti a modelli analogici (cassa a risonatore, rullante, tom, rim shot, clap a raffiche,
      campanaccio, piatto e charleston dal banco di 6 oscillatori quadri).
    - Delay stereo ping-pong a tempo, mandate dalle tre tracce.
    - Sequencer a 16 passi con swing, lunghezza per traccia, cambio di pattern a fine battuta.
    MIDI: canale 1 suona BASS 1, canale 2 BASS 2, canale 10 la batteria (note GM). Velocity > 100 = accento.   */
namespace acido
{
// ------------------------------------------------------------------------------------------- utilità
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void setBandPass (double sr, double f, double q);
    void setHighPass (double sr, double f, double q);
    void setLowPass (double sr, double f, double q);
    float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() noexcept { z1 = z2 = 0; }
};

struct Noise
{
    uint32_t s = 0x9e3779b9u;
    float next() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (int32_t) s * (1.f / 2147483648.f); }
};

inline float coefFor (double sr, double seconds) noexcept   // costante di tempo esponenziale
{
    return seconds <= 0 ? 0.f : (float) std::exp (-1.0 / (seconds * sr));
}

// ------------------------------------------------------------------------------------------- basso acido
struct BassSettings
{
    int wave = 0;
    float tune = 0, cutoff = .3f, reso = .7f, envMod = .5f, decay = .4f, accent = .6f, drive = .2f;
};

class AcidVoice
{
public:
    void prepare (double sampleRate);
    void set (const BassSettings& settings) noexcept { cfg = settings; }
    void trigger (int midiNote, bool accent, bool slide) noexcept;
    void release() noexcept { gate = false; }
    void panic() noexcept;
    bool isGateOn() const noexcept { return gate; }
    float process() noexcept;

private:
    double sr = 44100;
    BassSettings cfg;
    double phase = 0;
    float pitch = 36, target = 36;
    bool gliding = false, gate = false, accNote = false, attacking = false;
    float meg = 0, veg = 0, accCap = 0, accGain = 1;
    float megCoef = .999f, glideCoef = .999f, vegAtt = .99f, vegHold = .9999f, vegRel = .99f, accAtt = .99f, accRel = .999f, smooth2ms = .99f;
    float s[4] {}, hpState = 0, hpCoef = .999f, lastIn = 0, cutSm = 1000;
    float shaped = 0;
};

// ------------------------------------------------------------------------------------------- batteria
struct DrumSettings
{
    float bdTone = .35f, bdDecay = .55f, sdTone = .5f, sdSnappy = .6f;
    float tomTune[3] { .5f, .5f, .5f };
    float cyTone = .5f, cyDecay = .5f, ohDecay = .35f;
};

class DrumKit
{
public:
    void prepare (double sampleRate);
    void set (const DrumSettings& s) noexcept;
    void trigger (int drum, float gain) noexcept;
    void panic() noexcept;
    // scrive ogni strumento sul suo canale (out[drum]) per poterlo panoramicare e mixare
    void process (std::array<float, numDrums>& out) noexcept;

private:
    double sr = 44100;
    DrumSettings cfg;
    Noise noise;
    std::array<float, numDrums> gain {};
    // cassa
    double bdPhase = 0; float bdAmp = 0, bdPitch = 0, bdClick = 0, bdLp = 0;
    // rullante
    double sdPh1 = 0, sdPh2 = 0; float sdA1 = 0, sdA2 = 0, sdN = 0; Biquad sdHp, sdLp;
    // tom
    double tomPh[3] {}; float tomAmp[3] {}, tomPitch[3] {}, tomN[3] {}; Biquad tomNoiseLp[3];
    // rim shot
    double rsPh1 = 0, rsPh2 = 0; float rsAmp = 0; Biquad rsHp;
    // clap
    float cpT = 1e9f, cpTail = 0; Biquad cpBp, cpBp2;
    // banco metallico: 6 quadre
    std::array<double, 6> mPh {}; std::array<double, 6> mInc {};
    float ohAmp = 0, chAmp = 0, cyAmpLo = 0, cyAmpHi = 0, cbAmpFast = 0, cbAmpSlow = 0;
    Biquad hatHp1, hatHp2, cyBpLo, cyHpHi, cbBp;
    bool ohChoke = false;
    // coefficienti (ricalcolati in set())
    float cBdAmp = .999f, cBdPitch = .99f, cBdClick = .9f, bdLpA = .1f;
    float cSd1 = .999f, cSd2 = .999f, cSdN = .999f;
    float cTom[3] { .999f, .999f, .999f }, cTomP = .999f, cTomN = .99f, tomF[3] { 80, 120, 170 };
    float cRs = .99f, cCbF = .99f, cCbS = .999f, cCyLo = .9999f, cCyHi = .999f, cOh = .999f, cCh = .99f, cChoke = .9f;
};

// ------------------------------------------------------------------------------------------- delay
class PingPongDelay
{
public:
    void prepare (double sampleRate);
    void reset();
    void process (float inL, float inR, float& outL, float& outR, float timeSamples, float feedback, float tone) noexcept;
private:
    std::vector<float> bufL, bufR;
    int w = 0;
    float timeSm = 0, lpL = 0, lpR = 0, hpL = 0, hpR = 0;
    float read (const std::vector<float>&, float delay) const noexcept;
};

// ------------------------------------------------------------------------------------------- sintetizzatore completo
class Engine
{
public:
    explicit Engine (PatternStore& p) : patterns (p) {}

    void prepare (double sampleRate, int maxBlock);
    void process (juce::AudioBuffer<float>&, const juce::MidiBuffer&, const ParamReader&,
                  bool hostPlaying, double hostBpm, double hostPpq) noexcept;
    void panic() noexcept;

    // per il pannello (lettura dal thread dei messaggi)
    float getOutputLevel() const noexcept { return outLevel.load(); }
    int currentStep (int track) const noexcept { return stepNow[(size_t) track].load(); }   // -1 = fermo
    int playingPattern() const noexcept { return playPatternUi.load(); }
    bool isRunning() const noexcept { return runningUi.load(); }
    bool isHostClock() const noexcept { return hostClockUi.load(); }
    double currentBpm() const noexcept { return bpmUi.load(); }
    uint32_t drumHits() const noexcept { return hits.load(); }        // bit per strumento, azzerato dal pannello
    void clearDrumHits() noexcept { hits.store (0); }

private:
    PatternStore& patterns;
    double sr = 44100;
    AcidVoice bass[numBass];
    DrumKit drums;
    PingPongDelay delay;

    // sequencer
    double pos = 0;             // posizione in sedicesimi
    long long lastStep = -1;
    bool running = false, hostRun = false;
    int playPattern = 0;
    double offAt[numBass] { 1e18, 1e18 };
    BassStep prevStep[numBass];
    // MIDI dei bassi: pila delle note tenute
    std::array<int, 16> held[numBass] {};
    int heldCount[numBass] {};

    std::array<float, 8> gainSm {};   // BASS1 L/R, BASS2 L/R, DRUMS, MASTER, RETURN, (libero)
    bool needPattern = true;
    float peak = 0;

    std::atomic<float> outLevel { 0 };
    std::array<std::atomic<int>, 3> stepNow { -1, -1, -1 };
    std::atomic<int> playPatternUi { 0 };
    std::atomic<bool> runningUi { false }, hostClockUi { false };
    std::atomic<double> bpmUi { 125 };
    std::atomic<uint32_t> hits { 0 };

    static long long stepIndexAt (double p, double oddStart) noexcept;
    static double stepStart (long long idx, double oddStart) noexcept;
    void onStep (long long idx, double oddStart, const ParamReader&) noexcept;
    void stopSequencer() noexcept;
    void handleMidi (const juce::MidiMessage&, const ParamReader&) noexcept;
    void bassNoteOn (int t, int note, int vel) noexcept;
    void bassNoteOff (int t, int note) noexcept;
};
}
