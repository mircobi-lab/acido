#include "Pattern.h"

namespace acido
{
const char* drumShort (int d)
{
    static const char* n[] = { "BD", "SD", "LT", "MT", "HT", "RS", "CP", "CB", "CY", "OH", "CH" };
    return d >= 0 && d < numDrums ? n[d] : "AC";
}

const char* drumName (int d)
{
    static const char* n[] = { "BASS DRUM", "SNARE DRUM", "LOW TOM", "MID TOM", "HIGH TOM", "RIM SHOT",
                               "HAND CLAP", "COWBELL", "CYMBAL", "OPEN HAT", "CLOSED HAT" };
    return d >= 0 && d < numDrums ? n[d] : "ACCENT";
}

PatternStore::PatternStore() { loadFactory(); }

BassStep PatternStore::bass (int p, int t, int s) const noexcept { return BassStep::unpack (b[bi (p, t, s)].load (std::memory_order_relaxed)); }
void PatternStore::setBass (int p, int t, int s, const BassStep& st) noexcept { b[bi (p, t, s)].store (st.pack()); touch(); }
uint32_t PatternStore::drumRow (int p, int r) const noexcept { return d[di (p, r)].load (std::memory_order_relaxed); }
bool PatternStore::drum (int p, int r, int s) const noexcept { return (drumRow (p, r) >> s) & 1u; }

void PatternStore::setDrum (int p, int r, int s, bool on) noexcept
{
    auto& a = d[di (p, r)];
    const uint32_t bit = 1u << s;
    if (on) a.fetch_or (bit); else a.fetch_and (~bit);
    touch();
}

void PatternStore::clearTrack (int p, int track)
{
    if (track < numBass)
        for (int s = 0; s < numSteps; ++s) b[bi (p, track, s)].store (BassStep().pack());
    else
        for (int r = 0; r <= numDrums; ++r) d[di (p, r)].store (0);
    touch();
}

void PatternStore::copyPattern (int from, int to)
{
    if (from == to) return;
    for (int t = 0; t < numBass; ++t)
        for (int s = 0; s < numSteps; ++s) b[bi (to, t, s)].store (b[bi (from, t, s)].load());
    for (int r = 0; r <= numDrums; ++r) d[di (to, r)].store (d[di (from, r)].load());
    touch();
}

// ------------------------------------------------------------------------------------------- fabbrica
namespace
{
/*  Linee di basso in testo, 16 parole separate da spazi:
      "."  pausa      C C# D ... B  nota      C'  il Do sopra
      suffissi:  ^ ottava sopra   v ottava sotto   ! accento   ~ slide verso il passo successivo        */
void parseBass (PatternStore& ps, int p, int t, const char* text)
{
    auto tokens = juce::StringArray::fromTokens (text, " ", "");
    tokens.removeEmptyStrings();
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int s = 0; s < numSteps; ++s)
    {
        BassStep st;
        const juce::String tok = s < tokens.size() ? tokens[s] : juce::String (".");
        if (tok != ".")
        {
            juce::String core = tok.retainCharacters ("ABCDEFG#'");
            int n = 0;
            const bool top = core.endsWithChar ('\'');
            core = core.removeCharacters ("'");
            for (int k = 0; k < 12; ++k) if (core == names[k]) n = k;
            st.note = top ? 12 : n;
            st.octave = tok.containsChar ('^') ? 1 : (tok.containsChar ('v') ? -1 : 0);
            st.accent = tok.containsChar ('!');
            st.slide = tok.containsChar ('~');
            st.gate = true;
        }
        ps.setBass (p, t, s, st);
    }
}

void parseDrum (PatternStore& ps, int p, int row, const char* text)
{
    const juce::String s (text);
    for (int k = 0; k < numSteps; ++k) ps.setDrum (p, row, k, k < s.length() && s[k] == 'x');
}

struct Factory
{
    const char* bass1;
    const char* bass2;
    std::array<const char*, numDrums + 1> drums;   // BD SD LT MT HT RS CP CB CY OH CH AC
};

// Pattern originali MBStudio91 (non trascritti da brani esistenti).
const Factory factory[numPatterns] =
{
    { "C C^ . C! D#~ F C . G~ G^! . A# C^~ C . D#!",
      ". . . . G^! . . . . . D#^~ F^ . . . .",
      { "x...x...x...x...", "", "", "", "", "", "....x.......x...", "", "", "", "..x...x...x...x.", "x.......x......." } },
    { "A# C~ C C^! . C D#~ D# . C^! A#v C . G! G^~ C",
      "",
      { "x...x...x...x.x.", "....x.......x...", "", "", "", "", "", "", "", "...x.......x....", "x.x.x.x.x.x.x.x.", "....x.......x..x" } },
    { "C . C . D# . C~ C . . F . D#~ C . A#v",
      "G^ . . G^~ A#^ . . . C'! . . . G^~ F^ . .",
      { "x...x...x..xx...", "", "..............x.", "", "", "", "....x.......x...", "", "", "", "..x...x...x...x.", "x.......x......." } },
    { "C C C~ D# C C C^! C C C C~ G C C^ A#! C",
      "",
      { "x...x...x...x...", "", "", "", "", "...x..x....x..x.", "....x.......x...", "", "", "", "xxxxxxxxxxxxxxxx", "x.x.x.x.x.x.x.x." } },
    { "C . C^ . C . A#v . C . C^! . G . F~ F#",
      ". G^ . G^~ A#^ . C'! . . G^ . D#^ . F^~ G^ .",
      { "x...x...x...x...", "", "", "", "", "", "....x.......x...", "", "x...............", "..............x.", "..x...x...x.....", "x...x...x...x..." } },
    { "C~ C . C . . D# . C . . C . G~ A# .",
      "",
      { "x.....x..x......", "....x.......x...", "", "", "..............x.", "", "............x...", "..x.....x...x..x", "", "", "x.x.x.x.x.x.x.x.", "x...x.....x.x..." } },
    { "Cv~ C~ C^~ C~ D#~ D#^~ C~ C G~ G^~ A#~ A#^ C^~ C~ D#!~ C",
      "",
      { "x...x...x...x...", "", "", "", "", "", "....x.......x...", "", "", "..x...x...x...x.", "", "........x......." } },
    { "C . . C D#! . . C . C . . G^~ C . .",
      "",
      { "x...x...x...x...", "", "", "", "", "..x..x....x..x..", "", "", "", "", "..x...x...x...x.", "" } },
};
}

