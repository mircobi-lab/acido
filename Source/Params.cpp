#include "Params.h"

namespace acido
{
#define ACIDO_DEF(e, key, name, label, type, mn, mx, df, ctr, ch, unit) \
    ParamDef { key, name, label, PType::type, (float) (mn), (float) (mx), (float) (df), (float) (ctr), ch, unit },

const std::vector<ParamDef>& paramDefs()
{
    static const std::vector<ParamDef> defs { ACIDO_PARAMS (ACIDO_DEF) };
    return defs;
}
#undef ACIDO_DEF

juce::String formatValue (const ParamDef& d, float v)
{
    const juce::String u (d.unit);
    switch (d.type)
    {
        case PType::C: { auto c = d.choiceList(); return c[juce::jlimit (0, c.size() - 1, (int) std::lround (v))]; }
        case PType::T: return v > .5f ? "ON" : "OFF";
        case PType::I:
        {
            const int n = (int) std::lround (v);
            if (u == "st") return (n > 0 ? "+" : "") + juce::String (n) + " ST";
            if (u == "steps") return juce::String (n) + " STEPS";
            return juce::String (n);
        }
        case PType::R: break;
    }
    if (u == "dB")  return v <= -35.9f ? "-INF" : juce::String (v, 1) + " DB";
    if (u == "st")  return (v > 0 ? "+" : "") + juce::String (v, 2) + " ST";
    if (u == "bpm") return juce::String (v, 1) + " BPM";
    if (u == "sw")  return juce::String (v, 1) + "%";
    if (u == "pan")
    {
        const int p = (int) std::lround (v * 100.f);
        return p == 0 ? juce::String ("CENTRE") : (p < 0 ? "L " + juce::String (-p) : "R " + juce::String (p));
    }
    if (u == "%")   return juce::String ((int) std::lround ((v - d.min) / (d.max - d.min) * 100.f)) + "%";
    return juce::String (v, 2);
}

static std::unique_ptr<juce::RangedAudioParameter> makeParam (const ParamDef& d)
{
    const juce::ParameterID pid { d.key, 1 };
    switch (d.type)
    {
        case PType::C: return std::make_unique<juce::AudioParameterChoice> (pid, d.name, d.choiceList(), (int) d.def);
        case PType::T: return std::make_unique<juce::AudioParameterChoice> (pid, d.name, juce::StringArray { "OFF", "ON" }, d.def > .5f ? 1 : 0);
        case PType::I:
            return std::make_unique<juce::AudioParameterInt> (pid, d.name, (int) d.min, (int) d.max, (int) d.def,
                       juce::AudioParameterIntAttributes().withStringFromValueFunction ([d] (int v, int) { return formatValue (d, (float) v); }));
        case PType::R: break;
    }
    juce::NormalisableRange<float> r (d.min, d.max);
    if (d.centre > 0.f && d.centre > d.min && d.centre < d.max) r.setSkewForCentre (d.centre);
    return std::make_unique<juce::AudioParameterFloat> (pid, d.name, r, d.def,
               juce::AudioParameterFloatAttributes().withStringFromValueFunction ([d] (float v, int) { return formatValue (d, v); }));
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (auto& d : paramDefs()) layout.add (makeParam (d));
    return layout;
}

void ParamReader::attach (juce::AudioProcessorValueTreeState& s)
{
    const auto& defs = paramDefs();
    for (size_t i = 0; i < defs.size(); ++i)
    {
        ptr[i] = s.getRawParameterValue (defs[i].key);
        jassert (ptr[i] != nullptr);
    }
}
}
