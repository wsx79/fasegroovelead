# Fase Groove Lead – Guida

*🇬🇧 [English guide](GUIDE.md)*

Questa guida descrive la [v0.1.2](https://github.com/wsx79/fasegroovelead/releases/tag/v0.1.2). Pulsanti, menu, colori e disposizione seguono gli altri strumenti della famiglia; le corsie sono dedicate alla melodia solista.

**Preset Arranger:** i tredici stili `Arranger: …` usano i profili comuni della famiglia. Scegli lo stesso genere negli altri strumenti e usa LINK per tempo e trasporto; attiva KEY per condividere tonalità e scala.

## 1. Installazione

**Windows:** avvia `FaseGrooveLead-Windows-Setup.exe` dalla pagina
[Releases](https://github.com/wsx79/fasegroovelead/releases/latest) (oppure scompatta lo zip e copia
i file come sotto). **macOS / Linux:** scarica lo zip per il tuo sistema e scompattalo.

| Sistema | Copia | In |
|---|---|---|
| Windows | `Fase Groove Lead.vst3` | `C:\Program Files\Common Files\VST3\` |
| macOS | `Fase Groove Lead.vst3` / `Fase Groove Lead.component` | `~/Library/Audio/Plug-Ins/VST3/` / `~/Library/Audio/Plug-Ins/Components/` |
| Linux | `Fase Groove Lead.vst3` | `~/.vst3/` |

Poi rifai la scansione dei plugin nella DAW. Su macOS, se il plugin è bloccato, esegui una volta nel
Terminale: `xattr -cr ~/Library/Audio/Plug-Ins/VST3/"Fase Groove Lead.vst3"` (e lo stesso per il
`.component`). L'app **Standalone** funziona senza DAW.

Caricalo come **strumento** su una traccia MIDI. Premi play nella DAW (o **PLAY** nel plugin) e il
groove suona con il synth lead guida.

## 2. La finestra

La stessa disposizione di Fase Groove Piano, Drums e Guitar:

- **Intestazione:** PRESET, TUTTO (tutto casuale), annulla / ripeti, schermo intero, STILE, RESET,
  OPZIONI; TRASPORTO E TEMPO (⏮ ▶ ■, SYNC DAW, il tempo, LINK, Sequencer); il display (groove,
  genere, l'accordo che suona, la sezione, dove si trova la riproduzione).
- **GROOVE:** il groove, il genere dei dadi, la densità, la durata delle parti e le parti A-D.
- **PARTE SOLISTA:** la riga scelta nella griglia. **ACCORDI:** tonalità, giro di accordi, i quattro accordi.
- **FEEL E BRANO**, **SUONO GUIDA E MIDI OUT**.
- La grande **griglia** (o VELOCITY, MIXER, FILL) ed **EXPORT MIDI E AUDIO**.

## 3. Il groove

- **GROOVE:** 43 groove in 13 generi (gli stessi di Fase Groove Piano e Drums); clicca il nome per il
  browser (generi a sinistra, ogni groove con l'immagine del suo ritmo). **Groove casuale** è quello
  di NUOVO GROOVE, **User pattern** quello modificato nella griglia.
- **CASUALE / GROOVE (dadi):** un nuovo groove casuale nel genere scelto in **GENERE DEL GROOVE**.
  **DENSITÀ** va da motivi radi a frasi fitte in sedicesimi.
- **Parti A-D:** il groove in quattro parti (mezza battuta ciascuna, o una battuta con **Durata
  parti: 1 bar**); suonale in qualsiasi ordine (A A A B...), rigenerane una con il suo dado, bloccala
  con il lucchetto perché i dadi la lascino stare.
- **STILE** (in alto): groove, accordi, swing, feel, fill e suono di un genere in un clic. **RESET**
  rimette tutto come all'inizio.

## 4. Accordi

- **Tonalità** (il menu a sinistra): gli slot si spostano con lei. L'armonia guida la melodia;
  non genera accompagnamenti. Le note forti risolvono sugli accordi, quelle di passaggio seguono
  tonalità e scala. Il seed sceglie un motivo ripetibile di due battute e la sua risposta.
- **GIRO DI ACCORDI...:** il browser della famiglia. Un clic carica il giro nella tonalità.
  A destra trovi un'**anteprima della melodia**, poi tonalità, **scala** (13), **colore** (triadi,
  settime, none) e numero di **accordi** (1–8, uno per battuta). **NUOVA ARMONIA** genera un altro
  giro nel genere scelto. Il dado accanto al browser fa la stessa cosa dalla finestra principale.
- **Slot degli accordi:** un accordo per battuta, a ciclo. Clicca uno slot per cambiarne
  fondamentale e tipo, copiare quello di un'altra battuta o applicarlo a tutte. Lo slot corrente
  si illumina. Dal browser puoi scegliere quanti slot suonano; sono disponibili triadi, settime,
  none, accordi sospesi, diminuiti e aumentati.
- **MIDI IN** (attivo di default): le note tenute sull'ingresso MIDI forniscono l'armonia al posto
  degli slot. Suona un accordo sulla tastiera o manda il **MIDI out di Fase Groove Piano** alla
  traccia di Lead: la melodia segue l'armonia con il proprio ritmo. Una nota sola fornisce un
  riferimento di fondamentale e quinta. Spento, il MIDI in ingresso passa e suona il synth guida.

**Ottava generale** traspone tutte le corsie Lead insieme (da −2 a +2), mantenendo gli scostamenti delle singole corsie. Si salva nei preset ed è automatizzabile nella DAW.

## 5. La griglia

12 corsie di frase × 32 step (due battute di sedicesimi, oppure 64 step con parti di una battuta):

| Riga | Cosa suona |
|---|---|
| **HOOK** | il motivo principale, scelto dal seed |
| **ANSWER** | un profilo melodico di risposta |
| **SHORT** (Staccato) | note corte su un profilo compatto |
| **PULSE** | note alternate per una pulsazione ritmica |
| **GHOST** | note intonate molto corte e leggere tra gli attacchi principali |
| **APPROACH** | un'appoggiatura da un semitono sotto alla nota d'arrivo |
| **LONG** | una nota melodica tenuta |
| **ARP** | una nota dell'accordo alla volta |
| **RUN** | un passaggio melodico in movimento |
| **HIGH** | un motivo nel registro acuto |
| **LOW** | un motivo nel registro più basso |
| **TONIC** | la fondamentale dell'armonia corrente |

Le righe appartengono a **una sola voce lead monofonica**. Ogni nuova nota generata chiude la
precedente; tra attacchi simultanei di un fill resta quello più forte. Le ghost hanno un'altezza,
gli arpeggi suonano note singole e HIGH/LOW non aggiungono una seconda voce di accompagnamento.

| Azione | Risultato |
|---|---|
| Clic | colpo (di nuovo clic: pausa) |
| Shift + clic | accento |
| Alt + clic | ghost, molto leggera |
| Ctrl / Cmd + clic | **RIPETI**: due attacchi rapidi |
| Trascina | dipinge |
| Clic destro su uno step | probabilità, roll di 2–4 ripetizioni, simbolo e strumenti di spostamento |
| Clic sul nome di una riga | la ascolti e la scegli nella PARTE SOLISTA |
| Doppio clic sul nome di una riga | le sue velocity |
| Clic destro sul nome di una riga | mute, nuova linea, svuota, ottava, livello |
| Dado di una riga | una nuova linea soltanto per quella riga |

La prima modifica di un preset lo trasforma nello **User pattern**; il preset resta com'era.

## 6. Parte solista

La corsia scelta: **MUTE**, il suo dado, **OTTAVA**, **ASCOLTA**, **LIVELLO** (velocity MIDI) e
**VOLUME** (solo audio guida). **SUONA** mostra l'altezza melodica nel contesto corrente.
Ottava cambia le altezze; Livello cambia la dinamica MIDI e del suono guida, mentre Volume
regola soltanto il contributo della corsia al suono guida. La vista principale **VOLUMI** offre
gli stessi fader audio; **EDITOR...** apre griglia, velocity, mixer e fill a finestra intera.

## 7. Feel

- **SWING:** ritarda un sedicesimo sì e uno no: 50% dritto, 67% shuffle in terzina, 75% il più
  pesante. Ogni genere usa la stessa convenzione degli altri strumenti della famiglia.
- **UMANIZZA:** piccole variazioni di timing e velocity; le note leggere vanno e vengono nella
  frase. **NUOVO FEEL** crea un'altra umanizzazione ripetibile, salvata con il progetto.
- **VELOCITY:** dinamica generale. **ACCENTS:** dà risalto alle risposte, pulsazioni, ghost e
  arpeggi in levare e alleggerisce gli altri sedicesimi (0%: uniforme).
- **MIXER:** push/pull di ogni riga in millisecondi e la sua quota di swing. **TIMING...** offre
  feel pronti: rilassato, in anticipo, jackin', neo soul.
- **TEMPLATE...:** applica timing e accenti di un loop audio o file MIDI al groove corrente.
  Trascina un loop audio sulla finestra per usarlo come template.

## 8. Brano

- **Fill:** spenti, oppure ogni 1, 2, 4 o 8 battute. Gli stili sono **Motif build**, **High run**,
  **Ghost build**, **Pulse lift**, **Drop out**, **Approach up**, **Staccato stutter**,
  **Melodic run**, **Arpeggio run**, **Low run**, **Long note**, **Mix** e **Custom**.
  Disegna il tuo fill di una battuta nella vista FILL. Anche i fill restano monofonici.
- **RISOLVI:** un attacco del motivo sulla battuta dopo il fill, e ogni otto battute del drop.
- **Variazione:** ogni quattro o otto battute l'ultima cambia, con anticipi, passaggi o pause.
- **Sezione del brano:** Intro / Outro radi, Break sostenuto, Build-up crescente, Main / Drop pieni.
- **BRANO INTERO...:** scegli battute, variazione e fill conclusivo di ogni sezione. Trascina o
  salva tutto il brano con i marker, una sezione, oppure ogni sezione come clip separata.

## 9. Suono e MIDI

- **Suono guida:** il synth lead interno: **Soft sine**, **Warm triangle**, **Bright saw** e
  **Round pulse**, con **TIMBRO**, **RIVERBERO** e **VOLUME**. Spegnilo per usare solo il MIDI con
  il tuo strumento lead. Timbro regola la brillantezza; le quattro forme d'onda restano distinte.
- **RITARDO ATTACCO:** ritardo della frase in millisecondi. Regola anche la durata delle
  appoggiature della corsia APPROACH. Il timing viene incluso nel MIDI esportato.
- **CANALE MIDI:** il canale delle note in uscita, 1 di default.
- **Sequencer:** spento, il groove non segue il play della DAW, utile quando la clip esportata
  suona già sulla traccia. Il play del plugin permette comunque l'ascolto.
- **Standalone:** scegli dispositivo audio e porte MIDI nelle impostazioni audio/MIDI di JUCE.
  Senza DAW puoi usare tempo interno, LINK o clock MIDI in ingresso per tempo e trasporto.

## 10. Export e import

- **TRASCINA MIDI NELLA DAW:** trascina il riquadro arancione su una traccia con lo strumento
  lead. **Note:** tutte le righe su una traccia, oppure una traccia per riga. **Lunghezza clip:**
  loop completo, 1–16 battute o brano intero. Il loop completo comprende armonia, ritmo, fill
  e variazioni, fino a quando tornano all'inizio insieme.
- **SALVA .MID...:** salva la stessa clip in un file. **SALVA .WAV...:** esporta il suono guida
  come loop senza stacchi, brano completo o stem per corsia.
- **IMPORTA...** (o trascina un `.mid`): importa il ritmo nelle corsie adatte e stima l'armonia
  delle prime battute per gli slot. È un'importazione di groove e armonia, non una trascrizione
  esatta: Lead genera le altezze dal motivo, dalla scala e dall'armonia correnti.
- **I MIEI GROOVE:** salva la griglia e ricaricala in ogni progetto. **PRESET:** salva tutta
  l'impostazione. Anche il progetto della DAW conserva le impostazioni del plugin.
- Clip trascinate, preset e groove si trovano in **Documenti/Fase Groove Lead/**. Un salvataggio
  fallito conserva il file precedente; il MIDI esportato chiude tutte le note alla fine della clip.

## 11. LINK

**LINK**, nel trasporto, collega Lead con **Fase Groove Piano**, **Drums**, **Guitar** e altre
istanze: genere, swing, tempo, sync DAW e trasporto usano il protocollo comune della famiglia.
Puoi cambiare un'impostazione condivisa da qualsiasi strumento collegato. **KEY**, attivo negli
strumenti collegati, condivide anche tonalità e scala. Per seguire gli accordi del piano, manda
il suo MIDI out a Lead e attiva MIDI IN (sezione 4). LINK condivide le impostazioni tonali,
non il flusso delle note degli accordi.

## 12. Scorciatoie e opzioni

- **Spazio:** play / pausa. **Ctrl+Z / Ctrl+Shift+Z (Ctrl+Y):** annulla / ripeti.
- **OPZIONI** (o un clic sul logo): lingua (English, Italiano, Español, Français, Deutsch,
  Português), dimensione dell'interfaccia, suggerimenti, aggiornamenti automatici; **INFO**: crediti e link.

## 13. Suonare con la famiglia

- **KEY**, accanto a Sequencer, mostra tonalità e scala. Con LINK e KEY attivi negli strumenti,
  una modifica aggiorna gli altri. Passare alla relativa maggiore/minore conserva gli accordi.
- Lascia a Piano, Guitar e Drums l'accompagnamento e usa Lead per la linea melodica. Lead non
  attiva la modalità di Guitar che riduce l'accompagnamento del piano.
- **PLAYING...**, in SUONO GUIDA E MIDI OUT:
  - *Hold hooks until the next lead note*: prolunga le note dei motivi; la nota seguente chiude
    comunque quella precedente.
  - *Upper register*: alza la melodia di un'ottava per lasciare spazio a basso e piano.
  - *Push chord changes*: l'ultimo ottavo può usare l'armonia della battuta seguente.
  - *Articulation keyswitches and expression swells*: keyswitch opzionali per strumenti esterni
    (Normal, Short, Ghost, Approach) e crescendi di espressione CC 11. Scegli le note dei
    keyswitch dal menu e imposta la stessa mappa nello strumento ricevente. I keyswitch non
    suonano nel synth guida e nel MIDI possono comparire separati dalle note melodiche.
- Usa **LIVELLO** per la dinamica inviata allo strumento esterno e **VOLUME** per il mix del
  suono guida. Salva un preset quando melodia e accompagnamento stanno bene insieme.
