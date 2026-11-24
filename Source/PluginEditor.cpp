#include "PluginEditor.h"

namespace ui = mbstudio::ui;
namespace C = mbstudio::ui::colours;
using namespace acido;

namespace
{
constexpr int margin = 14;
const char* noteNames[13] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "C'" };

juce::Colour trackLed (int t) { return t == 0 ? C::ledBlue : (t == 1 ? C::ledGreen : C::ledAmber); }

juce::String ledHex (juce::Colour c) { return c.toString(); }
}

// ================================================================================================ griglia
SeqGrid::SeqGrid (AcidoProcessor& p) : proc (p) {}

juce::String SeqGrid::rowName (int row) const
{
    if (track < 2)
    {
        if (row < 13) return noteNames[12 - row];
        static const char* f[] = { "OCT UP", "OCT DOWN", "ACCENT", "SLIDE" };
        return f[row - 13];
    }
    return drumName (row);
}

juce::Rectangle<float> SeqGrid::cellArea (int row, int step) const
{
    const float gw = (float) (getWidth() - labelW - 8);
    const float group = 6.f;                               // spazio fra i gruppi da 4 passi
    const float cw = (gw - 3 * group) / 16.f;
    const float rh = (float) (getHeight() - headerH - 6) / (float) rows();
    float y = (float) headerH + (float) row * rh;
    if (track < 2 && row >= 13) y += 0;                    // le righe delle opzioni seguono le note
    return { (float) labelW + (float) step * cw + (float) (step / 4) * group, y, cw, rh };
}

bool SeqGrid::hit (juce::Point<int> p, int& row, int& step) const
{
    for (int r = 0; r < rows(); ++r)
        for (int s = 0; s < numSteps; ++s)
            if (cellArea (r, s).contains (p.toFloat())) { row = r; step = s; return true; }
    return false;
}

void SeqGrid::apply (int row, int step, bool fromDrag)
{
    const int pat = juce::jlimit (0, numPatterns - 1, (int) std::lround (proc.paramValue (PATTERN)) - 1);
    juce::String msg;
    if (track < 2)
    {
        auto st = proc.patterns.bass (pat, track, step);
        if (row < 13)
        {
            const int note = 12 - row;
            if (fromDrag) { st.note = note; st.gate = true; }
            else if (st.gate && st.note == note) st.gate = false;
            else { st.note = note; st.gate = true; }
            msg = st.gate ? juce::String (noteNames[note]) + (st.octave > 0 ? " UP" : st.octave < 0 ? " DOWN" : "") : "REST";
        }
        else
        {
            const bool v = fromDrag ? dragValue : true;
            switch (row)
            {
                case 13: st.octave = fromDrag ? (v ? 1 : 0) : (st.octave == 1 ? 0 : 1); msg = st.octave == 1 ? "OCT UP" : "OCT 0"; break;
                case 14: st.octave = fromDrag ? (v ? -1 : 0) : (st.octave == -1 ? 0 : -1); msg = st.octave == -1 ? "OCT DOWN" : "OCT 0"; break;
                case 15: st.accent = fromDrag ? v : ! st.accent; msg = st.accent ? "ACCENT ON" : "ACCENT OFF"; break;
                case 16: st.slide = fromDrag ? v : ! st.slide; msg = st.slide ? "SLIDE ON" : "SLIDE OFF"; break;
                default: break;
            }
        }
        if (! fromDrag && row >= 13)
            dragValue = row == 13 ? st.octave == 1 : row == 14 ? st.octave == -1 : row == 15 ? st.accent : st.slide;
        proc.patterns.setBass (pat, track, step, st);
    }
    else
    {
        const bool v = fromDrag ? dragValue : ! proc.patterns.drum (pat, row, step);
        if (! fromDrag) dragValue = v;
        proc.patterns.setDrum (pat, row, step, v);
        msg = juce::String (drumName (row)) + (v ? " ON" : " OFF");
    }
    if (onEdit) onEdit ("STEP " + juce::String (step + 1) + "  " + msg);
    repaint();
}

void SeqGrid::mouseDown (const juce::MouseEvent& e)
{
    int r, s;
    // batteria: clic sul nome dello strumento = colpo (con WRITE e sequencer in moto si scrive nel pattern)
    if (track == 2 && e.x < labelW)
    {
        for (int row = 0; row < numDrums; ++row)
        {
            const auto c = cellArea (row, 0);
            if (e.y >= (int) c.getY() && e.y < (int) c.getBottom())
            {
                proc.engine.tapDrum (row);
                if (onEdit) onEdit (juce::String ("TAP ") + drumShort (row));
                return;
            }
        }
        return;
    }
    if (! hit (e.getPosition(), r, s)) return;
    dragRow = r; lastDragStep = s;
    apply (r, s, false);
}

void SeqGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (dragRow < 0) return;
    int r, s;
    if (! hit (e.getPosition(), r, s) || s == lastDragStep) return;
    lastDragStep = s;
    apply (dragRow, s, true);     // trascinando si dipinge la stessa riga
}

