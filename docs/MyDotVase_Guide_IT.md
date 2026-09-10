# MyDot Vase Kit

Guida completa all'installazione e all'utilizzo dello sketch
`examples/MyDotVase` incluso nella libreria Arduino MyDot.

L'esempio trasforma la carrier MyDot in un controller connesso per la cura
delle piante. Legge un sensore analogico di umidità del terreno, controlla una
pompa tramite il relè della carrier, pilota i NeoPixel, riceve comandi dal cloud
Microeden e salva in memoria non volatile le impostazioni delle luci e della
pompa.

## 1. Hardware e sicurezza

### Hardware necessario

- carrier Microeden MyDot V1.0;
- una scheda Arduino supportata (l'esempio è testato con Arduino Nano ESP32);
- sensore analogico di umidità del terreno AZ-Delivery;
- pompa a bassa tensione e alimentatore esterno adatto alla pompa;
- tubo e serbatoio dell'acqua;
- stampante 3D per i componenti fisici del Vase Kit;
- dispositivo Microeden configurato sul cloud e rete Wi-Fi.

### Componenti stampati in 3D

Per realizzare i componenti fisici del Vase Kit, come contenitore della pianta,
staffe, supporti per il sensore e porta pompa/tubo, è necessaria una stampante
3D. La libreria Arduino contiene l'esempio firmware; i componenti meccanici
devono essere stampati e assemblati separatamente in base al design Vase Kit
utilizzato.

### File STL da stampare

Scarica e stampa i seguenti file per assemblare il Vase Kit:

| File | Componente | STL |
| --- | --- | --- |
| `column.stl` | Sezione standard della colonna; stampare il numero di pezzi necessario per l'altezza della pianta | [Scarica STL](https://microeden.io/files/mydot/vasekit/column.stl) |
| `mydot_pump_support.stl` | Supporto pompa | [Scarica STL](https://microeden.io/files/mydot/vasekit/mydot_pump_support.stl) |
| `mydot_support.stl` | Supporto case MyDot | [Scarica STL](https://microeden.io/files/mydot/vasekit/mydot_support.stl) |
| `vase_bottom.stl` | Sottovaso | [Scarica STL](https://microeden.io/files/mydot/vasekit/vase_bottom.stl) |
| `vase_top.stl` | Vaso | [Scarica STL](https://microeden.io/files/mydot/vasekit/vase_top.stl) |

Il file `column.stl` è una sezione standard della colonna. Stampa il numero di
pezzi identici necessario in base all'altezza della pianta e assemblali tra
loro. Non scalare il file e non modificare l'altezza della singola sezione.

### Materiali esterni consigliati

I seguenti materiali esterni sono quelli utilizzati come riferimento per il
prototipo Vase Kit:

- [Tubo trasparente flessibile – ANOMM](https://www.amazon.it/ANOMM-Trasparente-Flessibile-Lunghezza-Trasporto/dp/B0FCY1N6PM)
- [Pompa elettrica autoadescante – RUNCCI-YUN](https://www.amazon.it/RUNCCI-YUN-Membrana-Elettrica-autoadescante-macchina/dp/B0CB3QGFX2)
- [Sensore capacitivo di umidità del terreno – AZ-Delivery](https://www.amazon.it/Capacitive-Moisture-Corrosion-Resistant-Interface/dp/B07RF2PTD6)
- [Jumper maschio-maschio](https://www.amazon.it/jumper-maschio-backplane-connessione-rapida/dp/B0DHGJP47D)

I link sono riferimenti a prodotti esterni, non dipendenze della libreria e non
costituiscono una garanzia di compatibilità. Prima del montaggio verifica il
diametro interno del tubo, tensione e corrente della pompa, portata e
compatibilità del relè/driver con l'hardware specifico.

### Alimentazione

Alimenta la carrier tramite il jack DC esterno quando utilizzi relè, pompa,
NeoPixel o driver della ventola. La sola alimentazione USB non è sufficiente per
gli stadi di uscita della carrier. Rispetta il range di tensione specificato
per la carrier e per il driver della pompa e verifica la polarità prima di
collegare l'alimentazione.

La pompa deve essere alimentata tramite il relè della carrier o tramite un
driver esterno appropriato. Non collegare mai una pompa o un motore direttamente
a un GPIO Arduino.

### Attenzione al sensore del terreno

Alimenta il sensore AZ-Delivery a **3,3 V**, non a 5 V. Un'uscita analogica a
5 V può superare il range di ingresso della scheda selezionata e danneggiarla.

| Collegamento sensore | Collegamento MyDot/Nano |
| --- | --- |
| `VCC` | `3V3` |
| `GND` | `GND` |
| `AO` / uscita analogica | `A0` |

### Pin della Nano ESP32

Quando è selezionata una Arduino Nano ESP32, la libreria MyDot utilizza questi
alias della carrier:

| Funzione | Alias Nano ESP32 |
| --- | --- |
| Ingresso analogico sensore | `A0` |
| Pulsante A | `A7` |
| Pulsante B | `D4` |
| Relè | `D2` |
| NeoPixel | `D3` |
| Chip select SD | `D10` |

La libreria supporta sia la numerazione Arduino sia la modalità **By GPIO number
(legacy)**. È consigliata la normale numerazione Arduino; utilizza l'opzione
legacy solo se richiesta dal resto del progetto.

## 2. Installazione dell'esempio

1. Installa **MyDot** dal Library Manager dell'Arduino IDE oppure copia la
   libreria nella cartella delle librerie Arduino.
2. Installa le dipendenze dichiarate in `library.properties` se l'IDE non le
   installa automaticamente.
3. Seleziona **Arduino Nano ESP32** e la porta seriale corretta.
4. Apri **File > Examples > MyDot > MyDotVase**.
5. Apri la scheda `microeden_secrets.h` e sostituisci localmente i placeholder:

   ```cpp
   #define SECRET_WIFI_SSID "YOUR_WIFI_SSID"
   #define SECRET_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
   #define SECRET_DEVICE_ID "YOUR_DEVICE_ID"
   #define SECRET_DEVICE_TOKEN "YOUR_DEVICE_TOKEN"
   ```

   Non fare mai il commit di credenziali Wi-Fi o token cloud reali.
6. Collega l'alimentazione esterna della carrier, collega la scheda via USB e
   carica lo sketch.
7. Apri il Monitor Seriale a **115200 baud**.

La connessione cloud su ESP32 utilizza il certificato ISRG Root X1 incluso nella
libreria. Non è necessario abilitare TLS insicuro.

## 3. Funzionamento dello sketch

Il codice si trova in:

- [`examples/MyDotVase/MyDotVase.ino`](https://github.com/Microeden/MyDotLib/blob/main/examples/MyDotVase/MyDotVase.ino)
- [`examples/MyDotVase/MyDotVaseState.h`](https://github.com/Microeden/MyDotLib/blob/main/examples/MyDotVase/MyDotVaseState.h)

Lo sketch esegue queste operazioni:

1. Inizializza le periferiche MyDot e il backend per la memoria persistente.
2. Recupera modalità, stato e luminosità delle luci e durata della pompa.
3. Avvia Wi-Fi e connessione MQTT al cloud.
4. Legge e media otto campioni ADC del sensore del terreno.
5. Invia telemetria su terreno, pompa, luci ed eventi.
6. Gestisce comandi cloud, slider e pulsanti locali.
7. Gestisce il timer della pompa e la riconnessione automatica Wi-Fi/MQTT.

Il `loop()` deve continuare a chiamare `dot.run()`. Non sostituirlo con ritardi
bloccanti di lunga durata.

## 4. Controlli locali

- **Pulsante A**: accende e spegne le luci.
- **Pulsante B**: passa tra le cinque modalità:
  `coolWhite`, `warmWhite`, `growVegetative`, `growBloom` e `growFull`.

Le azioni dei pulsanti vengono salvate immediatamente.

## 5. Controlli cloud

Il campo dei comandi cloud normalmente si chiama `content`.

### Comandi

| Comando | Effetto |
| --- | --- |
| `pump` | Avvia un ciclo temporizzato con la durata salvata. |
| `lights_on` | Accende i NeoPixel con modalità e luminosità salvate. |
| `lights_off` | Spegne i NeoPixel senza perdere la luminosità. |
| `lights_warm` | Seleziona la luce bianco caldo. |
| `lights_cool` | Seleziona la luce bianco freddo. |
| `grow_veg` | Seleziona la modalità grow vegetativa. |
| `grow_bloom` | Seleziona la modalità grow fioritura. |
| `grow_full` | Seleziona il preset grow completo. |
| `lights_mode` | Usa un campo numerico aggiuntivo `mode` da 0 a 4. |

I vecchi comandi continui `on` e `off` della pompa non vengono più gestiti. La
pompa viene attivata solo da `pump` e si spegne automaticamente.

### Widget slider

I messaggi degli slider utilizzano il formato compatto `chiave_valore` nel
campo `content` del cloud.

| Chiave slider | Esempio messaggio | Intervallo | Salvato |
| --- | --- | ---: | --- |
| `brightness` | `brightness_128` | 0–255 | Sì |
| `pumpDuration` | `pumpDuration_5` | 1–60 secondi | Sì |

Il valore dello slider viene consumato una sola volta come evento. Un'eco di
stato o telemetria dal cloud non può essere interpretata come un nuovo comando
e non può riportare il valore a zero.

### Campi di telemetria

Lo sketch pubblica questi campi:

| Campo | Significato |
| --- | --- |
| `soilRaw` | Lettura ADC mediata da `A0`. |
| `soilPercent` | Percentuale del terreno dopo la calibrazione. |
| `pumpOn` | Stato corrente del relè, in sola lettura. |
| `pumpActivations` | Numero di attivazioni dall'avvio. |
| `pumpDuration` | Durata configurata per la prossima attivazione. |
| `lightsOn` | Stato corrente delle luci. |
| `lightMode` | Nome della modalità corrente. |
| `lightModeIndex` | Indice della modalità corrente, 0–4. |
| `brightness` | Luminosità corrente dei NeoPixel, 0–255. |
| `event` | Motivo dell'ultimo messaggio di telemetria. |

Gli eventi più comuni sono `periodic`, `pump`, `pumpStop`, `pumpBusy`,
`pumpCooldown`, `brightness`, `pumpDuration`, `lightsOn` e `lightsOff`.

## 6. Temporizzazione e sicurezza della pompa

Lo slider della durata è limitato dal codice a 1–60 secondi. Il valore
predefinito è due secondi. Quando arriva `pump`:

1. il relè viene attivato;
2. `pumpOn` diventa `true`;
3. `pumpActivations` viene incrementato;
4. viene inviata la telemetria `pump`;
5. il relè viene disattivato automaticamente dopo la durata configurata;
6. viene inviata la telemetria `pumpStop`.

È inoltre presente un cooldown di un minuto, misurato dall'inizio dell'ultima
attivazione. Un secondo comando durante il ciclo produce `pumpBusy`; un comando
durante il cooldown produce `pumpCooldown`. Questo impedisce a un widget
malfunzionante, a messaggi MQTT ripetuti o a un'automazione di riavviare
continuamente la pompa.

Il cooldown protegge il funzionamento durante l'avvio corrente. Viene azzerato
al riavvio della scheda; la durata configurata invece rimane salvata.

### Gestione dell'acqua

Prima di avviare un ciclo di irrigazione controlla che ci sia acqua disponibile
nel sottovaso/serbatoio. Dopo l'irrigazione controlla il sottovaso e ricordati di
svuotarlo periodicamente, per evitare ristagni o traboccamenti. Il sottovaso va
controllato e svuotato regolarmente durante la normale manutenzione.

Per il Vase Kit stampabile in 3D, inizia impostando una durata di irrigazione di
**massimo un secondo**. Il kit stampabile ha una capacità d'acqua ridotta e un
ciclo più lungo può riempire rapidamente il sottovaso. Se la pompa viene usata
con un sistema diverso o con un vaso più grande, regola il tempo di irrigazione
in base alle dimensioni del vaso, al tubo, alla portata della pompa e al
drenaggio. Prima di abilitare il funzionamento automatico prova sempre la durata
scelta sotto supervisione.

## 7. Calibrazione del sensore terreno

Lo sketch definisce:

```cpp
const int SOIL_RAW_DRY = 3000;
const int SOIL_RAW_WET = 1300;
```

La conversione predefinita assume che il valore ADC diminuisca quando il
terreno diventa più bagnato. `soilPercentFromRaw()` associa il valore secco a
0% e quello bagnato a 100%, limitando poi il risultato all'intervallo 0–100.

Per calibrare il sensore:

1. leggi `soilRaw` con la sonda nel terreno secco e annota il valore;
2. leggi il valore nel terreno ben bagnato;
3. sostituisci `SOIL_RAW_DRY` e `SOIL_RAW_WET` nello sketch;
4. ricompila e carica il programma.

Non alimentare il sensore a 5 V durante la calibrazione.

## 8. Stato persistente

Il record salvato contiene:

- `lightsOn`;
- `mode`;
- `brightness`;
- `pumpDurationSeconds`.

Il backend viene scelto automaticamente in base all'architettura:

- Nano ESP32: `Preferences` / NVS di ESP32;
- Nano RP2040 Connect con core Mbed: Mbed KVStore;
- core RP2040 o Nano 33 IoT con supporto EEPROM: EEPROM.

La modifica dello stato luci, della luminosità o dello slider della durata viene
salvata subito. La telemetria viene ritardata solo brevemente per evitare di
intasare il cloud mentre uno slider viene trascinato.

## 9. Diagnostica seriale

Usa il Monitor Seriale a 115200 baud. Sono utili questi messaggi:

- `Restored light state...`;
- `Restored pump duration...`;
- `Pump duration from cloud...`;
- `Pump request ignored: a timed cycle is already active`;
- `Pump request ignored: cooldown active...`;
- `Telemetry - ...`;
- `MyDot: MQTT connected.` e i messaggi di riconnessione.

L'esempio Display è dedicato alla diagnostica OLED. `MyDotVase` utilizza il
Monitor Seriale per lasciare visibili contemporaneamente messaggi del terreno,
del cloud, della pompa e della memoria persistente.

## 10. Risoluzione dei problemi

### Lo sketch compila ma la pompa non parte

- verifica che l'alimentazione esterna della carrier sia collegata;
- verifica che la pompa abbia un alimentatore adeguato;
- controlla cablaggio del relè/driver e massa comune;
- verifica nel Monitor Seriale i messaggi `pump` e `pumpStop`;
- controlla che non sia attivo il cooldown di un minuto.

### La pompa non riparte

È normale durante il ciclo attivo e fino a 60 secondi dal suo inizio. Attendi
la fine del cooldown o controlla la telemetria `pumpCooldown`.

### Lo slider non sembra cambiare valore

- verifica che la chiave della dashboard sia esattamente `brightness` o
  `pumpDuration`;
- verifica che il contenuto ricevuto sia `brightness_<valore>` o
  `pumpDuration_<secondi>`;
- verifica che il valore rientri nell'intervallo documentato;
- controlla il messaggio `from cloud` nel Monitor Seriale.

### Dopo il riavvio le luci sono spente

- controlla i messaggi ripristinati `lightsOn`, `brightness` e `lightMode`;
- verifica che la carrier sia alimentata dal jack esterno;
- verifica che il pin dati NeoPixel sia l'alias `D3` sulla Nano ESP32;
- assicurati che la libreria installata corrisponda alla versione dello sketch.

### I valori del terreno sono invertiti o fuori scala

- verifica che il sensore sia alimentato a 3,3 V;
- verifica che `AO` sia collegato ad `A0`;
- calibra `SOIL_RAW_DRY` e `SOIL_RAW_WET` sul terreno e sulla sonda reali.

## 11. Sequenza di test consigliata

1. Avvia lo sketch con la pompa scollegata e verifica la telemetria `soilRaw`.
2. Accendi e spegni le luci con il pulsante A.
3. Cambia modalità con il pulsante B.
4. Modifica `brightness` e verifica che il valore resti dopo un riavvio.
5. Imposta `pumpDuration` a un valore breve, ad esempio 1–2 secondi.
6. Invia una volta `pump` e verifica lo spegnimento automatico.
7. Invia subito un altro `pump` e verifica che venga rifiutato dal cooldown.
8. Collega la pompa solo dopo aver verificato relè e temporizzazione.
