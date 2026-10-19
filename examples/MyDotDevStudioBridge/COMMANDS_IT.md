# MyDot Dev Studio Bridge — Riferimento dei comandi seriali

Questa è la documentazione completa dell’esempio
`MyDotDevStudioBridge.ino`.

Il bridge legge una riga alla volta dalla `Serial` a **115200 baud**. Ogni
comando deve terminare con un carattere newline. I comandi e i sottocomandi
non distinguono maiuscole e minuscole; i valori testuali, come SSID Wi-Fi e
token cloud, mantengono invece il loro contenuto originale.
I dati ricevuti dal browser vengono elaborati in gruppi limitati (al massimo
un comando completo per passaggio del loop), lasciando tempo al runtime e
alla rete.

Nel Serial Monitor selezionare **Newline** come terminazione di riga.

## Risposte e avvio

Le principali risposte del bridge sono:

```text
OK ...       Comando accettato
ERR ...      Comando rifiutato o non riuscito
INFO ...     Messaggio informativo
STATUS ...   Stato del runtime, dei collegamenti e delle risorse
```

Il Dev Studio usa inoltre un canale USB prioritario per l'arresto: il byte
ETX (`0x03`) interrompe `RUN` anche durante una lettura o scrittura lunga della
microSD. È una funzione del bridge aggiornato; con firmware precedenti resta
disponibile il comando testuale `STOP`, ma non può anticipare un'operazione già
in corso.

Il Bridge non stampa messaggi diagnostici di avvio quando i log dettagliati
sono disattivati. Inviare
`STATUS` per verificare se la microSD è pronta e se il runtime salvato è in
esecuzione.

Per usare relay, NeoPixel e ventola alimentare la carrier tramite il jack DC
esterno. La sola USB è sufficiente per la comunicazione seriale, ma può non
fornire corrente sufficiente agli stadi di potenza.

## Comando CAPS strutturato

`CAPS` restituisce la versione di protocollo `2`. Oltre all’elenco sintetico,
stampa una riga `CAPS COMMAND` per ogni comando:

```text
CAPS COMMAND DISPLAY_TEXT args=text type=string
CAPS COMMAND RELAY args=state type=enum values=ON|OFF|TOGGLE|STATUS result=variable
CAPS COMMAND SENSOR_READ args=field,variable type=bme690 values=TEMPERATURE|PRESSURE|HUMIDITY|GAS
```

L’identificatore del comando usa gli underscore e può essere usato come chiave
stabile dall’editor. `args` elenca gli argomenti posizionali, `type` indica la
famiglia API, mentre `values` e `range` descrivono i valori ammessi.

## Comandi principali

| Comando | Descrizione |
| --- | --- |
| `HELP` | Stampa l’elenco breve dei comandi. |
| `CAPS` | Stampa capacità, argomenti, limiti e formato del file. |
| `STATUS` | Mostra modello della scheda, collegamento/runtime, Wi‑Fi, My Microeden, capacità microSD e memoria heap ESP32 quando supportate. |
| `LOGS ON|OFF|STATUS` | Attiva, disattiva o controlla i messaggi diagnostici spontanei del Bridge e della libreria MyDot. Dopo ogni avvio il valore predefinito è `OFF`. |
| `ADD <runtime-command>` | Aggiunge un comando alla sequenza in memoria. |
| `LIST` | Elenca la sequenza con indici a partire da zero. |
| `CLEAR` | Conferma subito la cancellazione della sequenza in memoria, poi ferma l’esecuzione e porta relay, ventola e NeoPixel allo stato sicuro. Non cancella il file SD. |
| `SAVE` | Cifra e salva la sequenza in `/MyDot.run`. |
| `LOAD` | Legge, decifra, verifica e carica il file runtime. |
| `RUN` | Avvia la sequenza dal comando zero senza bloccare il loop. |
| `STOP` | Ferma la sequenza, disattiva il watchdog runtime, spegne relay, ventola e NeoPixel e pulisce il display OLED. |
| `REBOOT` | Arresta in sicurezza il runtime e riavvia la scheda MyDot usando il reset nativo del core Arduino selezionato. |
| `EXEC <runtime-command>` | Esegue immediatamente un singolo comando. |