void SeqGrid::paint (juce::Graphics& g)
{
    const auto all = getLocalBounds().toFloat();
    ui::paintRecess (g, all);
    const int pat = juce::jlimit (0, numPatterns - 1, (int) std::lround (proc.paramValue (PATTERN)) - 1);
    const P lenP = track == 0 ? B1LEN : track == 1 ? B2LEN : DRLEN;
    const int len = juce::jlimit (1, 16, (int) std::lround (proc.paramValue (lenP)));
    const int now = proc.engine.isRunning() && proc.engine.playingPattern() == pat ? proc.engine.currentStep (track) : -1;
    const auto led = trackLed (track);

    // intestazione: numero del passo e LED del passo in corso
    for (int s = 0; s < numSteps; ++s)
    {
        const auto c = cellArea (0, s);
        const auto h = juce::Rectangle<float> (c.getX(), 4.f, c.getWidth(), (float) headerH - 6.f);
        g.setColour (s < len ? C::print : C::printDim.withAlpha (.4f));
        g.setFont (ui::Fonts::display (14.f));
        g.drawText (juce::String (s + 1), h.withTrimmedRight (h.getWidth() * .45f), juce::Justification::centredRight, false);
        ui::paintLed (g, { h.getRight() - h.getWidth() * .3f, h.getCentreY() }, 3.f, C::ledRed, s == now ? 1.f : 0.f);
    }

    std::array<BassStep, numSteps> bs {};
    if (track < 2) for (int s = 0; s < numSteps; ++s) bs[(size_t) s] = proc.patterns.bass (pat, track, s);

    for (int r = 0; r < rows(); ++r)
    {
        const auto c0 = cellArea (r, 0);
        const bool flagRow = track < 2 && r >= 13;
        const bool sharp = track < 2 && r < 13 && juce::String (noteNames[12 - r]).containsChar ('#');
        const bool accRow = (track < 2 && r == 15) || (track == 2 && r == accentRow);
        // etichetta
        auto lab = juce::Rectangle<float> (8.f, c0.getY(), (float) labelW - 14.f, c0.getHeight());
        if (sharp) { g.setColour (juce::Colour (0xff0b0b0c)); g.fillRect (lab.withX (4.f).withWidth ((float) labelW - 8.f).reduced (0.f, 1.f)); }
        g.setColour (flagRow || accRow ? C::printDim : C::print);
        g.setFont (track < 2 && r < 13 ? ui::Fonts::display (juce::jmin (16.f, c0.getHeight())) : ui::Fonts::label (juce::jmin (12.f, c0.getHeight() - 2.f)));
        g.drawText (rowName (r), lab, juce::Justification::centredLeft, false);
        if (track < 2 && r == 13) ui::paintEngravedLine (g, { 6.f, c0.getY() - 1.f }, { all.getRight() - 6.f, c0.getY() - 1.f });
        if (track == 2 && r == accentRow) ui::paintEngravedLine (g, { 6.f, c0.getY() - 1.f }, { all.getRight() - 6.f, c0.getY() - 1.f });

        for (int s = 0; s < numSteps; ++s)
        {
            const auto c = cellArea (r, s).reduced (2.f, 1.5f);
            bool on = false; float dim = 1.f;
            if (track < 2)
            {
                const auto& st = bs[(size_t) s];
                if (r < 13) on = st.gate && st.note == 12 - r;
                else
                {
                    on = r == 13 ? st.octave == 1 : r == 14 ? st.octave == -1 : r == 15 ? st.accent : st.slide;
                    if (! st.gate) dim = .45f;
                }
            }
            else on = proc.patterns.drum (pat, r, s);
            if (s >= len) dim *= .35f;

            // tasto in gomma
            g.setColour ((sharp ? juce::Colour (0xff0d0e0f) : C::key).withMultipliedAlpha (dim));
            g.fillRoundedRectangle (c, 2.f);
            g.setColour (C::keyEdge.withMultipliedAlpha (.8f * dim));
            g.drawRoundedRectangle (c, 2.f, 1.f);
            if (on)
            {
                const auto col = accRow ? C::ledAmber : (flagRow ? C::print : led);
                g.setColour (col.withAlpha (.22f * dim));
                g.fillRoundedRectangle (c.expanded (1.5f), 3.f);
                g.setColour (col.withMultipliedAlpha (dim));
                g.fillRoundedRectangle (c.reduced (c.getWidth() * .12f, c.getHeight() * .22f), 1.5f);
            }
            if (s == now)
            {
                g.setColour (C::print.withAlpha (.10f));
                g.fillRect (c);
            }
        }
    }
    // linea della nota per i bassi: rende leggibile la melodia
    if (track < 2)
    {
        juce::Path p;
        bool started = false;
        for (int s = 0; s < len; ++s)
        {
            const auto& st = bs[(size_t) s];
            if (! st.gate) { started = false; continue; }
            const auto c = cellArea (12 - st.note, s);
            if (! started) { p.startNewSubPath (c.getCentre()); started = true; }
            else p.lineTo (c.getCentre());
            if (! st.slide) started = false;
        }
        g.setColour (led.withAlpha (.55f));
        g.strokePath (p, juce::PathStrokeType (2.f));
    }
}

