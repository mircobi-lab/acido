#pragma once
#include "Pattern.h"
#include <juce_core/juce_core.h>

/*  Generatore casuale di acido.
    Sceglie uno stile (acid, techno, funk, hip hop, R&B), una tonica e una scala adatta allo stile, poi scrive:
      - la batteria da modelli ritmici tipici dello stile, con variazioni casuali;
      - BASS 1: linea su motivo di 4 o 8 passi ripetuto con variazioni (nell'hip hop e nell'R&B segue la cassa);
      - BASS 2: contro-linea più alta sulle note dell'accordo, dove BASS 1 lascia spazio.
    Le note restano sempre dentro la scala scelta.                                                         */
namespace acido::gen
{
enum Style { AUTO = 0, ACID, TECHNO, FUNK, HIPHOP, RNB, numStyles };
const char* styleName (int style);
const char* scaleName (int scale);
const std::vector<int>& scaleSteps (int scale);
int numScales();
juce::String keyName (int root);

struct Result
{
    int style = ACID, root = 0, scale = 0;
    float bpm = 128, swing = 50;
};

// what: 0 = BASS 1, 1 = BASS 2, 2 = batteria, 3 = tutto.
// Se il pattern ha già una tonalità e si rigenera una sola traccia, la tonalità resta la stessa.
Result generate (PatternStore&, int pattern, int what, int style, juce::Random&);
}