Il Bridge è silenzioso per impostazione predefinita: non invia all'USB i
messaggi di avvio, i log dei blocchi (`I2C DATA`, `VAR`, `OK WIDGET WRITE`),
gli avvisi/errori del runtime né i messaggi Wi-Fi/MQTT della libreria MyDot.
`LOGS ON` li attiva temporaneamente per il debug; `LOGS OFF` li disattiva e
`LOGS STATUS` mostra l'impostazione corrente. L'impostazione non viene salvata
in `/MyDot.run`. Restano sempre visibili le risposte ai comandi richiesti
(inclusi `EXEC` e lo `STOP` di emergenza) e le istruzioni runtime esplicite
`SERIAL <testo>`, finché il lettore USB riesce a riceverle.
Nella sequenza residente, `SERIAL` invia al massimo una riga ogni 100 ms
considerando tutti i task. Le istruzioni più frequenti continuano a girare,
ma le loro righe seriali vengono scartate; `EXEC SERIAL` e le risposte del
protocollo non sono limitati. Questo evita di saturare l'USB con programmi
ad alta frequenza. `STATUS` include `runtimeLogs=on|off`. Dopo una
disconnessione si può interrogare `STATUS` oppure attivare i log prima di
riprodurre il problema.
Le descrizioni seguenti dell'output seriale ordinario dei comandi runtime si
riferiscono a `EXEC` oppure alla sequenza residente con `LOGS ON`.

Su Nano ESP32, Nano RP2040 Connect e Nano 33 IoT, il Bridge applica un limite
di tempo all'output seriale. Se il browser non consuma i dati USB, l'output del runtime
viene scartato anziché accumularsi senza limite o fermare il programma.
Le risposte del protocollo hanno un limite separato e più lungo, ma possono
risultare incomplete se la connessione è bloccata. Il campo
`STATUS serialDroppedBytes` conta i byte scartati dall'avvio;
`serialSkippedLines` conta le righe `SERIAL` scartate dal limite di 100 ms.
La riduzione del traffico USB non limita una chiamata bloccata dentro il core
della scheda, non garantisce il recupero da un blocco interno, non sostituisce
il watchdog e non copre altre operazioni
di I/O bloccanti.

I comandi watchdog portabili sono disponibili come istruzioni runtime e anche
tramite `EXEC`:

```text
WATCHDOG BEGIN <timeout-ms>
WATCHDOG FEED
WATCHDOG STOP
```

`BEGIN` attiva il watchdog cooperativo, `FEED` rinnova la scadenza e `STOP` lo
disattiva. Il Bridge controlla la scadenza da `MyDot::run()` e riavvia la
scheda quando il tempo è superato. Inserire un blocco `Alimenta watchdog` nel
flusso continuo; la libreria non lo alimenta automaticamente, così un flusso
bloccato può essere rilevato. `CAPS FEATURE watchdog=cooperative` identifica
questo backend.

`STATUS` mantiene i campi chiave/valore già presenti e aggiunge `board` (nome
della scheda rilevato dal core Arduino), `serialDroppedBytes`, `wifi`,
`wifiRssi`, `cloud`, `sdCardBytes`, `sdTotalBytes`, `sdUsedBytes`,
`sdFreeBytes`, `ramFreeBytes` e `ramTotalBytes`. `sdCardBytes` è la capacità
fisica della scheda (`cardSize()`); `sdTotalBytes` è la capacità utilizzabile
del filesystem. Le dimensioni vengono restituite quando la scheda espone
l’API corrispondente; altrimenti il valore è `unknown`.

Su ESP32, `STATUS` mostra anche `taskWatchdog` (`ready` oppure `unavailable`),
`resetReason`, `heapLargestBytes` e
`heapMinimumBytes`. Dopo un riavvio non dovuto a spegnimento include
`previousStage`, `previousIndex`, `previousHeapBytes`,
`previousLargestBytes` e `previousMinimumBytes`, registrati senza scrivere
sulla flash. Se interviene di nuovo il watchdog, questi campi indicano se il
Bridge era nella rete, in una richiesta seriale (`serial-status`, `serial-caps`,
`serial-exec`, `serial-other`) o in un comando del runtime; per quest'ultimo
`LIST` permette di identificare `previousIndex`. Se diminuisce la memoria
libera potrebbero esserci
allocazioni trattenute; se diminuisce solo il blocco libero più grande mentre
la memoria libera totale resta quasi stabile, potrebbe esserci frammentazione.

Esempio:

```text
HELP
CLEAR
ADD BRIGHTNESS 255
ADD PIXELS 255 0 0
LIST
SAVE
RUN
```

`SAVE` conserva il testo completo della sequenza, nell’ordine originale,
inclusi ritardi, label, variabili e comandi cloud. Non salva automaticamente
il valore corrente delle variabili, a meno che quel valore sia rappresentato da
un comando `SET`.