// ================================================================================================ pannello
AcidoPanel::AcidoPanel (AcidoProcessor& p)
    : proc (p), grid (p), midiButtons (p.midiMap), midiOverlay (p.midiMap)
{
    // trasporto
    run = makeCtl (RUN, "TRANSPORT", C::ledGreen);
    tempo = makeCtl (BPM, "TRANSPORT", C::print, 50);
    swing = makeCtl (SWING, "TRANSPORT", C::print, 50);

    // bassi
    for (int t = 0; t < 2; ++t)
    {
        const int o = t == 0 ? 0 : (int) (B2WAVE - B1WAVE);
        const juce::String sec = t == 0 ? "BASS 1" : "BASS 2";
        const auto led = trackLed (t);
        for (auto k : { B1WAVE, B1TUNE, B1CUT, B1RES, B1ENV, B1DEC, B1ACC, B1DRIVE })
            bassRow1[(size_t) t].push_back (makeCtl ((P) (k + o), sec, led));
        for (auto k : { B1LEVEL, B1PAN, B1SEND, B1TRANS, B1LEN, B1MUTE })
            bassRow2[(size_t) t].push_back (makeCtl ((P) (k + o), sec, led, 46));
    }

    // batteria
    drumCols[0] = { makeCtl (AC, "RHYTHM", C::ledAmber, 46) };
    const std::vector<std::vector<P>> drumParams =
        { { BDLEV, BDTONE, BDDEC }, { SDLEV, SDTONE, SDSNAP }, { LTLEV, LTTUNE }, { MTLEV, MTTUNE }, { HTLEV, HTTUNE },
          { RSLEV }, { CPLEV }, { CBLEV }, { CYLEV, CYTONE, CYDEC }, { OHLEV, OHDEC }, { CHLEV } };
    for (int d = 0; d < numDrums; ++d)
        for (auto k : drumParams[(size_t) d])
            drumCols[(size_t) d + 1].push_back (makeCtl (k, drumName (d), C::ledAmber, 46));
    for (auto k : { DRLEVEL, DRSEND, DRLEN, DRMUTE }) busRow.push_back (makeCtl (k, "DRUM BUS", C::ledAmber, 46));
    for (auto k : { DLTIME, DLFB, DLTONE, DLRET }) delayRow.push_back (makeCtl (k, "DELAY", C::print, 46));
    master = makeCtl (MASTER, "OUTPUT", C::print, 64);

    // pattern
    for (int i = 0; i < numPatterns; ++i)
    {
        patBtn[i].setButtonText (juce::String (i + 1));
        patBtn[i].getProperties().set ("led", true);
        patBtn[i].getProperties().set ("ledColour", ledHex (C::ledBlue));
        patBtn[i].setTooltip ("Pattern " + juce::String (i + 1) + ": lo modifichi subito, in riproduzione cambia a fine battuta");
        patBtn[i].onClick = [this, i] { proc.setParamValue (PATTERN, (float) (i + 1)); grid.repaint(); };
        addAndMakeVisible (patBtn[i]);
    }
    static const char* trackNames[] = { "BASS 1", "BASS 2", "DRUMS" };
    for (int t = 0; t < 3; ++t)
    {
        trackBtn[t].setButtonText (trackNames[t]);
        trackBtn[t].getProperties().set ("led", true);
        trackBtn[t].getProperties().set ("ledColour", ledHex (trackLed (t)));
        trackBtn[t].onClick = [this, t] { selectTrack (t); };
        addAndMakeVisible (trackBtn[t]);
    }
    auto editPattern = [this] { return juce::jlimit (0, numPatterns - 1, (int) std::lround (proc.paramValue (PATTERN)) - 1); };
    clearBtn.setTooltip ("Svuota la traccia scelta nel pattern in modifica");
    clearBtn.onClick = [this, editPattern] { proc.patterns.clearTrack (editPattern(), grid.getTrack()); showTouched ("CLEAR", trackNames[grid.getTrack()]); grid.repaint(); };
    copyBtn.setTooltip ("Copia il pattern in modifica");
    copyBtn.onClick = [this, editPattern] { clipboard = editPattern(); showTouched ("COPY", "PATTERN " + juce::String (clipboard + 1)); };
    pasteBtn.setTooltip ("Incolla il pattern copiato su quello in modifica");
    pasteBtn.onClick = [this, editPattern]
    {
        if (clipboard < 0) { showTouched ("PASTE", "PRIMA COPIA UN PATTERN"); return; }
        proc.patterns.copyPattern (clipboard, editPattern());
        showTouched ("PASTE", "PATTERN " + juce::String (clipboard + 1) + " > " + juce::String (editPattern() + 1));
        grid.repaint();
    };
    factoryBtn.setTooltip ("Ricarica gli 8 pattern di fabbrica (sostituisce quelli attuali)");
    factoryBtn.onClick = [this]
    {
        juce::Component::SafePointer<AcidoPanel> safe (this);
        auto opts = juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::WarningIcon).withTitle ("FACTORY")
                        .withMessage ("Ricaricare gli 8 pattern di fabbrica? Quelli attuali vengono sostituiti.")
                        .withButton ("RICARICA", 1).withButton ("ANNULLA", 0).withAssociatedComponent (this);
        juce::AlertWindow::showAsync (opts, [safe] (int r)
        {
            if (safe == nullptr || r != 1) return;
            safe->proc.patterns.loadFactory();
            safe->showTouched ("FACTORY", "PATTERN RICARICATI");
            safe->grid.repaint();
        });
    };
    for (auto* b : { &clearBtn, &copyBtn, &pasteBtn, &factoryBtn }) addAndMakeVisible (*b);

    // generatore casuale
    // scrittura in tempo reale e catena dei pattern
    write = makeCtl (WRITE, "TRANSPORT", C::ledRed);
    write->button.getProperties().set ("ledColour", ledHex (C::ledRed));
    write->button.setTooltip ("WRITE: con il sequencer in moto, le note dei bassi (canale 1 e 2) e i colpi di batteria (canale 10, o clic sul nome dello strumento) si scrivono sul passo più vicino");
    chain = makeCtl (CHAIN, "PATTERN", C::ledAmber);
    chain->button.setTooltip ("CHAIN: suona la catena di pattern invece del pattern scelto, una battuta per voce");
    chainAddBtn.setTooltip ("Aggiunge il pattern in modifica in fondo alla catena (fino a 16)");
    chainAddBtn.onClick = [this, editPattern]
    {
        if (proc.patterns.chainAppend (editPattern())) showTouched ("CHAIN", chainText (-1));
        else showTouched ("CHAIN", "PIENA: 16 PATTERN");
    };
    chainDelBtn.setTooltip ("Toglie l'ultimo pattern della catena");
    chainDelBtn.onClick = [this] { proc.patterns.chainRemoveLast(); showTouched ("CHAIN", chainText (-1)); };
    chainClrBtn.setTooltip ("Svuota la catena");
    chainClrBtn.onClick = [this] { proc.patterns.chainClear(); showTouched ("CHAIN", "VUOTA"); };
    for (auto* b : { &chainAddBtn, &chainDelBtn, &chainClrBtn }) addAndMakeVisible (*b);

    style = makeCtl (GENSTYLE, "RANDOM", C::ledAmber);
    soundLock = makeCtl (GENSOUND, "RANDOM", C::ledGreen);
    soundLock->button.getProperties().set ("ledColour", ledHex (C::ledGreen));
    soundLock->button.setTooltip ("SND acceso: RANDOM regola anche i suoni (filtri, inviluppi, drive, batteria, delay) secondo lo stile. Spento: i suoni restano quelli tuoi");
    randomBtn.setTooltip ("Genera un pattern nuovo (bassi e batteria) nello stile scelto, con una scala adatta; imposta anche tempo e swing");
    randomBtn.getProperties().set ("led", true);
    randomBtn.getProperties().set ("ledColour", ledHex (C::ledAmber));
    randomBtn.onClick = [this] { randomize (true); };
    rndTrackBtn.setTooltip ("Rigenera solo la traccia scelta, restando nella tonalita' del pattern");
    rndTrackBtn.onClick = [this] { randomize (false); };
    addAndMakeVisible (randomBtn);
    addAndMakeVisible (rndTrackBtn);

    panicBtn.setTooltip ("MIDI PANIC: spegne subito tutte le note, ferma il sequencer interno e svuota il delay");
    panicBtn.getProperties().set ("led", true);
    panicBtn.getProperties().set ("ledColour", ledHex (C::ledRed));
    panicBtn.onClick = [this]
    {
        proc.requestPanic();
        if (proc.paramValue (RUN) > .5f) proc.setParamValue (RUN, 0.f);
        panicFlash = 15;
        showTouched ("PANIC", "TUTTE LE NOTE SPENTE");
    };
    addAndMakeVisible (panicBtn);

    grid.onEdit = [this] (const juce::String& s) { showTouched (juce::String ("EDIT ") + trackNames[grid.getTrack()], s); };
    addAndMakeVisible (grid);
    addAndMakeVisible (meter);
    addAndMakeVisible (midiButtons);
    addAndMakeVisible (midiOverlay);     // per ultimo: sopra a tutto

    selectTrack (0);
    touched.clear();
    setSize (baseW, baseH);
    startTimerHz (30);
}

