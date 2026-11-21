/*  Applicazione Standalone di acido con lo stile MBStudio anche per finestra, menu e dialoghi.
    - al primo avvio abilita tutti gli ingressi MIDI collegati (tastiere e controller);
    - argomenti di prova: --pattern N  --track 1..3  --run  --shot FILE.png  (salva un'immagine del pannello ed esce)        */
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Generator.h"
#include <juce_audio_plugin_client/juce_audio_plugin_client.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

class AcidoStandaloneApp final : public juce::JUCEApplication
{
public:
    AcidoStandaloneApp()
    {
        juce::PropertiesFile::Options o;
        o.applicationName = juce::CharPointer_UTF8 (JucePlugin_Name);
        o.filenameSuffix = ".settings";
        o.osxLibrarySubFolder = "Application Support";
       #if JUCE_LINUX || JUCE_BSD
        o.folderName = "~/.config";
       #endif
        appProperties.setStorageParameters (o);
    }

    const juce::String getApplicationName() override     { return juce::CharPointer_UTF8 (JucePlugin_Name); }
    const juce::String getApplicationVersion() override  { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override           { return true; }
    void anotherInstanceStarted (const juce::String&) override {}

    void initialise (const juce::String& commandLine) override
    {
        auto* settings = appProperties.getUserSettings();
        const bool firstRun = ! settings->containsKey ("acidoFirstRunDone");
        settings->setValue ("acidoFirstRunDone", true);

        juce::LookAndFeel::setDefaultLookAndFeel (&hardwareLook);
        mainWindow = std::make_unique<juce::StandaloneFilterWindow> (getApplicationName(), mbstudio::ui::colours::chassisBottom,
                                                                     settings, false);
        if (firstRun)
            for (auto& dev : juce::MidiInput::getAvailableDevices())
                mainWindow->pluginHolder->deviceManager.setMidiInputDeviceEnabled (dev.identifier, true);
        mainWindow->setVisible (true);
        // profilo del controller scelto dal nome della porta MIDI (solo se non ci sono gia' assegnazioni)
        if (auto* proc = dynamic_cast<AcidoProcessor*> (mainWindow->getAudioProcessor()))
        {
            juce::StringArray ports;
            for (auto& dev : juce::MidiInput::getAvailableDevices())
                if (mainWindow->pluginHolder->deviceManager.isMidiInputDeviceEnabled (dev.identifier)) ports.add (dev.name);
            proc->midiMap.autoSelectFromPorts (ports);
        }
        applyCommandLine (commandLine);
    }

    void applyCommandLine (const juce::String& cmd)
    {
        auto args = juce::StringArray::fromTokens (cmd, true);
        auto* proc = dynamic_cast<AcidoProcessor*> (mainWindow->getAudioProcessor());
        if (proc == nullptr) return;
        juce::String shot;
        int track = -1;
        if (args.contains ("--run")) proc->setParamValue (acido::RUN, 1.f);
        for (int i = 0; i + 1 < args.size(); ++i)
        {
            const auto a = args[i], v = args[i + 1].unquoted();
            if (a == "--pattern") proc->setParamValue (acido::PATTERN, (float) v.getIntValue());
            else if (a == "--track") track = v.getIntValue() - 1;
            else if (a == "--random")
            {
                juce::Random r (v.getIntValue() * 7919 + 1);
                const int pat = (int) std::lround (proc->paramValue (acido::PATTERN)) - 1;
                acido::gen::generate (proc->patterns, pat, 3, v.getIntValue(), r);
            }
            else if (a == "--shot") shot = v;
            else if (a == "--profile") proc->midiMap.applyProfile (v.getIntValue());
            else if (a == "--learn") proc->midiMap.setLearnMode (true);
        }
        if (track >= 0)
            juce::Timer::callAfterDelay (300, [this, track]
            {
                if (auto* ed = dynamic_cast<AcidoEditor*> (mainWindow->getAudioProcessor()->getActiveEditor())) ed->selectTrack (track);
            });
        if (shot.isNotEmpty())
            juce::Timer::callAfterDelay (1500, [this, shot]
            {
                if (auto* ed = mainWindow->getAudioProcessor()->getActiveEditor())
                {
                    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.f);
                    juce::File f (shot); f.deleteFile();
                    if (auto os = f.createOutputStream()) juce::PNGImageFormat().writeImageToStream (img, *os);
                }
                systemRequestedQuit();
            });
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
        appProperties.saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        if (mainWindow != nullptr) mainWindow->pluginHolder->savePluginState();
        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
            juce::Timer::callAfterDelay (100, [] { if (auto app = JUCEApplicationBase::getInstance()) app->systemRequestedQuit(); });
        else
            quit();
    }

private:
    juce::ApplicationProperties appProperties;
    mbstudio::ui::HardwareLookAndFeel hardwareLook;
    std::unique_ptr<juce::StandaloneFilterWindow> mainWindow;
};

JUCE_CREATE_APPLICATION_DEFINE (AcidoStandaloneApp)