Dopo un reset, se la microSD è disponibile e il controllo di integrità riesce,
il file viene rilevato, caricato ed eseguito automaticamente. In questo modo il
runtime riprende a funzionare dopo un'interruzione di corrente. `LOAD` resta
disponibile per riprovare o ricaricare manualmente il file, mentre `RUN` può
riavviare la sequenza dopo uno `STOP`.

## Limiti del runtime

| Risorsa | Limite |
| --- | ---: |
| Comandi nella sequenza | Limitati dalla dimensione del payload |
| Lunghezza di un comando | 96 caratteri |
| Payload cifrato | 4096 byte |
| Cicli `REPEAT` annidati | 8 |
| Variabili runtime | 48 |
| Lunghezza nome variabile | 16 caratteri |
| Lunghezza valore variabile | 64 caratteri |

## Variabili e sostituzione dei valori

Le variabili sono mantenute in RAM dall’interprete. I nomi vengono convertiti
in maiuscolo e possono contenere lettere, numeri e `_`.

```text
SET <nome> <valore>
GET <nome>
VARS
UNSET <nome>
CLEAR_VARS
```

Una variabile può essere usata nei comandi successivi con `$NOME` oppure
`${NOME}`:

```text
SET LIMIT 2200
ANALOG_READ A0 SOIL
SERIAL soil=$SOIL limit=$LIMIT
BRIGHTNESS $SOIL
```

La sostituzione avviene quando il comando viene eseguito, non quando viene
aggiunto con `ADD`. In questo modo il valore letto da un sensore può essere
riutilizzato nel resto della sequenza.

Per calcolare un intero a partire da variabili, il valore di `SET` può iniziare
con `=`. Sono supportati parentesi, numeri decimali o esadecimali e le
operazioni `+`, `-`, `*`, `/` e `%`; le variabili vengono sostituite prima del
calcolo. Senza `=`, `SET` continua a copiare il valore così com'è.

```text
SET KNOBRAW0 42
SET KNOBRAW1 1
SET KNOB_POSITION = $KNOBRAW0 + (256 * $KNOBRAW1)
GET KNOB_POSITION
```

Il terzo comando calcola `298`, combinando i byte little-endian in un intero.
Il risultato può essere usato da `IF_VAR`, da un comando di uscita o da un
ulteriore `SET`.

`BASE_CONVERT <valore> <baseDiPartenza> <baseDiArrivo> <variabile>` converte
interi tra basi da 2 a 36. Il risultato è salvato come testo nella variabile
indicata; sono accettati anche i prefissi `0b`, `0o` e `0x` per le basi 2, 8 e
16. Per esempio:

```text
BASE_CONVERT FF 16 10 DECIMALE
BASE_CONVERT $DECIMALE 10 2 BINARIO
```

Il primo comando salva `255` in `DECIMALE`; il secondo salva `11111111` in
`BINARIO`. Il segno meno è supportato.

`VARS` produce un output delimitato da `BEGIN_VARS` e `END_VARS`:

```text
BEGIN_VARS
VAR SOIL=1840
END_VARS
```

I valori correnti sono volatili. Per ricreare un valore iniziale dopo
`LOAD`/`RUN`, inserire un comando `SET` nella sequenza salvata.

## Matematica

`MATH <operazione> <operandi...> <variabile>` esegue calcoli decimali e salva
il risultato in una variabile runtime. Gli operandi possono essere numeri,
`PI`, `TAU`, `E` oppure riferimenti a variabili `$NOME`. Il separatore decimale
è il punto. Il nome della variabile di destinazione non viene sostituito,
quindi si può aggiornare la stessa variabile. I risultati usano fino a otto
cifre decimali; per valori molto grandi o piccoli viene usata la notazione
scientifica. È disponibile anche `EXEC MATH ...`.