void PatternStore::loadFactory()
{
    for (int p = 0; p < numPatterns; ++p)
    {
        parseBass (*this, p, 0, factory[p].bass1);
        parseBass (*this, p, 1, factory[p].bass2);
        for (int r = 0; r <= numDrums; ++r) parseDrum (*this, p, r, factory[p].drums[(size_t) r]);
    }
    touch();
}

// ------------------------------------------------------------------------------------------- stato
juce::ValueTree PatternStore::toValueTree() const
{
    juce::ValueTree t ("PATTERNS");
    t.setProperty ("format", 1, nullptr);
    for (int p = 0; p < numPatterns; ++p)
    {
        juce::ValueTree pt ("P");
        for (int tr = 0; tr < numBass; ++tr)
        {
            juce::StringArray v;
            for (int s = 0; s < numSteps; ++s) v.add (juce::String::toHexString ((int) b[bi (p, tr, s)].load()));
            pt.setProperty ("bass" + juce::String (tr + 1), v.joinIntoString (" "), nullptr);
        }
        juce::StringArray dv;
        for (int r = 0; r <= numDrums; ++r) dv.add (juce::String::toHexString ((int) d[di (p, r)].load()));
        pt.setProperty ("drums", dv.joinIntoString (" "), nullptr);
        t.appendChild (pt, nullptr);
    }
    return t;
}

void PatternStore::fromValueTree (const juce::ValueTree& t)
{
    if (! t.hasType ("PATTERNS")) return;
    for (int p = 0; p < numPatterns && p < t.getNumChildren(); ++p)
    {
        const auto pt = t.getChild (p);
        for (int tr = 0; tr < numBass; ++tr)
        {
            auto v = juce::StringArray::fromTokens (pt.getProperty ("bass" + juce::String (tr + 1)).toString(), " ", "");
            for (int s = 0; s < numSteps && s < v.size(); ++s)
                b[bi (p, tr, s)].store (BassStep::unpack ((uint32_t) v[s].getHexValue32()).pack());
        }
        auto dv = juce::StringArray::fromTokens (pt.getProperty ("drums").toString(), " ", "");
        for (int r = 0; r <= numDrums && r < dv.size(); ++r) d[di (p, r)].store ((uint32_t) dv[r].getHexValue32() & 0xffffu);
    }
    touch();
}
}
