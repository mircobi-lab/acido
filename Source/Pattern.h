#pragma once
#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <atomic>

/*  Memoria dei pattern di acido: 8 pattern, ognuno con
      - due linee di basso (BASS 1 e BASS 2) da 16 passi: nota, ottava, gate, accento, slide
      - la batteria: 11 strumenti + riga ACCENT, 16 passi ciascuno
    Tutto è in variabili atomiche: il pannello scrive, il thread audio legge senza blocchi.   */
namespace acido
{
constexpr int numPatterns = 8, numSteps = 16, numBass = 2, numDrums = 11, accentRow = numDrums;

// ordine del pannello frontale
enum Drum { BD, SD, LT, MT, HT, RS, CP, CB, CY, OH, CH };
const char* drumShort (int d);    // "BD"
const char* drumName (int d);     // "BASS DRUM"

struct BassStep
{
    int note = 0;           // 0..12 (C..C sopra)
    int octave = 0;         // -1, 0, +1
    bool gate = false, accent = false, slide = false;

    uint32_t pack() const noexcept
    {
        return (uint32_t) juce::jlimit (0, 12, note) | ((uint32_t) (octave + 1) << 4)
             | (gate ? 0x40u : 0u) | (accent ? 0x80u : 0u) | (slide ? 0x100u : 0u);
    }
    static BassStep unpack (uint32_t v) noexcept
    {
        BassStep s;
        s.note = (int) (v & 15u);
        s.octave = juce::jlimit (-1, 1, (int) ((v >> 4) & 3u) - 1);
        s.gate = (v & 0x40u) != 0; s.accent = (v & 0x80u) != 0; s.slide = (v & 0x100u) != 0;
        return s;
    }
    int midiNote (int transpose) const noexcept { return 36 + note + 12 * octave + transpose; }   // C2 = 36
};

struct KeyInfo { int root = -1, scale = -1, style = -1; };    // tonalità scelta dal generatore (-1 = nessuna)

class PatternStore
{
public:
    PatternStore();

    BassStep bass (int pattern, int track, int step) const noexcept;
    void setBass (int pattern, int track, int step, const BassStep&) noexcept;
    bool drum (int pattern, int row, int step) const noexcept;
    uint32_t drumRow (int pattern, int row) const noexcept;
    void setDrum (int pattern, int row, int step, bool on) noexcept;

    KeyInfo key (int pattern) const noexcept;
    void setKey (int pattern, KeyInfo) noexcept;

    void clearTrack (int pattern, int track);       // track 0/1 basso, 2 batteria

    // catena dei pattern: fino a 16 battute, ognuna con il suo pattern (0..7)
    static constexpr int maxChain = 16;
    int chainLength() const noexcept { return chainLen.load(); }
    int chainAt (int i) const noexcept { return chain[(size_t) juce::jlimit (0, maxChain - 1, i)].load(); }
    bool chainAppend (int pattern) noexcept;
    void chainRemoveLast() noexcept { if (chainLen.load() > 0) { chainLen.fetch_sub (1); touch(); } }
    void chainClear() noexcept { chainLen = 0; touch(); }
    void copyPattern (int from, int to);
    void loadFactory();
    uint32_t version() const noexcept { return ver.load(); }

    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree&);

private:
    std::array<std::atomic<uint32_t>, numPatterns * numBass * numSteps> b {};
    std::array<std::atomic<uint32_t>, numPatterns * (numDrums + 1)> d {};
    std::array<std::atomic<int>, numPatterns * 3> keys {};
    std::array<std::atomic<int>, maxChain> chain {};
    std::atomic<int> chainLen { 0 };
    std::atomic<uint32_t> ver { 1 };
    void touch() noexcept { ver.fetch_add (1); }
    static size_t bi (int p, int t, int s) noexcept { return (size_t) ((p * numBass + t) * numSteps + s); }
    static size_t di (int p, int r) noexcept { return (size_t) (p * (numDrums + 1) + r); }
};
}