| Operazione | Operandi prima della destinazione | Significato |
| --- | --- | --- |
| `PI`, `TAU`, `E` | nessuno | Salva la costante (τ = 2π). |
| `ADD`, `SUB`, `MUL`, `DIV`, `MOD`, `POW` | `a b` | Aritmetica; `MOD` è il resto fra numeri decimali. |
| `MIN`, `MAX`, `AVG`, `HYPOT` | `a b` | Minimo, massimo, media e lunghezza euclidea. |
| `ABS`, `NEG`, `SIGN` | `a` | Valore assoluto, opposto, segno (`-1`, `0`, `1`). |
| `SQRT`, `CBRT`, `EXP` | `a` | Radice quadrata, radice cubica, e elevato ad `a`. |
| `LN`, `LOG10`, `LOG2` | `a` | Logaritmo naturale, in base 10 o in base 2. |
| `FLOOR`, `CEIL`, `ROUND`, `TRUNC` | `a` | Arrotondamenti; `ROUND` allontana i mezzi da zero. |
| `ROUND_TO` | `a decimali` | Arrotonda a 0–8 cifre decimali. |
| `SIN`, `COS`, `TAN` | `angolo` | Trigonometria con ingresso in radianti. |
| `ASIN`, `ACOS`, `ATAN` | `a` | Trigonometria inversa con risultato in radianti. |
| `ATAN2` | `y x` | Angolo con riconoscimento del quadrante, in radianti. |
| `SIN_DEG`, `COS_DEG`, `TAN_DEG` | `angolo` | Trigonometria con ingresso in gradi. |
| `ASIN_DEG`, `ACOS_DEG`, `ATAN_DEG` | `a` | Trigonometria inversa con risultato in gradi. |
| `ATAN2_DEG` | `y x` | Angolo con riconoscimento del quadrante, in gradi (−180..180). |
| `TO_DEG`, `TO_RAD` | `angolo` | Conversione radianti→gradi o gradi→radianti. |
| `SINH`, `COSH`, `TANH` | `a` | Funzioni iperboliche. |
| `ASINH`, `ACOSH`, `ATANH` | `a` | Funzioni iperboliche inverse. |
| `CLAMP` | `valore minimo massimo` | Limita il valore all'intervallo incluso. |
| `LERP` | `inizio fine frazione` | Interpolazione lineare (anche estrapolazione). |
| `NORM3` | `x y z` | Lunghezza di un vettore a tre componenti. |
| `MAP` | `valore inMin inMax outMin outMax` | Rimappatura lineare; non limita il risultato. |

Per esempio, per calcolare un angolo e inviarlo a un widget cloud:

```text
MATH PI CIRCLE_PI
MATH SQRT 9 ROOT
MATH HYPOT $AY $AZ YZ_LENGTH
MATH ATAN2_DEG $AX $YZ_LENGTH PITCH_DEG
SERIAL pitch=$PITCH_DEG
WIDGET WRITE LEVEL pitch $PITCH_DEG
```

`$AX`, `$AY` e `$AZ` rappresentano campioni di un accelerometro già salvati
in variabili; la formula corretta per gli assi dipende dall'orientamento del
sensore. Divisione o `MOD` per zero, radice quadrata di un numero negativo,
logaritmo di un numero non positivo, argomenti non validi per le funzioni
trigonometriche inverse e risultati non finiti producono `ERR MATH ...`.
Come per gli altri comandi runtime non validi, la sequenza `RUN` si ferma.
Per dati normalizzati rumorosi, usare `CLAMP` prima di `ASIN` o `ACOS`.
Le operazioni disponibili e i loro argomenti sono esposti anche nelle righe
`CAPS MATH`, utilizzabili dal Dev Studio.

## Ingressi e uscite digitali/analogici

I pin possono essere indicati con il numero Arduino oppure con etichette come
`A0`, `A1`, `D2` e `D3`. Sulla Nano ESP32 vengono usati gli alias del pinout
Arduino Nano.

```text
DIGITAL_READ <pin> <variabile>
DIGITAL_WRITE <pin> <valore>
ANALOG_READ <pin> <variabile>
ANALOG_WRITE <pin> <0..255>
```

`DIGITAL_READ` configura il pin come `INPUT` e salva `0` o `1`. I valori per
`DIGITAL_WRITE` possono essere `HIGH`, `LOW`, `ON`, `OFF`, `TRUE`, `FALSE` o un
numero. `ANALOG_READ` salva il valore ADC grezzo. `ANALOG_WRITE` usa l’intervallo
PWM 0–255 sui pin compatibili.

Esempio:

```text
EXEC DIGITAL_READ D4 BUTTON_STATE
EXEC GET BUTTON_STATE
EXEC DIGITAL_WRITE D13 HIGH
EXEC ANALOG_READ A0 SOIL
EXEC SERIAL soil=$SOIL
```

## Blocchi generici per il bus I²C

Il bridge espone il bus Arduino `Wire` tramite comandi runtime orientati ai
byte. Le primitive sono indipendenti dal sensore specifico: l'editor web può
quindi costruire un blocco dedicato a un sensore generando un singolo comando
`READ_REG` o `TRANSFER`. Il bus usa i pin SDA/SCL predefiniti dalla scheda; il
protocollo non codifica numeri GPIO fissi.

```text
I2C BEGIN [frequency]
I2C SCAN
I2C PING <address>
I2C WRITE <address> <byte0> [byte1 ... byte31]
I2C READ <address> <length> [prefix]
I2C TRANSFER <address> <writeCount> <readCount> <writeByte...> [prefix]
I2C READ_REG <address> <register> <length> [prefix]
I2C WRITE_REG <address> <register> <value>
```

