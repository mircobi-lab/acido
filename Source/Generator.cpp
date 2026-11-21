#include "Generator.h"

namespace acido::gen
{
namespace
{
struct ScaleDef { const char* name; std::vector<int> steps; };

const std::vector<ScaleDef>& scaleDefs()
{
    static const std::vector<ScaleDef> s {
        { "MINOR PENTATONIC", { 0, 3, 5, 7, 10 } },
        { "AEOLIAN",          { 0, 2, 3, 5, 7, 8, 10 } },
        { "DORIAN",           { 0, 2, 3, 5, 7, 9, 10 } },
        { "PHRYGIAN",         { 0, 1, 3, 5, 7, 8, 10 } },
        { "BLUES",            { 0, 3, 5, 6, 7, 10 } },
        { "HARMONIC MINOR",   { 0, 2, 3, 5, 7, 8, 11 } },
        { "MIXOLYDIAN",       { 0, 2, 4, 5, 7, 9, 10 } },
        { "MAJOR PENTATONIC", { 0, 2, 4, 7, 9 } },
    };
    return s;
}

// scale adatte a ogni stile (indici di scaleDefs)
const std::vector<int>& scalesFor (int style)
{
    static const std::vector<int> acid { 0, 3, 1, 5, 4 }, techno { 0, 3, 1, 2 }, funk { 2, 6, 0, 4 },
                                  hiphop { 0, 1, 2, 4 }, rnb { 2, 1, 7, 0 };
    switch (style)
    {
        case TECHNO: return techno;
        case FUNK:   return funk;
        case HIPHOP: return hiphop;
        case RNB:    return rnb;
        default:     return acid;
    }
}

struct Feel
{
    float density, accent, slide, octUp, octDown, tie, rootBias, variation;
    int motif;              // lunghezza del motivo ripetuto
    float bpmLo, bpmHi, swingLo, swingHi;
};

Feel feelFor (int style, juce::Random& r)
{
    switch (style)
    {
        case TECHNO: return { .62f, .22f, .12f, .25f, .05f, 0.f,  .55f, .2f,  r.nextFloat() < .5f ? 4 : 8, 126, 134, 50, 53 };
        case FUNK:   return { .58f, .20f, .15f, .30f, .10f, .1f,  .40f, .3f,  8, 106, 118, 54, 58 };
        case HIPHOP: return { .30f, .12f, .30f, .10f, .30f, .5f,  .50f, .25f, 8, 84, 94, 56, 62 };
        case RNB:    return { .35f, .10f, .30f, .15f, .20f, .45f, .35f, .25f, 8, 72, 90, 55, 60 };
        default:     return { .75f, .28f, .25f, .22f, .05f, 0.f,  .35f, .3f,  8, 124, 132, 50, 54 };
    }
}

bool chance (juce::Random& r, float p) { return r.nextFloat() < p; }

int pickWeighted (juce::Random& r, std::initializer_list<std::pair<int, int>> items)
{
    int total = 0;
    for (auto& i : items) total += i.second;
    int x = r.nextInt (juce::jmax (1, total));
    for (auto& i : items) { if (x < i.second) return i.first; x -= i.second; }
    return items.begin()->first;
}

// altezza (semitoni sopra C2) del grado deg della scala, anche negativo o oltre l'ottava
int degreePitch (int root, int scale, int deg)
{
    const auto& st = scaleDefs()[(size_t) scale].steps;
    const int n = (int) st.size();
    const int oct = deg >= 0 ? deg / n : -((-deg + n - 1) / n);
    return root + st[(size_t) (deg - oct * n)] + 12 * oct;
}

BassStep toStep (int semis)
{
    while (semis < -12) semis += 12;
    while (semis > 24) semis -= 12;
    BassStep s;
    s.gate = true;
    s.octave = semis < 0 ? -1 : (semis <= 12 ? 0 : 1);
    s.note = semis - 12 * s.octave;
    return s;
}

// --------------------------------------------------------------------------------------- batteria
using Rows = std::array<uint32_t, numDrums + 1>;

void put (Rows& d, int row, std::initializer_list<int> steps) { for (int s : steps) d[(size_t) row] |= 1u << s; }
void put (Rows& d, int row, int s) { d[(size_t) row] |= 1u << s; }
void del (Rows& d, int row, int s) { d[(size_t) row] &= ~(1u << s); }

// sceglie k passi diversi da una lista
std::vector<int> pickSome (juce::Random& r, std::vector<int> from, int k)
{
    std::vector<int> out;
    while (k-- > 0 && ! from.empty())
    {
        const int i = r.nextInt ((int) from.size());
        out.push_back (from[(size_t) i]);
        from.erase (from.begin() + i);
    }
    return out;
}

Rows makeDrums (int style, juce::Random& r)
{
    Rows d {};
    auto offbeats = { 2, 6, 10, 14 };
    auto eighths = { 0, 2, 4, 6, 8, 10, 12, 14 };
    auto openOver = [&] (int s) { put (d, OH, s); del (d, CH, s); };   // il chiuso fermerebbe l'aperto

    switch (style)
    {
        case ACID:
        case TECHNO:
        {
            put (d, BD, { 0, 4, 8, 12 });
            if (chance (r, style == ACID ? .25f : .15f)) put (d, BD, chance (r, .5f) ? 14 : 15);
            const int hat = pickWeighted (r, { { 0, 50 }, { 1, 30 }, { 2, 20 } });
            if (hat == 0) put (d, CH, offbeats);
            else if (hat == 1) { for (int s = 0; s < 16; ++s) put (d, CH, s); }
            else put (d, CH, eighths);
            if (hat != 0 && chance (r, .6f)) for (int s : offbeats) openOver (s);
            else if (hat == 0 && chance (r, .35f)) openOver (14);
            if (chance (r, style == ACID ? .7f : .5f)) put (d, CP, { 4, 12 });
            else put (d, SD, { 4, 12 });
            if (style == ACID && chance (r, .25f)) for (int s : pickSome (r, { 3, 6, 7, 10, 11, 13, 15 }, 2 + r.nextInt (2))) put (d, CB, s);
            if (style == TECHNO && chance (r, .6f)) for (int s : pickSome (r, { 3, 6, 7, 10, 11, 13, 14 }, 3 + r.nextInt (3))) put (d, RS, s);
            else if (chance (r, .2f)) for (int s : pickSome (r, { 3, 7, 11, 15 }, 2)) put (d, RS, s);
            if (style == TECHNO && chance (r, .15f)) { put (d, LT, 13); put (d, MT, 15); }
            if (chance (r, .1f)) put (d, CY, 0);
            put (d, accentRow, { 0, 4, 8, 12 });
            if (chance (r, .4f)) put (d, accentRow, 14);
            break;
        }
        case FUNK:
        {
            put (d, BD, 0);
            if (chance (r, .5f)) put (d, BD, 8);
            for (int s : pickSome (r, { 3, 6, 7, 10, 11, 14 }, 2 + r.nextInt (2))) put (d, BD, s);
            put (d, SD, { 4, 12 });
            if (chance (r, .5f)) put (d, CP, { 4, 12 });
            if (chance (r, .3f)) put (d, SD, chance (r, .5f) ? 7 : 15);
            for (int s = 0; s < 16; ++s) if (! chance (r, .2f)) put (d, CH, s);
            if (chance (r, .6f)) openOver (14); else if (chance (r, .4f)) openOver (6);
            if (chance (r, .3f)) for (int s : pickSome (r, { 0, 3, 6, 8, 10, 13 }, 3 + r.nextInt (2))) put (d, CB, s);
            if (chance (r, .25f)) { put (d, HT, 13); put (d, MT, 14); put (d, LT, 15); }
            put (d, accentRow, { 0, 4, 12 });
            for (int s : pickSome (r, { 3, 6, 10, 14 }, 2)) put (d, accentRow, s);
            break;
        }
        case HIPHOP:
        {
            put (d, BD, 0);
            for (int s : pickSome (r, { 3, 7, 8, 9, 10, 11, 14 }, 2 + r.nextInt (2))) put (d, BD, s);
            put (d, SD, { 4, 12 });
            if (chance (r, .5f)) put (d, CP, { 4, 12 });
            if (chance (r, .6f)) put (d, CH, eighths);
            else for (int s = 0; s < 16; ++s) put (d, CH, s);
            if (chance (r, .3f)) put (d, CH, { 13, 14, 15 });                  // rullata di charleston
            if (chance (r, .1f)) openOver (14);
            if (chance (r, .25f)) for (int s : pickSome (r, { 6, 11, 15 }, 1 + r.nextInt (2))) put (d, RS, s);
            if (chance (r, .15f)) put (d, CY, 0);
            put (d, accentRow, { 0, 4, 12 });
            break;
        }
        case RNB:
        default:
        {
            put (d, BD, 0);
            for (int s : pickSome (r, { 3, 6, 9, 10, 11 }, 1 + r.nextInt (2))) put (d, BD, s);
            const bool clap = chance (r, .6f);
            if (clap) put (d, CP, { 4, 12 });
            if (! clap || chance (r, .5f)) put (d, SD, { 4, 12 });
            if (chance (r, .35f)) put (d, RS, chance (r, .5f) ? 7 : 15);
            if (chance (r, .5f)) { for (int s = 0; s < 16; ++s) put (d, CH, s); }
            else put (d, CH, eighths);
            if (chance (r, .15f)) openOver (6);
            if (chance (r, .1f)) { put (d, MT, 14); put (d, LT, 15); }
            put (d, accentRow, { 4, 12 });
            break;
        }
    }
    return d;
}

// --------------------------------------------------------------------------------------- bassi
std::array<BassStep, numSteps> makeBass1 (int style, int root, int scale, uint32_t kicks, juce::Random& r)
{
    const Feel f = feelFor (style, r);
    const int n = (int) scaleDefs()[(size_t) scale].steps.size();
    const bool followKick = style == HIPHOP || style == RNB || (style == FUNK && chance (r, .6f));

    // gate del motivo
    const int L = f.motif;
    std::array<bool, numSteps> gate {};
    for (int s = 0; s < L; ++s)
    {
        float p = f.density;
        if (style == FUNK && (s % 2) == 1) p += .1f;           // il funk vive di controtempi
        if (style == TECHNO && (s % 4) == 0) p -= .25f;        // il basso techno evita la cassa
        gate[(size_t) s] = s == 0 ? style != TECHNO || chance (r, .3f) : chance (r, p);
    }
    if (followKick)
        for (int s = 0; s < numSteps; ++s) if ((kicks >> s) & 1u) gate[(size_t) (s % L)] = true;
    if (style == TECHNO) { bool any = false; for (int s = 0; s < L; ++s) any |= gate[(size_t) s]; if (! any) gate[2] = true; }

    // note del motivo: passeggiata sui gradi della scala con ritorni alla tonica
    std::array<BassStep, numSteps> line {};
    int deg = 0;
    for (int s = 0; s < L; ++s)
    {
        if (! gate[(size_t) s]) continue;
        if (s == 0 || chance (r, f.rootBias)) deg = chance (r, .15f) ? n : 0;          // tonica o la sua ottava
        else deg = juce::jlimit (-n, 2 * n - 1, deg + pickWeighted (r, { { -2, 2 }, { -1, 3 }, { 1, 3 }, { 2, 2 }, { 4, 1 }, { -3, 1 } }));
        int semis = degreePitch (root, scale, deg);
        if (chance (r, f.octUp)) semis += 12;
        else if (chance (r, f.octDown)) semis -= 12;
        BassStep st = toStep (semis);
        st.accent = chance (r, f.accent * (style == TECHNO && s % 4 == 2 ? 1.6f : 1.f));
        line[(size_t) s] = st;
    }
    // ripetizione del motivo con variazioni
    for (int s = L; s < numSteps; ++s)
    {
        line[(size_t) s] = line[(size_t) (s % L)];
        if (chance (r, f.variation) && ! (followKick && ((kicks >> s) & 1u)))
        {
            if (line[(size_t) s].gate && chance (r, .35f)) line[(size_t) s].gate = false;
            else
            {
                BassStep st = toStep (degreePitch (root, scale, r.nextInt (n + 1)) + (chance (r, f.octUp) ? 12 : 0));
                st.accent = chance (r, f.accent);
                line[(size_t) s] = st;
            }
        }
        if (followKick && ((kicks >> s) & 1u) && ! line[(size_t) s].gate) line[(size_t) s] = toStep (degreePitch (root, scale, 0));
    }
    // note tenute (bassi lunghi tipo hip hop / R&B): lo slide verso la stessa nota la prolunga
    for (int s = 0; s < numSteps - 1; ++s)
    {
        auto& a = line[(size_t) s];
        auto& b = line[(size_t) s + 1];
        if (a.gate && ! b.gate && chance (r, f.tie))
        {
            b = a; b.accent = false; a.slide = true;
            ++s;
        }
    }
    // slide fra note diverse
    for (int s = 0; s < numSteps - 1; ++s)
        if (line[(size_t) s].gate && line[(size_t) s + 1].gate && ! line[(size_t) s].slide && chance (r, f.slide)) line[(size_t) s].slide = true;
    return line;
}

std::array<BassStep, numSteps> makeBass2 (int style, int root, int scale, const std::array<BassStep, numSteps>& b1, juce::Random& r)
{
    std::array<BassStep, numSteps> line {};
    const int n = (int) scaleDefs()[(size_t) scale].steps.size();
    // note dell'accordo: gradi 0, 2, 4 e (nell'R&B e nel funk) 6, un'ottava sopra
    std::vector<int> chord { 0, 2, 4 };
    if (style == RNB || style == FUNK || style == HIPHOP) chord.push_back (n >= 7 ? 6 : 4);
    auto chordNote = [&] { return degreePitch (root, scale, chord[(size_t) r.nextInt ((int) chord.size())]) + 12; };

    if (style == HIPHOP || style == RNB)
    {
        // poche note lunghe, come un pad o un lead morbido
        int s = r.nextInt (3);
        while (s < numSteps)
        {
            const int len = 2 + r.nextInt (3);
            const BassStep st = toStep (chordNote());
            for (int k = 0; k < len && s + k < numSteps; ++k)
            {
                line[(size_t) (s + k)] = st;
                line[(size_t) (s + k)].slide = k < len - 1;
            }
            line[(size_t) s].accent = chance (r, .3f);
            s += len + 1 + r.nextInt (5);
        }
        return line;
    }
    const float dens = style == FUNK ? .35f : .3f;
    for (int s = 0; s < numSteps; ++s)
    {
        float p = dens;
        if (! b1[(size_t) s].gate) p += .25f;                     // risponde dove BASS 1 tace
        if (style == FUNK && (s % 4 == 2 || s % 4 == 3)) p += .15f;
        if (s % 4 == 0) p -= .15f;
        if (! chance (r, p)) continue;
        BassStep st = toStep (chordNote() + (chance (r, .2f) ? 12 : 0));
        st.accent = chance (r, .35f);
        line[(size_t) s] = st;
    }
    for (int s = 0; s < numSteps - 1; ++s)
        if (line[(size_t) s].gate && line[(size_t) s + 1].gate && chance (r, style == FUNK ? .1f : .3f)) line[(size_t) s].slide = true;
    return line;
}
}

// --------------------------------------------------------------------------------------- interfaccia
const char* styleName (int s)
{
    static const char* n[] = { "AUTO", "ACID", "TECHNO", "FUNK", "HIP HOP", "R&B" };
    return s >= 0 && s < numStyles ? n[s] : "AUTO";
}

const char* scaleName (int s) { return s >= 0 && s < numScales() ? scaleDefs()[(size_t) s].name : ""; }
const std::vector<int>& scaleSteps (int s) { return scaleDefs()[(size_t) juce::jlimit (0, numScales() - 1, s)].steps; }
int numScales() { return (int) scaleDefs().size(); }

juce::String keyName (int root)
{
    static const char* n[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return n[((root % 12) + 12) % 12];
}

Result generate (PatternStore& ps, int pattern, int what, int style, juce::Random& r)
{
    Result res;
    const auto key = ps.key (pattern);
    const bool keep = what != 3 && key.root >= 0;          // una sola traccia: resta nella tonalità del pattern
    if (keep)
    {
        res.style = style == AUTO ? juce::jlimit ((int) ACID, (int) RNB, key.style) : style;
        res.root = key.root;
        res.scale = key.scale;
    }
    else
    {
        res.style = style != AUTO ? style
                                  : pickWeighted (r, { { ACID, 35 }, { TECHNO, 25 }, { FUNK, 15 }, { HIPHOP, 15 }, { RNB, 10 } });
        const auto& sc = scalesFor (res.style);
        res.scale = sc[(size_t) r.nextInt ((int) sc.size())];
        static const int roots[] = { 0, 2, 4, 5, 7, 9 };       // tonalità comode per un basso: C D E F G A
        res.root = roots[r.nextInt (6)];
    }
    const Feel f = feelFor (res.style, r);
    res.bpm = juce::roundToInt ((f.bpmLo + r.nextFloat() * (f.bpmHi - f.bpmLo)) * 2.f) / 2.f;
    res.swing = juce::roundToInt ((f.swingLo + r.nextFloat() * (f.swingHi - f.swingLo)) * 2.f) / 2.f;

    // la batteria serve anche ai bassi (cassa); se non la rigeneriamo usiamo quella del pattern
    Rows drums {};
    if (what == 2 || what == 3) drums = makeDrums (res.style, r);
    else for (int row = 0; row <= numDrums; ++row) drums[(size_t) row] = ps.drumRow (pattern, row);

    std::array<BassStep, numSteps> b1 {};
    for (int s = 0; s < numSteps; ++s) b1[(size_t) s] = ps.bass (pattern, 0, s);
    if (what == 0 || what == 3) b1 = makeBass1 (res.style, res.root, res.scale, drums[BD], r);
    std::array<BassStep, numSteps> b2 {};
    if (what == 1 || what == 3) b2 = makeBass2 (res.style, res.root, res.scale, b1, r);

    if (what == 0 || what == 3) for (int s = 0; s < numSteps; ++s) ps.setBass (pattern, 0, s, b1[(size_t) s]);
    if (what == 1 || what == 3) for (int s = 0; s < numSteps; ++s) ps.setBass (pattern, 1, s, b2[(size_t) s]);
    if (what == 2 || what == 3)
        for (int row = 0; row <= numDrums; ++row)
            for (int s = 0; s < numSteps; ++s) ps.setDrum (pattern, row, s, (drums[(size_t) row] >> s) & 1u);
    ps.setKey (pattern, { res.root, res.scale, res.style });
    return res;
}
}
