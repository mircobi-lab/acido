#pragma once
#include "PluginProcessor.h"
#include "MBStudioLookAndFeel.h"
#include <juce_audio_utils/juce_audio_utils.h>

// Griglia del sequencer: note e opzioni dei bassi, oppure le 11 righe della batteria + ACCENT.
class SeqGrid : public juce::Component
{
public:
    explicit SeqGrid (AcidoProcessor&);
    void setTrack (int t) { track = t; repaint(); }
    int getTrack() const noexcept { return track; }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragRow = -1; }
    std::function<void (const juce::String&)> onEdit;

    static constexpr int labelW = 96, headerH = 24;

private:
    AcidoProcessor& proc;
    int track = 0;
    int dragRow = -1, lastDragStep = -1;
    bool dragValue = false;

    int rows() const noexcept { return track < 2 ? 17 : acido::numDrums + 1; }
    juce::Rectangle<float> cellArea (int row, int step) const;
    bool hit (juce::Point<int>, int& row, int& step) const;
    void apply (int row, int step, bool fromDrag);
    juce::String rowName (int row) const;
};

// Pannello frontale di acido (dimensione base 1440 x 900, scalabile).
class AcidoPanel : public juce::Component, private juce::Timer
{
public:
    explicit AcidoPanel (AcidoProcessor&);
    ~AcidoPanel() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void selectTrack (int t);

    static constexpr int baseW = 1440, baseH = 900;

private:
    enum class Kind { knob, combo, toggle };
    struct Ctl
    {
        Kind kind = Kind::knob;
        acido::P param {};
        juce::String section, label;
        juce::Colour led;
        int knobSize = 54;
        juce::Slider slider;
        juce::ComboBox combo;
        juce::TextButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sa;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> ca;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> ba;
        juce::Rectangle<int> cell;
        juce::Component& comp() { return kind == Kind::knob ? (juce::Component&) slider : kind == Kind::combo ? (juce::Component&) combo : (juce::Component&) button; }
    };

    AcidoProcessor& proc;
    std::vector<std::unique_ptr<Ctl>> ctls;
    std::array<std::vector<Ctl*>, 2> bassRow1, bassRow2;
    std::array<std::vector<Ctl*>, acido::numDrums + 1> drumCols;   // 0 = ACCENT, poi gli strumenti
    std::vector<Ctl*> busRow, delayRow;
    Ctl* tempo = nullptr;
    Ctl* swing = nullptr;
    Ctl* master = nullptr;
    Ctl* run = nullptr;

    juce::TextButton patBtn[acido::numPatterns];
    juce::TextButton trackBtn[3], clearBtn { "CLEAR" }, copyBtn { "COPY" }, pasteBtn { "PASTE" }, factoryBtn { "FACTORY" }, panicBtn { "PANIC" };
    SeqGrid grid;
    mbstudio::ui::LedMeter meter { 12 };
    mbstudio::midi::MidiControls midiButtons;
    mbstudio::midi::MidiLearnOverlay midiOverlay;
    juce::Rectangle<int> displayArea, bassArea[2], seqArea, drumArea, busArea, delayArea, masterArea;
    juce::String touched, touchedValue;
    int panicFlash = 0, clipboard = -1;
    uint32_t lastVersion = 0, drumFlash[acido::numDrums] {};
    int lastSteps[3] { -2, -2, -2 }, lastPattern = -1;

    Ctl* makeCtl (acido::P, const juce::String& section, juce::Colour led, int knobSize = 54, const juce::String& label = {});
    void placeCtl (Ctl&, juce::Rectangle<int> cell);
    void showTouched (const juce::String& what, const juce::String& value);
    void timerCallback() override;
};

class AcidoEditor : public juce::AudioProcessorEditor
{
public:
    explicit AcidoEditor (AcidoProcessor&);
    ~AcidoEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void selectTrack (int t) { panel.selectTrack (t); }
private:
    mbstudio::ui::HardwareLookAndFeel lnf;
    AcidoProcessor& proc;
    AcidoPanel panel;
};