Gli indirizzi e i byte possono essere scritti in decimale o in esadecimale,
per esempio `0x76`. Gli indirizzi I²C a 7 bit validi sono `0x03`–`0x77` e una
transazione può contenere al massimo 32 byte. `I2C BEGIN` è opzionale perché
`MyDot::begin()` avvia già il bus predefinito, ma permette di scegliere una
frequenza da 10 kHz a 1 MHz. Per i sensori si usa normalmente 100 kHz; per i
dispositivi compatibili con il fast mode si può usare 400000.

`SCAN` stampa tutti gli indirizzi che rispondono. `PING` verifica un singolo
indirizzo. `WRITE` invia una sequenza di byte. `READ` legge byte senza una fase
di selezione del registro. `TRANSFER` esegue una transazione combinata di
scrittura e lettura con repeated start, il formato tipico dei sensori a
registri. I conteggi rendono il comando non ambiguo per un blocco grafico:

```text
I2C TRANSFER <address> <numero-byte-da-scrivere> <numero-da-leggere> <byte-di-scrittura...> [prefix]
```

`READ_REG` è una scorciatoia per il caso più comune: scrive un byte di
registro e legge il numero di byte richiesto. `WRITE_REG` scrive un byte di
registro seguito da un byte di valore.

Se viene indicato `prefix`, ogni byte ricevuto viene salvato in una variabile
runtime chiamata `<prefix>0`, `<prefix>1` e così via. I blocchi successivi
possono quindi usare il risultato, ad esempio `$WHO0`, nel display, nel cloud,
in una condizione o in un log sulla SD. Con i log dettagliati attivi (oppure
con `EXEC`), i byte vengono anche stampati in esadecimale con una riga
`I2C DATA`.

Le operazioni I²C hanno un limite di tempo: sulla Nano ESP32 il bridge imposta
un timeout di 50 ms per transazione e riprova fino a due volte una lettura
incompleta. Se il dispositivo viene scollegato temporaneamente o mantiene il
bus occupato, il bridge reinizializza `Wire`, segnala `WARN I2C`/`ERR I2C` e
prosegue il runtime invece di terminare la sequenza salvata. Le variabili
dell'ultima lettura completa restano disponibili, così un blocco successivo
può decidere come gestire il campione mancante.

Esempio:

```text
EXEC I2C BEGIN 400000
EXEC I2C SCAN
EXEC I2C READ_REG 0x76 0xD0 1 WHO
EXEC SERIAL sensor-id=$WHO0
```

Un blocco dedicato a un sensore può quindi contenere indirizzo, registro,
ordine dei byte e formula di conversione, usando questo comando generico come
unico trasporto hardware. SDA e SCL devono avere i pull-up corretti secondo il
modulo del sensore; non collegare mai un dispositivo I²C a una tensione logica
superiore a quella supportata dalla scheda.

## Controllo del flusso

I comandi di controllo del flusso funzionano dentro una sequenza avviata con
`RUN`. `EXEC` li rifiuta perché non dispone del cursore e dello stack dei cicli.

Il Dev Studio può compilare più catene scollegate come attività cooperative.
Ogni attività conserva il proprio cursore, timer e stato di attesa: `DELAY` e
`WAIT_BUTTON` sospendono solo la catena in cui si trovano, mentre il bridge
continua a servire le altre. Una catena collegata resta un'unica attività; i
rami vero e falso di `IF_VAR` o `IF_BUTTON` restano alternativi e non partono
in parallelo. I marker `TASK BEGIN ONCE|LOOP` e `TASK END` sono generati dal
Dev Studio e non sono blocchi della palette. I programmi salvati senza marker
mantengono la precedente esecuzione lineare.
Un'attività può saltare a un flusso manuale condiviso fuori dalle sezioni
`TASK`. Quando il flusso torna all'etichetta del ciclo continuo generata dal
compilatore, l'attività chiamante `ONCE` termina oppure quella `LOOP` riparte;
il ciclo di destinazione è già in esecuzione autonomamente. Non è valido
saltare nel corpo di un'altra attività.

### Label e salti

```text
LABEL <nome>
GOTO <nome>
IF_BUTTON A|B PRESSED|CLICKED GOTO <nome>
IF_VAR <nome> <operatore> <valore> GOTO <nome>
```

Le label non distinguono maiuscole e minuscole. Un salto che esce da un ciclo
annidato rimuove automaticamente i relativi frame.