AcidoPanel::~AcidoPanel()
{
    stopTimer();
    for (auto& c : ctls) { c->sa.reset(); c->ca.reset(); c->ba.reset(); }
}

AcidoPanel::Ctl* AcidoPanel::makeCtl (P param, const juce::String& section, juce::Colour led, int knobSize, const juce::String& label)
{
    auto c = std::make_unique<Ctl>();
    const auto& d = paramDefs()[(size_t) param];
    c->param = param;
    c->section = section;
    c->label = label.isNotEmpty() ? label : juce::String (d.label);
    c->led = led;
    c->knobSize = knobSize;
    auto* cp = c.get();
    auto show = [this, cp] (float v) { showTouched (cp->section + "  " + cp->label, formatValue (paramDefs()[(size_t) cp->param], v)); };
    switch (d.type)
    {
        case PType::C:
            c->kind = Kind::combo;
            c->combo.addItemList (d.choiceList(), 1);
            c->combo.setTooltip (d.name);
            c->combo.onChange = [cp, show] { show ((float) (cp->combo.getSelectedId() - 1)); };
            c->ca = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.state, d.key, c->combo);
            break;
        case PType::T:
            c->kind = Kind::toggle;
            c->button.setButtonText (c->label);
            c->button.setClickingTogglesState (true);
            c->button.setTooltip (d.name);
            c->button.getProperties().set ("ledColour", ledHex (param == RUN ? C::ledGreen : C::ledRed));
            c->button.onClick = [cp, show] { show (cp->button.getToggleState() ? 1.f : 0.f); };
            c->ba = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.state, d.key, c->button);
            break;
        case PType::R:
        case PType::I:
            c->kind = Kind::knob;
            c->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            c->slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
            c->slider.setColour (juce::Slider::trackColourId, led);
            c->slider.setMouseDragSensitivity (200);
            c->slider.setDoubleClickReturnValue (true, d.def);
            c->slider.setTooltip (d.name);
            if (d.min < 0 && d.max > 0) c->slider.getProperties().set ("bipolar", true);
            c->slider.onValueChange = [cp, show] { show ((float) cp->slider.getValue()); };
            c->sa = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.state, d.key, c->slider);
            break;
    }
    mbstudio::midi::tag (c->comp(), d.key);
    addAndMakeVisible (c->comp());
    ctls.push_back (std::move (c));
    return cp;
}

