#include "Engine.h"

namespace acido
{
namespace
{
constexpr double twoPi = 6.283185307179586;

inline float polyBlep (double t, double dt) noexcept
{
    if (t < dt)       { t /= dt; return (float) (t + t - t * t - 1.0); }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
    return 0.f;
}

inline float squareBlep (double ph, double dt) noexcept
{
    float v = ph < .5 ? 1.f : -1.f;
    v += polyBlep (ph, dt);
    double p2 = ph + .5; if (p2 >= 1.0) p2 -= 1.0;
    v -= polyBlep (p2, dt);
    return v;
}

inline float fastSin (double phase) noexcept { return (float) std::sin (twoPi * phase); }

inline void advance (double& ph, double inc) noexcept { ph += inc; if (ph >= 1.0) ph -= std::floor (ph); }

inline float onePoleA (double sr, double hz) noexcept { return (float) (1.0 - std::exp (-twoPi * hz / sr)); }
}

// =========================================================================================== Biquad (RBJ)
static void rbj (Biquad& q, double b0, double b1, double b2, double a0, double a1, double a2)
{
    q.b0 = (float) (b0 / a0); q.b1 = (float) (b1 / a0); q.b2 = (float) (b2 / a0);
    q.a1 = (float) (a1 / a0); q.a2 = (float) (a2 / a0);
}

void Biquad::setBandPass (double sr, double f, double q)
{
    const double w = twoPi * juce::jmin (f, sr * .45) / sr, a = std::sin (w) / (2 * q), c = std::cos (w);
    rbj (*this, a, 0, -a, 1 + a, -2 * c, 1 - a);
}

void Biquad::setHighPass (double sr, double f, double q)
{
    const double w = twoPi * juce::jmin (f, sr * .45) / sr, a = std::sin (w) / (2 * q), c = std::cos (w);
    rbj (*this, (1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + a, -2 * c, 1 - a);
}

void Biquad::setLowPass (double sr, double f, double q)
{
    const double w = twoPi * juce::jmin (f, sr * .45) / sr, a = std::sin (w) / (2 * q), c = std::cos (w);
    rbj (*this, (1 - c) / 2, 1 - c, (1 - c) / 2, 1 + a, -2 * c, 1 - a);
}

// =========================================================================================== voce acida
void AcidVoice::prepare (double sampleRate)
{
    sr = sampleRate;
    glideCoef = coefFor (sr, .040);     // slide: circa 60 ms per arrivare alla nota
    vegAtt    = coefFor (sr, .0012);
    vegHold   = coefFor (sr, 3.0);      // l'ampiezza cala piano mentre il gate è aperto
    vegRel    = coefFor (sr, .0045);
    accAtt    = coefFor (sr, .045);     // condensatore dell'accento: carica lenta, si accumula
    accRel    = coefFor (sr, .22);
    smooth2ms = coefFor (sr, .002);
    hpCoef    = onePoleA (sr, 24.0);
    panic();
}

void AcidVoice::panic() noexcept
{
    gate = gliding = accNote = attacking = false;
    meg = veg = accCap = 0; accGain = 1;
    for (auto& v : s) v = 0;
    hpState = lastIn = 0; phase = 0;
}

void AcidVoice::trigger (int midiNote, bool accent, bool slide) noexcept
{
    const float n = (float) juce::jlimit (12, 108, midiNote);
    accNote = accent;
    if (slide && gate)
    {
        target = n;
        gliding = true;
        return;
    }
    pitch = target = n;
    gliding = false;
    meg = 1.f;
    megCoef = coefFor (sr, accent ? .09 : .08 * std::pow (25.0, (double) cfg.decay));
    attacking = true;
    gate = true;
}

float AcidVoice::process() noexcept
{
    if (! gate && veg < 1e-5f && accCap < 1e-5f) { veg = 0; return 0.f; }

    if (gliding) pitch += (target - pitch) * (1.f - glideCoef);
    const double f = 440.0 * std::pow (2.0, (pitch + cfg.tune - 69.0) / 12.0);
    const double dt = juce::jmin (f / sr, .45);
    advance (phase, dt);
    const float osc = cfg.wave == 0 ? (float) (2.0 * phase - 1.0) - polyBlep (phase, dt)
                                    : squareBlep (phase, dt) * .85f;

    // inviluppi
    meg *= megCoef;
    const float accIn = accNote ? meg : 0.f;
    accCap += (accIn - accCap) * (1.f - (accIn > accCap ? accAtt : accRel));
    if (gate)
    {
        if (attacking) { veg += (1.02f - veg) * (1.f - vegAtt); if (veg >= 1.f) { veg = 1.f; attacking = false; } }
        else veg *= vegHold;
    }
    else veg *= vegRel;
    const float accTarget = accNote ? 1.f + cfg.accent * 1.1f : 1.f;
    accGain += (accTarget - accGain) * (1.f - smooth2ms);

    // frequenza di taglio: base + inviluppo + sweep dell'accento
    const float base = 35.f * std::exp2 (cfg.cutoff * 7.f - cfg.envMod * 1.1f);
    const float oct = cfg.envMod * 5.5f * meg + cfg.accent * 4.f * accCap;
    float fc = base * std::exp2 (oct);
    fc = juce::jlimit (20.f, (float) (sr * .42), fc);
    cutSm += (fc - cutSm) * .35f;

    // passa-basso a 4 poli (struttura ladder, integratori trapezoidali) con saturazione all'ingresso
    const float g = std::tan ((float) juce::MathConstants<double>::pi * cutSm / (float) sr);
    const float G = g / (1.f + g), inv = 1.f / (1.f + g);
    const float k = 3.9f * cfg.reso;
    const float S1 = s[0] * inv, S2 = s[1] * inv, S3 = s[2] * inv, S4 = s[3] * inv;
    const float G2 = G * G, G3 = G2 * G, G4 = G3 * G;
    const float x = osc * .55f;
    float u = (x - k * (G3 * S1 + G2 * S2 + G * S3 + S4)) / (1.f + k * G4);
    u = std::tanh (u * 1.2f) / 1.2f;
    float y = u;
    for (int i = 0; i < 4; ++i)
    {
        const float v = (y - s[i]) * G;
        const float o = v + s[i];
        s[i] = o + v;
        y = o;
    }
    float out = y * (1.f + .5f * k) * veg * accGain;

    // drive
    const float dr = 1.f + 8.f * cfg.drive;
    out = std::tanh (out * dr) / std::tanh (dr) * .9f;

    // taglio sotto i 24 Hz
    hpState += (out - hpState) * hpCoef;
    out -= hpState;
    if (! std::isfinite (out)) { panic(); return 0.f; }
    return out;
}

// =========================================================================================== batteria
void DrumKit::prepare (double sampleRate)
{
    sr = sampleRate;
    static const double metal[6] = { 205.3, 304.4, 369.6, 522.7, 540.0, 800.0 };   // banco di quadre
    for (int i = 0; i < 6; ++i) { mInc[(size_t) i] = metal[i] / sr; mPh[(size_t) i] = i * .17; }
    hatHp1.setHighPass (sr, 7200, .707);
    hatHp2.setHighPass (sr, 6000, .9);
    cyBpLo.setBandPass (sr, 3400, 1.3);
    cyHpHi.setHighPass (sr, 7800, .707);
    cbBp.setBandPass (sr, 1050, .9);
    sdHp.setHighPass (sr, 1600, .707);
    sdLp.setLowPass (sr, 9500, .707);
    rsHp.setHighPass (sr, 380, .707);
    cpBp.setBandPass (sr, 1150, 1.6);
    cpBp2.setBandPass (sr, 950, 1.0);
    for (auto& f : tomNoiseLp) f.setLowPass (sr, 2200, .707);
    set (cfg);
    panic();
}

void DrumKit::set (const DrumSettings& s) noexcept
{
    cfg = s;
    cBdAmp = coefFor (sr, .045 + .33 * cfg.bdDecay);
    cBdPitch = coefFor (sr, .011);
    cBdClick = coefFor (sr, .0011);
    bdLpA = onePoleA (sr, 600.0 * std::exp2 (cfg.bdTone * 3.6));
    cSd1 = coefFor (sr, .045); cSd2 = coefFor (sr, .03); cSdN = coefFor (sr, .075);
    static const float tomBase[3] = { 84.f, 124.f, 172.f };
    static const float tomTau[3] = { .30f, .24f, .19f };
    for (int i = 0; i < 3; ++i)
    {
        tomF[i] = tomBase[i] * std::exp2 ((cfg.tomTune[i] - .5f) * 1.2f);
        cTom[i] = coefFor (sr, tomTau[i]);
    }
    cTomP = coefFor (sr, .045); cTomN = coefFor (sr, .012);
    cRs = coefFor (sr, .010);
    cCbF = coefFor (sr, .011); cCbS = coefFor (sr, .13);
    cCyLo = coefFor (sr, .22 + 1.1 * cfg.cyDecay); cCyHi = coefFor (sr, .07 + .3 * cfg.cyDecay);
    cOh = coefFor (sr, .05 + .45 * cfg.ohDecay); cCh = coefFor (sr, .017); cChoke = coefFor (sr, .004);
}

void DrumKit::panic() noexcept
{
    bdAmp = bdPitch = bdClick = bdLp = 0;
    sdA1 = sdA2 = sdN = 0;
    for (int i = 0; i < 3; ++i) tomAmp[i] = tomPitch[i] = tomN[i] = 0;
    rsAmp = 0; cpT = 1e9f; cpTail = 0;
    ohAmp = chAmp = cyAmpLo = cyAmpHi = cbAmpFast = cbAmpSlow = 0;
    for (auto* q : { &hatHp1, &hatHp2, &cyBpLo, &cyHpHi, &cbBp, &sdHp, &sdLp, &rsHp, &cpBp, &cpBp2 }) q->reset();
}

void DrumKit::trigger (int d, float g) noexcept
{
    if (d < 0 || d >= numDrums) return;
    gain[(size_t) d] = g;
    switch (d)
    {
        case BD: bdAmp = 1; bdPitch = 1; bdClick = 1; bdPhase = 0; break;
        case SD: sdA1 = sdA2 = sdN = 1; sdPh1 = sdPh2 = 0; break;
        case LT: case MT: case HT: { const int i = d - LT; tomAmp[i] = tomPitch[i] = tomN[i] = 1; tomPh[i] = 0; break; }
        case RS: rsAmp = 1; rsPh1 = rsPh2 = 0; break;
        case CP: cpT = 0; cpTail = 1; break;
        case CB: cbAmpFast = cbAmpSlow = 1; break;
        case CY: cyAmpLo = cyAmpHi = 1; break;
        case OH: ohAmp = 1; ohChoke = false; break;
        case CH: chAmp = 1; ohChoke = true; break;     // il charleston chiuso ferma quello aperto
        default: break;
    }
}

void DrumKit::process (std::array<float, numDrums>& out) noexcept
{
    out.fill (0.f);
    const double isr = 1.0 / sr;

    // banco metallico, sempre in corsa come nell'originale analogico
    float m = 0, cb = 0;
    for (size_t i = 0; i < 6; ++i)
    {
        const float v = squareBlep (mPh[i], mInc[i]);
        advance (mPh[i], mInc[i]);
        m += v;
        if (i >= 4) cb += v;
    }
    m *= 1.f / 6.f; cb *= .5f;

    if (bdAmp > 1e-5f)
    {
        const double f = 49.0 * (1.0 + .9 * bdPitch);
        advance (bdPhase, f * isr);
        bdLp += (bdClick - bdLp) * bdLpA;
        out[BD] = std::tanh (1.3f * (fastSin (bdPhase) * bdAmp + bdLp * .55f)) * gain[BD];
        bdAmp *= cBdAmp; bdPitch *= cBdPitch; bdClick *= cBdClick;
    }
    if (sdA1 > 1e-5f || sdN > 1e-5f)
    {
        advance (sdPh1, 182.0 * (1.0 + .12 * sdA1) * isr);
        advance (sdPh2, 331.0 * isr);
        const float n = sdLp.process (sdHp.process (noise.next()));
        const float lowG = 1.f - cfg.sdTone * .6f, highG = .35f + cfg.sdTone * .65f;
        out[SD] = ((fastSin (sdPh1) * sdA1 * lowG + fastSin (sdPh2) * sdA2 * highG) * .62f
                   + n * sdN * cfg.sdSnappy * 1.6f) * gain[SD];
        sdA1 *= cSd1; sdA2 *= cSd2; sdN *= cSdN;
    }
    for (int i = 0; i < 3; ++i)
        if (tomAmp[i] > 1e-5f)
        {
            advance (tomPh[i], tomF[i] * (1.0 + .28 * tomPitch[i]) * isr);
            const float n = tomNoiseLp[i].process (noise.next());
            out[(size_t) (LT + i)] = (fastSin (tomPh[i]) * tomAmp[i] + n * tomN[i] * .2f) * .9f * gain[(size_t) (LT + i)];
            tomAmp[i] *= cTom[i]; tomPitch[i] *= cTomP; tomN[i] *= cTomN;
        }
    if (rsAmp > 1e-5f)
    {
        advance (rsPh1, 503.0 * isr);
        advance (rsPh2, 1720.0 * isr);
        const float v = rsHp.process ((fastSin (rsPh1) * .6f + fastSin (rsPh2)) * rsAmp);
        out[RS] = std::tanh (2.2f * v) * .7f * gain[RS];
        rsAmp *= cRs;
    }
    if (cpT < 1.2f)
    {
        const float n = noise.next();
        float burst = 0;
        if (cpT < .044f) burst = std::exp (-std::fmod (cpT, .0105f) / .0032f);
        const float tail = std::exp (-cpT / .12f) * juce::jmin (1.f, cpT / .025f);
        out[CP] = (cpBp.process (n) * burst * 1.6f + cpBp2.process (n) * tail * 1.1f) * gain[CP];
        cpT += (float) isr;
    }
    if (cbAmpSlow > 1e-5f)
    {
        out[CB] = cbBp.process (cb) * (.6f * cbAmpFast + .4f * cbAmpSlow) * 1.5f * gain[CB];
        cbAmpFast *= cCbF; cbAmpSlow *= cCbS;
    }
    if (cyAmpLo > 1e-5f)
    {
        const float lo = cyBpLo.process (m) * cyAmpLo * (1.f - cfg.cyTone * .7f);
        const float hi = cyHpHi.process (m) * cyAmpHi * (.3f + cfg.cyTone * .7f);
        out[CY] = (lo * 1.3f + hi * 1.6f) * gain[CY];
        cyAmpLo *= cCyLo; cyAmpHi *= cCyHi;
    }
    if (ohAmp > 1e-5f || chAmp > 1e-5f)
    {
        const float h = hatHp2.process (hatHp1.process (m));
        out[OH] = h * ohAmp * 2.4f * gain[OH];
        out[CH] = h * chAmp * 2.4f * gain[CH];
        ohAmp *= ohChoke ? cChoke : cOh;
        chAmp *= cCh;
    }
}

// =========================================================================================== delay
void PingPongDelay::prepare (double sampleRate)
{
    const size_t n = (size_t) (sampleRate * 4.0) + 8;
    bufL.assign (n, 0.f); bufR.assign (n, 0.f);
    reset();
}

void PingPongDelay::reset()
{
    std::fill (bufL.begin(), bufL.end(), 0.f);
    std::fill (bufR.begin(), bufR.end(), 0.f);
    w = 0; lpL = lpR = hpL = hpR = 0;
}

float PingPongDelay::read (const std::vector<float>& b, float delay) const noexcept
{
    const int n = (int) b.size();
    float rp = (float) w - delay;
    while (rp < 0) rp += (float) n;
    const int i0 = (int) rp;
    const float fr = rp - (float) i0;
    const int i1 = (i0 + 1) % n;
    return b[(size_t) (i0 % n)] * (1.f - fr) + b[(size_t) i1] * fr;
}

void PingPongDelay::process (float inL, float inR, float& outL, float& outR, float timeSamples, float feedback, float toneA) noexcept
{
    if (bufL.empty()) { outL = outR = 0; return; }
    const float maxT = (float) bufL.size() - 4.f;
    timeSamples = juce::jlimit (1.f, maxT, timeSamples);
    if (timeSm <= 0) timeSm = timeSamples;
    timeSm += (timeSamples - timeSm) * .0006f;
    float dL = read (bufL, timeSm), dR = read (bufR, timeSm);
    lpL += (dL - lpL) * toneA; lpR += (dR - lpR) * toneA;
    hpL += (lpL - hpL) * .012f; hpR += (lpR - hpR) * .012f;     // toglie il fango sotto i 100 Hz circa
    const float fbL = std::tanh ((lpL - hpL) * feedback), fbR = std::tanh ((lpR - hpR) * feedback);
    bufL[(size_t) w] = .5f * (inL + inR) + fbR;
    bufR[(size_t) w] = fbL;
    if (++w >= (int) bufL.size()) w = 0;
    outL = dL; outR = dR;
}

// =========================================================================================== motore
void Engine::prepare (double sampleRate, int)
{
    sr = sampleRate;
    for (auto& b : bass) b.prepare (sr);
    drums.prepare (sr);
    delay.prepare (sr);
    gainSm.fill (0.f);
    stopSequencer();
    hostRun = false;
}

void Engine::panic() noexcept
{
    for (auto& b : bass) b.panic();
    drums.panic();
    delay.reset();
    for (int t = 0; t < numBass; ++t) { heldCount[t] = 0; offAt[t] = 1e18; prevStep[t] = {}; }
}

long long Engine::stepIndexAt (double p, double oddStart) noexcept
{
    const double pair = std::floor (p / 2.0);
    const double frac = p - 2.0 * pair;
    return (long long) pair * 2 + (frac >= oddStart ? 1 : 0);
}

double Engine::stepStart (long long idx, double oddStart) noexcept
{
    const long long pair = (idx >= 0 ? idx : idx - 1) / 2;
    return (double) pair * 2.0 + ((idx & 1) ? oddStart : 0.0);
}

void Engine::stopSequencer() noexcept
{
    for (int t = 0; t < numBass; ++t)
    {
        if (heldCount[t] == 0) bass[t].release();
        offAt[t] = 1e18;
        prevStep[t] = {};
    }
    running = false;
    lastStep = -1;
    needPattern = true;
    lastWritten[0] = lastWritten[1] = -1;
    skipDrumIdx = skipBassIdx[0] = skipBassIdx[1] = std::numeric_limits<long long>::min();
    chainPosUi.store (-1);
    for (auto& s : stepNow) s.store (-1);
}

static int wrapStep (long long idx, int len) noexcept
{
    const long long m = idx % len;
    return (int) (m < 0 ? m + len : m);
}

void Engine::onStep (long long idx, double oddStart, const ParamReader& pr) noexcept
{
    if (needPattern || wrapStep (idx, 16) == 0)
    {
        const int cl = patterns.chainLength();
        if (pr.b (CHAIN) && cl > 0)
        {
            // catena: la battuta (16 passi) sceglie la voce della catena, così in una DAW la posizione decide il pattern
            const long long bar = idx >= 0 ? idx / 16 : (idx - 15) / 16;
            const int cp = wrapStep (bar, cl);
            playPattern = patterns.chainAt (cp);
            chainPosUi.store (cp);
        }
        else
        {
            playPattern = juce::jlimit (0, numPatterns - 1, pr.i (PATTERN) - 1);
            chainPosUi.store (-1);
        }
        needPattern = false;
    }
    const int pat = playPattern;

    // batteria
    {
        const int s = wrapStep (idx, juce::jlimit (1, 16, pr.i (DRLEN)));
        stepNow[2].store (s);
        if (! pr.b (DRMUTE))
        {
            const bool acc = patterns.drum (pat, accentRow, s);
            const float g = acc ? .72f + .55f * pr[AC] : .72f;
            uint32_t hit = 0;
            // un colpo appena scritto in anticipo su questo passo è già suonato: non si ribatte
            const uint32_t skip = idx == skipDrumIdx ? skipDrumMask : 0u;
            for (int r = 0; r < numDrums; ++r)
                if (r != OH && ! (skip & (1u << r)) && patterns.drum (pat, r, s)) { drums.trigger (r, g); hit |= 1u << r; }
            if (! (skip & (1u << OH)) && patterns.drum (pat, OH, s)) { drums.trigger (OH, g); hit |= 1u << OH; }
            if (hit) hits.fetch_or (hit);
        }
    }

    // bassi
    const double start = stepStart (idx, oddStart), len = stepStart (idx + 1, oddStart) - start;
    for (int t = 0; t < numBass; ++t)
    {
        const P lenP = t == 0 ? B1LEN : B2LEN, muteP = t == 0 ? B1MUTE : B2MUTE, transP = t == 0 ? B1TRANS : B2TRANS;
        const int s = wrapStep (idx, juce::jlimit (1, 16, pr.i (lenP)));
        stepNow[(size_t) t].store (s);
        BassStep st = pr.b (muteP) ? BassStep() : patterns.bass (pat, t, s);
        if (heldCount[t] > 0 || idx == skipBassIdx[t]) { prevStep[t] = st; continue; }   // la tastiera MIDI ha la precedenza
        if (st.gate)
        {
            const bool slideIn = prevStep[t].gate && prevStep[t].slide && bass[t].isGateOn();
            bass[t].trigger (st.midiNote (pr.i (transP)), st.accent, slideIn);
            offAt[t] = st.slide ? 1e18 : start + len * .55;
        }
        else
        {
            bass[t].release();
            offAt[t] = 1e18;
        }
        prevStep[t] = st;
    }
}

// ------------------------------------------------------------------------------------------- tap write
long long Engine::nearestStep (double& late) const noexcept
{
    // il passo più vicino alla posizione attuale: oltre metà passo si scrive sul passo dopo
    const long long idx = stepIndexAt (pos, curOddStart);
    const double a = stepStart (idx, curOddStart), b = stepStart (idx + 1, curOddStart);
    late = pos - a;
    return (pos - a) > (b - a) * .5 ? idx + 1 : idx;
}

void Engine::writeDrum (int drum, bool accent, const ParamReader& pr) noexcept
{
    double late;
    const long long idx = nearestStep (late);
    const int s = wrapStep (idx, juce::jlimit (1, 16, pr.i (DRLEN)));
    patterns.setDrum (playPattern, drum, s, true);
    if (accent) patterns.setDrum (playPattern, accentRow, s, true);
    if (idx > lastStep)
    {
        if (skipDrumIdx != idx) { skipDrumIdx = idx; skipDrumMask = 0; }
        skipDrumMask |= 1u << drum;
    }
}

void Engine::writeBass (int t, int note, int vel, bool legato, const ParamReader& pr) noexcept
{
    double late;
    const long long idx = nearestStep (late);
    const int s = wrapStep (idx, juce::jlimit (1, 16, pr.i (t == 0 ? B1LEN : B2LEN)));
    // la nota diventa nota + ottava del passo, riferita a C2 (36) meno la trasposizione della traccia
    const int rel = note - 36 - pr.i (t == 0 ? B1TRANS : B2TRANS);
    // ottava 0 copre C..C sopra (0..12): si sale o si scende solo fuori da lì
    const int oct = rel > 12 ? 1 : (rel < 0 ? -1 : 0);
    BassStep st;
    st.note = juce::jlimit (0, 12, rel - 12 * oct);
    st.octave = oct;
    st.gate = true;
    st.accent = vel > 100;
    patterns.setBass (playPattern, t, s, st);
    if (legato && lastWritten[t] >= 0 && lastWritten[t] != s)
    {
        auto prev = patterns.bass (playPattern, t, lastWritten[t]);
        prev.slide = true;
        patterns.setBass (playPattern, t, lastWritten[t], prev);
    }
    lastWritten[t] = s;
    if (idx > lastStep) skipBassIdx[t] = idx;
}

void Engine::bassNoteOn (int t, int note, int vel, const ParamReader& pr) noexcept
{
    auto& h = held[t];
    int& n = heldCount[t];
    if (running && pr.b (WRITE)) writeBass (t, note, vel, n > 0, pr);
    for (int i = 0; i < n; ++i) if (h[(size_t) i] == note) { for (int j = i; j < n - 1; ++j) h[(size_t) j] = h[(size_t) j + 1]; --n; break; }
    const bool legato = n > 0 && bass[t].isGateOn();
    if (n < (int) h.size()) h[(size_t) n++] = note;
    bass[t].trigger (note, vel > 100, legato);
    offAt[t] = 1e18;
}

void Engine::bassNoteOff (int t, int note) noexcept
{
    auto& h = held[t];
    int& n = heldCount[t];
    int found = -1;
    for (int i = 0; i < n; ++i) if (h[(size_t) i] == note) { found = i; break; }
    if (found < 0) return;
    const bool wasTop = found == n - 1;
    for (int j = found; j < n - 1; ++j) h[(size_t) j] = h[(size_t) j + 1];
    --n;
    if (! wasTop) return;
    if (n > 0) bass[t].trigger (h[(size_t) n - 1], false, true);
    else bass[t].release();
}

void Engine::handleMidi (const juce::MidiMessage& m, const ParamReader& pr) noexcept
{
    const int ch = m.getChannel();
    if (m.isController())
    {
        if (m.getControllerNumber() == 120) panic();
        else if (m.getControllerNumber() == 123)
            for (int t = 0; t < numBass; ++t) { heldCount[t] = 0; bass[t].release(); }
        return;
    }
    if (ch == 10)
    {
        if (! m.isNoteOn()) return;
        int d = -1;
        switch (m.getNoteNumber())
        {
            case 35: case 36: d = BD; break;
            case 38: case 40: d = SD; break;
            case 41: case 43: d = LT; break;
            case 45: case 47: d = MT; break;
            case 48: case 50: d = HT; break;
            case 37: d = RS; break;
            case 39: d = CP; break;
            case 56: d = CB; break;
            case 49: case 57: d = CY; break;
            case 46: d = OH; break;
            case 42: case 44: d = CH; break;
            default: break;
        }
        if (d >= 0)
        {
            const int v = m.getVelocity();
            if (running && pr.b (WRITE)) writeDrum (d, v > 100, pr);
            drums.trigger (d, .72f * (float) v / 127.f + (v > 100 ? .55f * pr[AC] : 0.f));
            hits.fetch_or (1u << d);
        }
        return;
    }
    const int t = ch == 2 ? 1 : 0;      // canale 2: BASS 2, tutti gli altri (tranne il 10): BASS 1
    if (m.isNoteOn()) bassNoteOn (t, m.getNoteNumber(), m.getVelocity(), pr);
    else if (m.isNoteOff()) bassNoteOff (t, m.getNoteNumber());
}

void Engine::process (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, const ParamReader& pr,
                      bool hostPlaying, double hostBpm, double hostPpq) noexcept
{
    const int n = buffer.getNumSamples();
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    // impostazioni delle voci
    for (int t = 0; t < numBass; ++t)
    {
        const int o = t == 0 ? 0 : (int) (B2WAVE - B1WAVE);
        BassSettings b;
        b.wave = pr.i ((acido::P) (B1WAVE + o));
        b.tune = pr[(acido::P) (B1TUNE + o)];
        b.cutoff = pr[(acido::P) (B1CUT + o)];
        b.reso = pr[(acido::P) (B1RES + o)];
        b.envMod = pr[(acido::P) (B1ENV + o)];
        b.decay = pr[(acido::P) (B1DEC + o)];
        b.accent = pr[(acido::P) (B1ACC + o)];
        b.drive = pr[(acido::P) (B1DRIVE + o)];
        bass[t].set (b);
    }
    DrumSettings ds;
    ds.bdTone = pr[BDTONE]; ds.bdDecay = pr[BDDEC]; ds.sdTone = pr[SDTONE]; ds.sdSnappy = pr[SDSNAP];
    ds.tomTune[0] = pr[LTTUNE]; ds.tomTune[1] = pr[MTTUNE]; ds.tomTune[2] = pr[HTTUNE];
    ds.cyTone = pr[CYTONE]; ds.cyDecay = pr[CYDEC]; ds.ohDecay = pr[OHDEC];
    drums.set (ds);

    // livelli
    std::array<float, numDrums> dl {}, dr {};
    {
        static const float pan[numDrums] = { 0.f, 0.f, -.35f, 0.f, .35f, .1f, -.05f, .2f, -.25f, .3f, .22f };
        const float lev[numDrums] = { pr[BDLEV], pr[SDLEV], pr[LTLEV], pr[MTLEV], pr[HTLEV], pr[RSLEV], pr[CPLEV], pr[CBLEV], pr[CYLEV], pr[OHLEV], pr[CHLEV] };
        for (int d = 0; d < numDrums; ++d)
        {
            const float a = (pan[d] + 1.f) * juce::MathConstants<float>::pi * .25f;
            dl[(size_t) d] = lev[d] * lev[d] * std::cos (a) * 1.414f;
            dr[(size_t) d] = lev[d] * lev[d] * std::sin (a) * 1.414f;
        }
    }
    std::array<float, 8> target {};
    for (int t = 0; t < numBass; ++t)
    {
        const float lev = pr[t == 0 ? B1LEVEL : B2LEVEL], pan = pr[t == 0 ? B1PAN : B2PAN];
        const float a = (pan + 1.f) * juce::MathConstants<float>::pi * .25f;
        target[(size_t) (2 * t)] = lev * lev * std::cos (a) * 1.414f;
        target[(size_t) (2 * t + 1)] = lev * lev * std::sin (a) * 1.414f;
    }
    target[4] = pr[DRLEVEL] * pr[DRLEVEL];
    target[5] = juce::Decibels::decibelsToGain (pr[MASTER], -35.9f);
    target[6] = pr[DLRET];
    const float send[3] = { pr[B1SEND], pr[B2SEND], pr[DRSEND] };
    const float fb = pr[DLFB];
    const float toneA = onePoleA (sr, 1400.0 * std::exp2 (pr[DLTONE] * 3.4));

    // trasporto
    const double bpm = juce::jlimit (20.0, 400.0, hostPlaying && hostBpm > 0 ? hostBpm : (double) pr[BPM]);
    const double inc = bpm / 60.0 * 4.0 / sr;
    const double oddStart = 2.0 * juce::jlimit (50.0, 75.0, (double) pr[SWING]) / 100.0;
    curOddStart = oddStart;
    static const double dlBeats[6] = { .25, .5, .75, 1.0, 1.5, 2.0 };
    const float dlTime = (float) (dlBeats[juce::jlimit (0, 5, pr.i (DLTIME))] * 60.0 / bpm * sr);

    if (hostPlaying)
    {
        const double hp = hostPpq * 4.0;
        if (! hostRun || ! running || std::abs (hp - pos) > .25)
        {
            pos = hp;
            const long long idx = stepIndexAt (pos, oddStart);
            lastStep = (pos - stepStart (idx, oddStart) < .05) ? idx - 1 : idx;
            if (! hostRun) needPattern = true;
            running = true;
        }
    }
    else
    {
        if (hostRun) stopSequencer();
        if (pr.b (RUN) && ! running) { pos = 0; lastStep = -1; needPattern = true; running = true; }
        else if (! pr.b (RUN) && running) stopSequencer();
    }
    hostRun = hostPlaying;

    // colpi dal pannello: suonano subito e, con WRITE in riproduzione, finiscono nel pattern
    if (const uint32_t taps = tapRequests.exchange (0u))
        for (int d = 0; d < numDrums; ++d)
            if (taps & (1u << d))
            {
                if (running && pr.b (WRITE)) writeDrum (d, false, pr);
                drums.trigger (d, .72f);
                hits.fetch_or (1u << d);
            }

    auto midiIt = midi.cbegin();
    const auto midiEnd = midi.cend();
    float pk = 0;
    std::array<float, numDrums> dOut {};
    for (int i = 0; i < n; ++i)
    {
        for (; midiIt != midiEnd && (*midiIt).samplePosition <= i; ++midiIt)
            handleMidi ((*midiIt).getMessage(), pr);

        if (running)
        {
            const long long idx = stepIndexAt (pos, oddStart);
            if (idx != lastStep) { lastStep = idx; onStep (idx, oddStart, pr); }
            for (int t = 0; t < numBass; ++t)
                if (pos >= offAt[t]) { bass[t].release(); offAt[t] = 1e18; }
            pos += inc;
        }

        for (size_t k = 0; k < 7; ++k) gainSm[k] += (target[k] - gainSm[k]) * .002f;

        const float b0 = bass[0].process(), b1 = bass[1].process();
        drums.process (dOut);
        float drL = 0, drR = 0, drMono = 0;
        for (size_t d = 0; d < (size_t) numDrums; ++d) { drL += dOut[d] * dl[d]; drR += dOut[d] * dr[d]; drMono += dOut[d]; }
        drL *= gainSm[4]; drR *= gainSm[4]; drMono *= gainSm[4];

        float l = b0 * gainSm[0] + b1 * gainSm[2] + drL;
        float r = b0 * gainSm[1] + b1 * gainSm[3] + drR;
        const float sendIn = b0 * send[0] + b1 * send[1] + drMono * .5f * send[2];
        float eL, eR;
        delay.process (sendIn, sendIn, eL, eR, dlTime, fb, toneA);
        l = (l + eL * gainSm[6]) * gainSm[5];
        r = (r + eR * gainSm[6]) * gainSm[5];

        auto soft = [] (float x)
        {
            const float a = std::abs (x);
            if (a <= .8f) return x;
            return std::copysign (.8f + .2f * std::tanh ((a - .8f) / .2f), x);
        };
        l = soft (l); r = soft (r);
        if (R != nullptr) { L[i] = l; R[i] = r; }
        else L[i] = .5f * (l + r);
        pk = juce::jmax (pk, std::abs (l), std::abs (r));
    }
    for (; midiIt != midiEnd; ++midiIt) handleMidi ((*midiIt).getMessage(), pr);

    peak = juce::jmax (pk, peak * std::pow (.5f, (float) n / (float) (sr * .15)));
    outLevel.store (peak);
    playPatternUi.store (playPattern);
    runningUi.store (running);
    hostClockUi.store (hostPlaying);
    bpmUi.store (bpm);
    if (! running) for (auto& s : stepNow) s.store (-1);
}
}