`IF_BUTTON` e `IF_VAR` sono controlli istantanei: quando la condizione è falsa
il runtime prosegue con la riga successiva. Per attendere realmente la
pressione usare `WAIT_BUTTON`. Nel grafo del Dev Studio il ramo senza
continuazione viene chiuso automaticamente, così non può cadere per errore
nel blocco della label.

`IF_VAR` supporta i confronti numerici `==`, `=`, `!=`, `>`, `>=`, `<` e `<=`.
Per valori testuali sono disponibili `==`, `=` e `!=`.

```text
ADD ANALOG_READ A0 SOIL
ADD IF_VAR SOIL > 2200 GOTO DRY
ADD BRIGHTNESS 32
ADD GOTO END
ADD LABEL DRY
ADD BRIGHTNESS 255
ADD LABEL END
```

### Attesa di un pulsante

```text
WAIT_BUTTON A|B PRESSED|CLICKED [timeoutMs]
```

L’attesa non blocca seriale, Wi-Fi o cloud. Senza timeout l’attesa è
indefinita. Se scade il timeout, la sequenza continua e viene stampato
`INFO WAIT_BUTTON timeout`.

Quando l’attesa viene attivata il bridge stampa `INFO WAIT_BUTTON waiting`;
quando rileva il click stampa `INFO WAIT_BUTTON triggered`. L’edge del pulsante
è memorizzato dal loop del bridge, quindi un click breve non viene perso mentre
sono in corso attività lente.

### Cicli

```text
REPEAT <conteggio-positivo>
  ...comandi...
ENDREPEAT
```

`BREAK` esce dal ciclo più interno. `CONTINUE` raggiunge `ENDREPEAT`, aggiorna
il contatore e avvia l’iterazione successiva.

```text
ADD REPEAT 3
ADD PIXELS_RANDOM
ADD DELAY 500
ADD ENDREPEAT
```

## Relay e NeoPixel

```text
RELAY ON
RELAY OFF
RELAY TOGGLE
RELAY STATUS <variabile>

BRIGHTNESS <0..255>
PIXELS <red> <green> <blue>
PIXELS_SHOW
PIXELS_RANDOM
PIXEL <index> <red> <green> <blue>
CLEAR_PIXELS
```

`RELAY STATUS` legge l’uscita attuale senza modificarla e salva il testo `ON`
oppure `OFF` nella variabile runtime scelta.

Esempio:

```text
ADD RELAY STATUS STATO_RELE
ADD SERIAL relay=$STATO_RELE
```

`PIXELS` e `PIXEL` inviano subito i dati ai LED. `PIXELS_SHOW` permette di
richiedere esplicitamente l’invio del buffer corrente. Gli indici dei pixel
vanno da `0` a `NUMPIXELS - 1`.

## Ventola e DRV8830

```text
FAN <-100..100>
FAN STOP
FAN STATUS
FAN CLEAR_FAULT
```

I valori positivi e negativi selezionano la direzione. `STATUS` stampa la
velocità richiesta e il registro di fault DRV8830 in esadecimale.
`CLEAR_FAULT` invia il comando di cancellazione, ma non risolve un guasto
hardware ancora presente.

## Display OLED

Il display può essere assente senza causare errori. La forma `DISPLAY <testo>`
è una scorciatoia; le forme strutturate espongono i singoli metodi:

```text
DISPLAY <testo>
DISPLAY PRESENT
DISPLAY LOGO
DISPLAY SENSOR
DISPLAY CLEAR
DISPLAY SHOW
DISPLAY RAW_SHOW
DISPLAY CURSOR <x> <y>
DISPLAY SIZE <1..4>
DISPLAY COLOR <0..65535>
DISPLAY TEXT <testo>
DISPLAY TEXT_AT <x> <y> <size> <testo>
DISPLAY PRINT <testo>
DISPLAY PRINTLN <testo>
DISPLAY NEWLINE
```

Le operazioni di scrittura restituiscono `OK DISPLAY TEXT` (o l’operazione
corrispondente). Se il display OLED non è stato rilevato viene inoltre stampato
`WARN DISPLAY unavailable`: la sequenza non si interrompe, ma occorre verificare
alimentazione, cablaggio I2C e indirizzo `0x3C`.

`RAW_SHOW` richiama direttamente l’oggetto `Adafruit_SSD1306` restituito da
`getDisplay()`.

## Sensore BME690 e log su SD

```text
SENSORS
SENSOR_READ TEMPERATURE|PRESSURE|HUMIDITY|GAS <variabile>
SENSORS_LOG <percorso>
```