void AcidoPanel::showTouched (const juce::String& what, const juce::String& value)
{
    touched = what; touchedValue = value;
    repaint (displayArea);
}

void AcidoPanel::randomize (bool wholePattern)
{
    static const char* trackNames[] = { "BASS 1", "BASS 2", "DRUMS" };
    const int pat = juce::jlimit (0, numPatterns - 1, (int) std::lround (proc.paramValue (PATTERN)) - 1);
    const int st = (int) std::lround (proc.paramValue (GENSTYLE));
    const int what = wholePattern ? 3 : grid.getTrack();
    const auto res = gen::generate (proc.patterns, pat, what, st, rng);
    juce::String info = gen::keyName (res.root) + " " + gen::scaleName (res.scale);
    if (proc.paramValue (GENSOUND) > .5f)
    {
        for (const auto& [p, v] : gen::sounds (res.style, what, rng)) proc.setParamValue (p, v);
        info << "  +SND";
    }
    if (wholePattern)
    {
        proc.setParamValue (BPM, res.bpm);
        proc.setParamValue (SWING, res.swing);
        for (auto p : { B1LEN, B2LEN, DRLEN }) proc.setParamValue (p, 16.f);
        info << "  " << juce::String (res.bpm, 1) << " BPM  SW " << juce::String (res.swing, 1) << "%";
        showTouched (juce::String ("RANDOM ") + gen::styleName (res.style), info);
    }
    else showTouched (juce::String ("RANDOM ") + trackNames[grid.getTrack()], info);
    grid.repaint();
    repaint (seqArea);
}

void AcidoPanel::selectTrack (int t)
{
    for (int i = 0; i < 3; ++i) trackBtn[i].setToggleState (i == t, juce::dontSendNotification);
    grid.setTrack (t);
    repaint (seqArea);
}

void AcidoPanel::placeCtl (Ctl& c, juce::Rectangle<int> cell)
{
    c.cell = cell;
    const int cx = cell.getCentreX();
    switch (c.kind)
    {
        case Kind::knob:   c.slider.setBounds (cx - c.knobSize / 2, cell.getY() + 16, c.knobSize, c.knobSize); break;
        case Kind::combo:  c.combo.setBounds (cell.getX() + 2, cell.getY() + 24, cell.getWidth() - 4, 26); break;
        case Kind::toggle: c.button.setBounds (cell.getX() + 4, cell.getY() + 22, cell.getWidth() - 8, 28); break;
    }
}

juce::String AcidoPanel::chainText (int current) const
{
    const int n = proc.patterns.chainLength();
    if (n == 0) return "CHAIN VUOTA";
    juce::String t ("CHAIN");
    for (int i = 0; i < n; ++i)
    {
        const juce::String p (proc.patterns.chainAt (i) + 1);
        t << " " << (i == current ? "[" + p + "]" : p);
    }
    return t;
}

