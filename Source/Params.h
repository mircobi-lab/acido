#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

/*  MBStudio91 acido: mappa dei parametri.
    La lista X() qui sotto è l'unica fonte: enum, parametri del plugin e lettura nel motore derivano da qui.
    X(enum, chiave, nome lungo, etichetta pannello, tipo, min, max, default, centro-scala, scelte, unità)
    tipo: R reale, I intero, C scelta, T interruttore.   centro-scala > 0: valore a metà corsa (scala logaritmica).   */

#define ACIDO_BASS_PARAMS(X, n, wave, cut, res, env, dec, pan) \
    X (B##n##WAVE,  "B" #n "WAVE",  "Bass " #n " Wave",       "WAVE",      C, 0, 1, wave, 0, "SAW|SQUARE", "") \
    X (B##n##TUNE,  "B" #n "TUNE",  "Bass " #n " Tune",       "TUNE",      R, -12, 12, 0, 0, "", "st") \
    X (B##n##CUT,   "B" #n "CUT",   "Bass " #n " Cutoff",     "CUTOFF",    R, 0, 1, cut, 0, "", "%") \
    X (B##n##RES,   "B" #n "RES",   "Bass " #n " Resonance",  "RESO",      R, 0, 1, res, 0, "", "%") \
    X (B##n##ENV,   "B" #n "ENV",   "Bass " #n " Env Mod",    "ENV MOD",   R, 0, 1, env, 0, "", "%") \
    X (B##n##DEC,   "B" #n "DEC",   "Bass " #n " Decay",      "DECAY",     R, 0, 1, dec, 0, "", "%") \
    X (B##n##ACC,   "B" #n "ACC",   "Bass " #n " Accent",     "ACCENT",    R, 0, 1, .6f, 0, "", "%") \
    X (B##n##DRIVE, "B" #n "DRIVE", "Bass " #n " Drive",      "DRIVE",     R, 0, 1, .2f, 0, "", "%") \
    X (B##n##LEVEL, "B" #n "LEVEL", "Bass " #n " Level",      "LEVEL",     R, 0, 1, .75f, 0, "", "%") \
    X (B##n##PAN,   "B" #n "PAN",   "Bass " #n " Pan",        "PAN",       R, -1, 1, pan, 0, "", "pan") \
    X (B##n##SEND,  "B" #n "SEND",  "Bass " #n " Delay Send", "DELAY",     R, 0, 1, .18f, 0, "", "%") \
    X (B##n##TRANS, "B" #n "TRANS", "Bass " #n " Transpose",  "TRANSPOSE", I, -12, 12, 0, 0, "", "st") \
    X (B##n##LEN,   "B" #n "LEN",   "Bass " #n " Length",     "LENGTH",    I, 1, 16, 16, 0, "", "steps") \
    X (B##n##MUTE,  "B" #n "MUTE",  "Bass " #n " Mute",       "MUTE",      T, 0, 1, 0, 0, "", "")

#define ACIDO_PARAMS(X) \
    X (RUN,     "RUN",     "Run",               "RUN",       T, 0, 1, 0, 0, "", "") \
    X (BPM,     "BPM",     "Tempo",             "TEMPO",     R, 40, 220, 125, 0, "", "bpm") \
    X (SWING,   "SWING",   "Swing",             "SWING",     R, 50, 75, 50, 0, "", "sw") \
    X (PATTERN, "PATTERN", "Pattern",           "PATTERN",   I, 1, 8, 1, 0, "", "") \
    ACIDO_BASS_PARAMS (X, 1, 0, .30f, .72f, .55f, .40f, -.15f) \
    ACIDO_BASS_PARAMS (X, 2, 1, .45f, .60f, .45f, .30f, .20f) \
    X (AC,      "AC",      "Drum Accent",       "ACCENT",    R, 0, 1, .6f, 0, "", "%") \
    X (BDLEV,   "BDLEV",   "Bass Drum Level",   "LEVEL",     R, 0, 1, .85f, 0, "", "%") \
    X (BDTONE,  "BDTONE",  "Bass Drum Tone",    "TONE",      R, 0, 1, .35f, 0, "", "%") \
    X (BDDEC,   "BDDEC",   "Bass Drum Decay",   "DECAY",     R, 0, 1, .55f, 0, "", "%") \
    X (SDLEV,   "SDLEV",   "Snare Level",       "LEVEL",     R, 0, 1, .7f, 0, "", "%") \
    X (SDTONE,  "SDTONE",  "Snare Tone",        "TONE",      R, 0, 1, .5f, 0, "", "%") \
    X (SDSNAP,  "SDSNAP",  "Snare Snappy",      "SNAPPY",    R, 0, 1, .6f, 0, "", "%") \
    X (LTLEV,   "LTLEV",   "Low Tom Level",     "LEVEL",     R, 0, 1, .7f, 0, "", "%") \
    X (LTTUNE,  "LTTUNE",  "Low Tom Tuning",    "TUNING",    R, 0, 1, .5f, 0, "", "%") \
    X (MTLEV,   "MTLEV",   "Mid Tom Level",     "LEVEL",     R, 0, 1, .7f, 0, "", "%") \
    X (MTTUNE,  "MTTUNE",  "Mid Tom Tuning",    "TUNING",    R, 0, 1, .5f, 0, "", "%") \
    X (HTLEV,   "HTLEV",   "High Tom Level",    "LEVEL",     R, 0, 1, .7f, 0, "", "%") \
    X (HTTUNE,  "HTTUNE",  "High Tom Tuning",   "TUNING",    R, 0, 1, .5f, 0, "", "%") \
    X (RSLEV,   "RSLEV",   "Rim Shot Level",    "LEVEL",     R, 0, 1, .6f, 0, "", "%") \
    X (CPLEV,   "CPLEV",   "Hand Clap Level",   "LEVEL",     R, 0, 1, .7f, 0, "", "%") \
    X (CBLEV,   "CBLEV",   "Cowbell Level",     "LEVEL",     R, 0, 1, .55f, 0, "", "%") \
    X (CYLEV,   "CYLEV",   "Cymbal Level",      "LEVEL",     R, 0, 1, .55f, 0, "", "%") \
    X (CYTONE,  "CYTONE",  "Cymbal Tone",       "TONE",      R, 0, 1, .5f, 0, "", "%") \
    X (CYDEC,   "CYDEC",   "Cymbal Decay",      "DECAY",     R, 0, 1, .5f, 0, "", "%") \
    X (OHLEV,   "OHLEV",   "Open Hat Level",    "LEVEL",     R, 0, 1, .6f, 0, "", "%") \
    X (OHDEC,   "OHDEC",   "Open Hat Decay",    "DECAY",     R, 0, 1, .35f, 0, "", "%") \
    X (CHLEV,   "CHLEV",   "Closed Hat Level",  "LEVEL",     R, 0, 1, .6f, 0, "", "%") \
    X (DRLEVEL, "DRLEVEL", "Drums Level",       "LEVEL",     R, 0, 1, .8f, 0, "", "%") \
    X (DRSEND,  "DRSEND",  "Drums Delay Send",  "DELAY",     R, 0, 1, 0, 0, "", "%") \
    X (DRLEN,   "DRLEN",   "Drums Length",      "LENGTH",    I, 1, 16, 16, 0, "", "steps") \
    X (DRMUTE,  "DRMUTE",  "Drums Mute",        "MUTE",      T, 0, 1, 0, 0, "", "") \
    X (DLTIME,  "DLTIME",  "Delay Time",        "TIME",      C, 0, 5, 2, 0, "1/16|1/8|3/16|1/4|3/8|1/2", "") \
    X (DLFB,    "DLFB",    "Delay Feedback",    "FEEDBACK",  R, 0, .95f, .45f, 0, "", "%") \
    X (DLTONE,  "DLTONE",  "Delay Tone",        "TONE",      R, 0, 1, .5f, 0, "", "%") \
    X (DLRET,   "DLRET",   "Delay Return",      "RETURN",    R, 0, 1, .7f, 0, "", "%") \
    X (MASTER,  "MASTER",  "Master Volume",     "MASTER",    R, -36, 6, -7, 0, "", "dB") \
    X (GENSTYLE,"GENSTYLE","Random Style",      "STYLE",     C, 0, 5, 0, 0, "AUTO|ACID|TECHNO|FUNK|HIP HOP|R&B", "") \
    X (GENSOUND,"GENSOUND","Random Sounds",     "SND",       T, 0, 1, 1, 0, "", "") \
    X (WRITE,   "WRITE",   "Tap Write",         "WRITE",     T, 0, 1, 0, 0, "", "") \
    X (CHAIN,   "CHAIN",   "Pattern Chain",     "CHAIN",     T, 0, 1, 0, 0, "", "")

namespace acido
{
enum class PType { R, I, C, T };

#define ACIDO_ENUM(e, ...) e,
enum P : int { ACIDO_PARAMS (ACIDO_ENUM) NUM_PARAMS };
#undef ACIDO_ENUM

struct ParamDef
{
    const char* key;
    const char* name;
    const char* label;
    PType type;
    float min, max, def, centre;
    const char* choices;
    const char* unit;
    juce::StringArray choiceList() const { return juce::StringArray::fromTokens (choices, "|", ""); }
};

const std::vector<ParamDef>& paramDefs();
juce::String formatValue (const ParamDef&, float);
juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

// Lettura dei valori dal thread audio senza allocare.
class ParamReader
{
public:
    void attach (juce::AudioProcessorValueTreeState&);
    float operator[] (P p) const noexcept { return ptr[(size_t) p]->load (std::memory_order_relaxed); }
    int i (P p) const noexcept { return (int) std::lround ((*this)[p]); }
    bool b (P p) const noexcept { return (*this)[p] > .5f; }
private:
    std::array<std::atomic<float>*, NUM_PARAMS> ptr {};
};
}