`SENSORS` legge il BME690 e stampa temperatura, pressione, umidità e gas
resistance sulla seriale. `SENSOR_READ` salva un singolo valore in una
variabile, riutilizzabile sul display, nella seriale, nel cloud o su file:

```text
EXEC SENSOR_READ TEMPERATURE TEMP
EXEC SENSOR_READ HUMIDITY HUM
EXEC DISPLAY TEXT T=$TEMP C  H=$HUM %
EXEC SD APPEND_LINE /custom.csv $TEMP,$HUM
```

`SENSORS_LOG` è una scorciatoia che crea, se necessario, un CSV e aggiunge una
riga con le colonne `epoch`, `time`, `temperature_c`, `pressure_hpa`,
`humidity_percent` e `gas_kohm`.

Logger a intervallo di un minuto:

```text
CLEAR
ADD LABEL LOG
ADD SENSORS_LOG /bme690.csv
ADD DELAY 60000
ADD GOTO LOG
SAVE
RUN
```

## Pulsanti

```text
BUTTON STATUS
BUTTON A PRESSED
BUTTON A CLICKED
BUTTON B PRESSED
BUTTON B CLICKED
```

L’editor può anche inviare la forma strutturata `BUTTON READ A PRESSED` (o
`CLICKED`/`B`); è equivalente alla forma breve.

`PRESSED` restituisce il livello corrente. `CLICKED` restituisce un fronte
debounced e viene consumato dalla libreria quando viene usato in una
condizione; `BUTTON STATUS` lo visualizza senza consumarlo. Per controllare
una sequenza usare `IF_BUTTON` o `WAIT_BUTTON`.

## Wi-Fi e orologio

```text
WIFI BEGIN <ssid> <password>
WIFI STATUS
WIFI RSSI

TIME EPOCH
TIME FORMAT [gmtOffset]
```

Dopo `WIFI BEGIN`, il bridge chiama continuamente `MyDot::run()` per mantenere
attive le riconnessioni Wi-Fi e MQTT. Questo protocollo separato da spazi non
supporta SSID o password contenenti spazi.

## Cloud Microeden

```text
CLOUD BEGIN <deviceId> <token>
CLOUD SEND
CLOUD STATUS
CLOUD BUFFER <128..65535>
CLOUD SYNC <intervalMs>
CLOUD WRITE_TEXT <key> <value>
CLOUD WRITE_INT <key> <integer>
CLOUD WRITE_FLOAT <key> <number>
CLOUD WRITE_DOUBLE <key> <number>
CLOUD WRITE_BOOL <key> TRUE|FALSE
CLOUD ON_COMMAND <expected> [key]
CLOUD READ <key>
CLOUD READ_ONCE <key>
```

Nel flusso continuo il Bridge può aggiungere `GOTO <label>` alla forma
`CLOUD ON_COMMAND <expected>` per sondare più comandi senza bloccare gli altri
ascoltatori. Senza `GOTO` resta l'attesa cooperativa tradizionale.

`READ` restituisce lo stato cloud memorizzato. `READ_ONCE` consuma un nuovo
valore widget ricevuto e restituisce `CLOUD VALUE_ONCE empty` quando non ce ne
sono. `ON_COMMAND` usa la chiave predefinita `content`, consuma un comando
entrante quando corrisponde al testo atteso e, durante un `RUN`, resta in attesa
in modo cooperativo fino alla ricezione del comando. L'argomento `key` resta
accettato per compatibilità, ma il runtime del Bridge supporta `content`.

`CLOUD SYNC` installa il callback di telemetria del bridge, che pubblica il
nome del bridge e il numero corrente di comandi alla frequenza richiesta.

Se MQTT è temporaneamente indisponibile, `CLOUD SEND` conserva in memoria i
valori in attesa e il runtime salvato prosegue. Un invio successivo può
pubblicarli dopo la riconnessione.

## Widget cloud

```text
WIDGET READ <type> <key>
WIDGET WRITE <type> <key> <value...>
```

`WIDGET WRITE` pubblica subito il valore. Se MQTT è disconnesso, il valore
rimane in attesa di un successivo `WIDGET WRITE` o `CLOUD SEND` e il runtime
prosegue. Un eventuale `CLOUD SEND` dopo un invio riuscito è innocuo.