// ------------------------------------------------------------------------------------------------ layout
void AcidoPanel::resized()
{
    // intestazione
    displayArea = { 340, 14, 330, 72 };
    run->button.setBounds (690, 44, 74, 32);
    write->button.setBounds (690, 80, 74, 22);
    chain->button.setBounds (922, 80, 64, 22);
    chainAddBtn.setBounds (990, 80, 52, 22);
    chainDelBtn.setBounds (1046, 80, 46, 22);
    chainClrBtn.setBounds (1096, 80, 46, 22);
    placeCtl (*tempo, { 772, 22, 64, 70 });
    placeCtl (*swing, { 840, 22, 64, 70 });
    for (int i = 0; i < numPatterns; ++i) patBtn[i].setBounds (922 + i * 38, 44, 34, 32);
    panicBtn.setBounds (1236, 14, 166, 28);
    midiButtons.setBounds (1236, 50, 166, 26);
    meter.setBounds (1412, 14, 14, 72);

    // bassi
    const int bw = (baseW - 2 * margin - 24) / 2;
    for (int t = 0; t < 2; ++t)
    {
        const int x = margin + t * (bw + 24);
        bassArea[t] = { x, 106, bw, 190 };
        const int cw1 = bw / (int) bassRow1[(size_t) t].size();
        for (size_t k = 0; k < bassRow1[(size_t) t].size(); ++k)
            placeCtl (*bassRow1[(size_t) t][k], { x + (int) k * cw1, 126, cw1, 78 });
        const int cw2 = bw / 8;
        for (size_t k = 0; k < bassRow2[(size_t) t].size(); ++k)
            placeCtl (*bassRow2[(size_t) t][k], { x + (int) k * cw2 + cw2 / 2, 208, cw2, 74 });
    }

    // sequencer
    seqArea = { margin, 308, baseW - 2 * margin, 310 };
    for (int t = 0; t < 3; ++t) trackBtn[t].setBounds (margin, 328 + t * 31, 124, 28);
    style->cell = {};
    style->combo.setBounds (margin, 438, 78, 24);
    soundLock->button.setBounds (margin + 82, 438, 42, 24);
    randomBtn.setBounds (margin, 466, 124, 28);
    rndTrackBtn.setBounds (margin, 498, 60, 26);
    clearBtn.setBounds (margin + 64, 498, 60, 26);
    copyBtn.setBounds (margin, 528, 60, 26);
    pasteBtn.setBounds (margin + 64, 528, 60, 26);
    factoryBtn.setBounds (margin, 558, 124, 26);
    grid.setBounds (margin + 140, 328, baseW - 2 * margin - 140, 288);

    // batteria
    drumArea = { margin, 632, 960, 256 };
    const int dw = 80;
    for (size_t col = 0; col < drumCols.size(); ++col)
        for (size_t k = 0; k < drumCols[col].size(); ++k)
            placeCtl (*drumCols[col][k], { margin + (int) col * dw, 668 + (int) k * 72, dw, 70 });
    busArea = { 996, 632, 300, 120 };
    for (size_t k = 0; k < busRow.size(); ++k) placeCtl (*busRow[k], { 996 + (int) k * 75, 652, 75, 72 });
    delayArea = { 996, 742, 300, 120 };
    for (size_t k = 0; k < delayRow.size(); ++k) placeCtl (*delayRow[k], { 996 + (int) k * 75, 762, 75, 72 });
    masterArea = { 1316, 632, 110, 200 };
    placeCtl (*master, { 1316, 668, 110, 96 });

    midiOverlay.setBounds (getLocalBounds());
}