| Tipo | Lettura | Scrittura |
| --- | --- | --- |
| `RAW` / `TEXT` | Testo memorizzato | Testo arbitrario |
| `COLOR` | Tripla RGB | `red green blue` |
| `MAP` | Coordinate memorizzate | `latitude longitude` oppure `TEXT <value>` |
| `LEVEL` | Intero | Valore numerico, anche decimale |
| `SLIDER` | Intero appena ricevuto (`-1` se vuoto) | Valore numerico, anche decimale |
| `SWITCH` | Booleano | `TRUE`/`FALSE` |
| `PUSHBUTTON` | Booleano | `TRUE`/`FALSE` |
| `LED` | Booleano | `TRUE`/`FALSE` |
| `PHOTO` | Testo memorizzato | Testo |

Esempi:

```text
EXEC WIDGET WRITE COLOR light 255 0 0
EXEC WIDGET WRITE SLIDER brightness 128
EXEC WIDGET WRITE LEVEL temperature 21.95
EXEC WIDGET READ SLIDER brightness
EXEC WIDGET READ MAP location
```

Le scritture `LEVEL` e `SLIDER` accettano sia interi sia numeri decimali. Il
bridge li pubblica come numeri JSON, quindi il valore prodotto da `SENSOR_READ`
(per esempio `21.95`) può essere inviato direttamente al widget:

```text
ADD SENSOR_READ TEMPERATURE TEMP
ADD WIDGET WRITE LEVEL temperature $TEMP
SAVE
RUN
```

Gli helper tipizzati `Level` e `Slider` continuano a esporre valori interi
quando leggono gli input della dashboard; il supporto ai decimali riguarda i
valori scritti dai blocchi runtime.

## Conversione colori

```text
COLOR HEX_TO_RGB #00AAFF
COLOR RGB_TO_HEX 0 170 255
```

Il primo comando stampa `COLOR RGB r g b`, il secondo stampa
`COLOR HEX #RRGGBB`.

## Scheda SD e file di testo

La SD viene inizializzata in `setup()`. `SD BEGIN` permette di riprovare
l’inizializzazione.

```text
SD BEGIN
SD EXISTS <path>
SD MKDIR <path>
SD TOUCH <path>
SD REMOVE <path>
SD WRITE <path> <text>
SD WRITE_LINE <path> <text>
SD APPEND <path> <text>
SD APPEND_LINE <path> <text>
SD OPEN <path> [r|w|a]
SD READ <path>
SD READ_B64 <path>
SD LIST [path]
```

`WRITE` sostituisce il file, mentre `APPEND` aggiunge testo. Le varianti
`WRITE_LINE` e `APPEND_LINE` aggiungono automaticamente un newline. `READ`
stampa il contenuto tra `BEGIN_SD_READ` e `END_SD_READ`.

`MKDIR` crea una nuova cartella e `TOUCH` crea un file vuoto; entrambe le
operazioni restituiscono errore se il percorso esiste già.

`LIST` elenca i file della directory indicata; senza percorso usa la root:

```text
EXEC SD WRITE_LINE /message.txt Ciao da MyDot
EXEC SD APPEND_LINE /message.txt Seconda riga
EXEC SD READ /message.txt
EXEC SD LIST /
```

L’elenco è delimitato da `BEGIN_SD_LIST` e `END_SD_LIST`. Ogni elemento viene
stampato come `FILE <nome> FILE size=<byte>` oppure `FILE <nome> DIR`.

`READ_B64` è la variante sicura per il Dev Studio: restituisce qualsiasi file,
anche binario, codificato Base64 tra `BEGIN_SD_READ_B64` e
`END_SD_READ_B64`.

Il file runtime è sempre `/MyDot.run`. Contiene header versionato,
payload dei comandi cifrato XTEA-CTR e checksum CRC32. La chiave è compilata
nel firmware: la cifratura protegge i dati sulla SD, ma non sostituisce un
secure key store. Le credenziali vengono inserite nel runtime solo se si
aggiungono esplicitamente comandi come `ADD WIFI BEGIN ...` o `ADD CLOUD BEGIN
...`; i comandi `EXEC` non vengono registrati.

## Esempio completo

```text
CLEAR
ADD SET LIMIT 2200
ADD LABEL LOOP
ADD SENSOR_READ TEMPERATURE TEMP
ADD ANALOG_READ A0 SOIL
ADD SERIAL temp=$TEMP soil=$SOIL
ADD IF_VAR SOIL > $LIMIT GOTO DRY
ADD DISPLAY TEXT T=$TEMP C
ADD GOTO WAIT
ADD LABEL DRY
ADD DISPLAY TEXT Soil dry
ADD LABEL WAIT
ADD SD APPEND_LINE /plant.csv $TEMP,$SOIL
ADD WAIT_BUTTON B CLICKED 60000
ADD GOTO LOOP
SAVE
RUN
```

Usare `STOP` per terminare la sequenza e spegnere le uscite.