// ------------------------------------------------------------------------------------------------ disegno
void AcidoPanel::paint (juce::Graphics& g)
{
    ui::paintChassis (g, getLocalBounds());

    // marchio e sottotitolo
    ui::paintBrand (g, { 18.f, 16.f, 310.f, 34.f }, "ACIDO");
    ui::paintPrinted (g, { 18.f, 56.f, 320.f, 16.f }, "TWIN ACID BASS LINE  +  ANALOG DRUMS", 14.f, juce::Justification::centredLeft, C::printDim);
    ui::paintPrinted (g, { 18.f, 74.f, 320.f, 14.f }, "16 STEP SEQUENCER  -  SLIDE  -  ACCENT  -  SWING", 11.f, juce::Justification::centredLeft, C::printDim.withAlpha (.7f));

    // display
    const auto d = displayArea.toFloat();
    ui::paintRecess (g, d);
    {
        const auto in = d.reduced (12.f, 8.f);
        const int edit = (int) std::lround (proc.paramValue (PATTERN));
        const int playing = proc.engine.playingPattern() + 1;
        g.setColour (C::print);
        g.setFont (ui::Fonts::display (26.f));
        juce::String top = "PATTERN " + juce::String (edit);
        if (proc.engine.isRunning() && playing != edit) top << "  (PLAY " << playing << ")";
        g.drawText (top, in.withHeight (28.f), juce::Justification::centredLeft, true);
        g.setFont (ui::Fonts::thin (26.f));
        g.drawText (juce::String (proc.engine.currentBpm(), 1), in.withHeight (28.f).withTrimmedRight (34.f), juce::Justification::centredRight, true);
        g.setFont (ui::Fonts::label (11.f));
        g.setColour (C::printDim);
        g.drawText ("BPM", in.withHeight (28.f), juce::Justification::centredRight, true);
        const juce::String clock = proc.engine.isHostClock() ? "CLOCK HOST" : "CLOCK INT";
        juce::String idle = clock + (proc.engine.isRunning() ? "   RUNNING" : "   STOP");
        if (proc.paramValue (WRITE) > .5f) idle << "   WRITE";
        if (proc.paramValue (CHAIN) > .5f) idle << "   " << chainText (proc.engine.chainPosition());
        const juce::String line2 = touched.isNotEmpty() ? touched + "  =  " + touchedValue : idle;
        g.setFont (ui::Fonts::label (14.f));
        g.setColour (C::print);
        g.drawText (line2.toUpperCase(), in.withY (in.getY() + 34.f).withHeight (20.f), juce::Justification::centredLeft, true);
    }

    auto chipAt = [&] (float x, float y, const juce::String& t) { ui::paintChip (g, { x, y, ui::chipWidth (t, 16.f), 16.f }, t, 14.f); };
    chipAt (690.f, 16.f, "TRANSPORT");
    ui::paintLed (g, { 690.f + ui::chipWidth ("TRANSPORT", 16.f) + 10.f, 24.f }, 3.5f, C::ledGreen, proc.engine.isRunning() ? 1.f : 0.f);
    chipAt (922.f, 16.f, "PATTERN");
    ui::paintPrinted (g, { 922.f + ui::chipWidth ("PATTERN", 16.f) + 8.f, 16.f, 200.f, 16.f }, "EDIT / PLAY", 11.f, juce::Justification::centredLeft, C::printDim);
    // LED verde sopra il pattern che sta suonando
    if (proc.engine.isRunning())
    {
        const int pl = proc.engine.playingPattern();
        const auto b = patBtn[juce::jlimit (0, numPatterns - 1, pl)].getBounds().toFloat();
        ui::paintLed (g, { b.getCentreX(), b.getY() - 5.f }, 2.5f, C::ledGreen, 1.f);
    }
    ui::paintEngravedLine (g, { (float) margin, 98.f }, { (float) baseW - margin, 98.f });

    // intestazione di sezione: chip + linea incisa + nota a destra con LED
    auto header = [&] (juce::Rectangle<int> a, const juce::String& title, const juce::String& note, juce::Colour led)
    {
        const auto r = a.toFloat();
        const float cw = ui::chipWidth (title, 16.f);
        ui::paintChip (g, { r.getX(), r.getY(), cw, 16.f }, title, 14.f);
        g.setFont (ui::Fonts::label (11.f));
        const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), note) + 6.f;
        const float lineEnd = r.getRight() - tw - 14.f;
        if (lineEnd > r.getX() + cw + 6.f) ui::paintEngravedLine (g, { r.getX() + cw + 4.f, r.getY() + 8.f }, { lineEnd, r.getY() + 8.f });
        g.setColour (C::printDim);
        g.drawText (note, juce::Rectangle<float> (r.getRight() - tw, r.getY() + 1.f, tw, 14.f), juce::Justification::centredRight, false);
        ui::paintLed (g, { r.getRight() - tw - 6.f, r.getY() + 8.f }, 2.5f, led, 1.f);
    };
    auto labels = [&] (const std::vector<Ctl*>& row)
    {
        for (auto* c : row)
        {
            if (c->kind == Kind::toggle) continue;
            g.setColour (C::print);
            g.setFont (ui::Fonts::label (12.f));
            g.drawText (c->label, c->cell.toFloat().withHeight (16.f).expanded (4.f, 0.f), juce::Justification::centred, false);
        }
    };
    labels ({ tempo, swing });

    for (int t = 0; t < 2; ++t)
    {
        header (bassArea[t], t == 0 ? "BASS 1" : "BASS 2", t == 0 ? "MIDI CH 1" : "MIDI CH 2", trackLed (t));
        labels (bassRow1[(size_t) t]);
        labels (bassRow2[(size_t) t]);
    }

    header (seqArea, "SEQUENCER", "CLICK = NOTE / STEP   -   DRAG = PAINT", trackLed (grid.getTrack()));
    ui::paintPrinted (g, { (float) margin, 422.f, 124.f, 14.f }, "RANDOM STYLE", 11.f, juce::Justification::centredLeft, C::printDim);
    {
        const int pat = juce::jlimit (0, numPatterns - 1, (int) std::lround (proc.paramValue (PATTERN)) - 1);
        const auto k = proc.patterns.key (pat);
        const juce::String key = k.root >= 0 ? gen::keyName (k.root) + " " + gen::scaleName (k.scale) : juce::String ("FREE");
        ui::paintPrinted (g, { (float) margin, 590.f, 124.f, 12.f }, "KEY", 10.f, juce::Justification::centredLeft, C::printDim);
        ui::paintPrinted (g, { (float) margin + 22.f, 588.f, 104.f, 16.f }, key, 12.f, juce::Justification::centredLeft, C::print);
    }

    header (drumArea, "RHYTHM", "MIDI CH 10", C::ledAmber);
    for (size_t col = 0; col < drumCols.size(); ++col)
    {
        const auto first = drumCols[col].front()->cell.toFloat();
        const juce::String name = col == 0 ? juce::String ("ACCENT") : juce::String (drumName ((int) col - 1));
        ui::paintPrinted (g, { first.getX(), 650.f, first.getWidth(), 14.f }, name, 13.f, juce::Justification::centred, C::print);
        if (col > 0)
        {
            const bool lit = (drumFlash[col - 1] > 0);
            ui::paintLed (g, { first.getX() + 8.f, 657.f }, 2.5f, C::ledAmber, lit ? 1.f : 0.f);
        }
        if (col > 0) ui::paintEngravedLine (g, { first.getX(), 652.f }, { first.getX(), 880.f });
        for (auto* c : drumCols[col])
        {
            if (col == 0) continue;
            g.setColour (C::printDim);
            g.setFont (ui::Fonts::label (11.f));
            g.drawText (c->label, c->cell.toFloat().withY (c->cell.getY() + 4.f).withHeight (14.f), juce::Justification::centred, false);
        }
    }
    header (busArea, "DRUM BUS", "", C::ledAmber);
    labels (busRow);
    header (delayArea, "DELAY", "TEMPO SYNC", C::ledBlue);
    labels (delayRow);
    header (masterArea, "OUTPUT", "", C::ledGreen);
    labels ({ master });
    ui::paintPrinted (g, { 1316.f, 768.f, 110.f, 14.f }, formatValue (paramDefs()[MASTER], proc.paramValue (MASTER)), 12.f,
                      juce::Justification::centred, C::printDim);

    // logo impresso in basso a destra
    ui::paintLogo (g, { (float) baseW - 260.f, (float) baseH - 30.f, 246.f, 16.f }, false, ui::Imprint::debossed, juce::Justification::centredRight);
    ui::paintPrinted (g, { 996.f, (float) baseH - 30.f, 400.f, 16.f }, "DOUBLE CLICK A KNOB TO RESET", 11.f, juce::Justification::centredLeft, C::printDim);
}

void AcidoPanel::timerCallback()
{
    meter.setLevel (std::pow (proc.engine.getOutputLevel(), .5f));
    if (panicFlash > 0) --panicFlash;
    panicBtn.setToggleState (panicFlash > 0 && (panicFlash / 3) % 2 == 0, juce::dontSendNotification);

    const int edit = (int) std::lround (proc.paramValue (PATTERN)) - 1;
    for (int i = 0; i < numPatterns; ++i) patBtn[i].setToggleState (i == edit, juce::dontSendNotification);

    // LED delle voci della batteria
    const uint32_t h = proc.engine.drumHits();
    if (h) proc.engine.clearDrumHits();
    bool drumsChanged = false;
    for (int dI = 0; dI < numDrums; ++dI)
    {
        if (h & (1u << dI)) { drumFlash[dI] = 4; drumsChanged = true; }
        else if (drumFlash[dI] > 0) { if (--drumFlash[dI] == 0) drumsChanged = true; }
    }
    if (drumsChanged) repaint (drumArea);

    bool gridDirty = proc.patterns.version() != lastVersion;
    lastVersion = proc.patterns.version();
    for (int t = 0; t < 3; ++t)
    {
        const int s = proc.engine.isRunning() ? proc.engine.currentStep (t) : -1;
        if (s != lastSteps[t]) { lastSteps[t] = s; if (t == grid.getTrack()) gridDirty = true; }
    }
    if (proc.engine.playingPattern() != lastPattern || edit != lastPattern) gridDirty = true;
    lastPattern = proc.engine.playingPattern();
    if (gridDirty) { grid.repaint(); repaint (seqArea.withHeight (300).withWidth (140)); }
    repaint (displayArea);
    repaint (juce::Rectangle<int> (680, 14, 540, 80));
    repaint (masterArea);
}

// ================================================================================================ editor
AcidoEditor::AcidoEditor (AcidoProcessor& p) : AudioProcessorEditor (p), proc (p), panel (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (panel);
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) AcidoPanel::baseW / AcidoPanel::baseH);
    setResizeLimits (AcidoPanel::baseW / 2, AcidoPanel::baseH / 2, AcidoPanel::baseW * 2, AcidoPanel::baseH * 2);
    setSize ((int) (AcidoPanel::baseW * .9), (int) (AcidoPanel::baseH * .9));
}

AcidoEditor::~AcidoEditor() { setLookAndFeel (nullptr); }

void AcidoEditor::paint (juce::Graphics& g) { ui::paintChassis (g, getLocalBounds()); }

void AcidoEditor::resized()
{
    const float s = (float) getWidth() / (float) AcidoPanel::baseW;
    panel.setBounds (0, 0, AcidoPanel::baseW, AcidoPanel::baseH);
    panel.setTransform (juce::AffineTransform::scale (s));
}
