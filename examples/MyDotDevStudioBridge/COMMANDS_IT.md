# MyDot Dev Studio Bridge — Riferimento dei comandi seriali

Questa è la documentazione completa dell’esempio
`MyDotDevStudioBridge.ino`.

Il bridge legge una riga alla volta dalla `Serial` a **115200 baud**. Ogni
comando deve terminare con un carattere newline. I comandi e i sottocomandi
non distinguono maiuscole e minuscole; i valori testuali, come SSID Wi-Fi e
token cloud, mantengono invece il loro contenuto originale.

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

All’avvio viene stampato `MyDot Dev Studio Bridge ready`. Viene inoltre
indicato se la scheda microSD è stata inizializzata.

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
| `ADD <runtime-command>` | Aggiunge un comando alla sequenza in memoria. |
| `LIST` | Elenca la sequenza con indici a partire da zero. |
| `CLEAR` | Conferma subito la cancellazione della sequenza in memoria, poi ferma l’esecuzione e porta relay, ventola e NeoPixel allo stato sicuro. Non cancella il file SD. |
| `SAVE` | Cifra e salva la sequenza in `/MyDot.run`. |
| `LOAD` | Legge, decifra, verifica e carica il file runtime. |
| `RUN` | Avvia la sequenza dal comando zero senza bloccare il loop. |
| `STOP` | Ferma la sequenza, disattiva il watchdog runtime, spegne relay, ventola e NeoPixel e pulisce il display OLED. |
| `REBOOT` | Arresta in sicurezza il runtime e riavvia la scheda MyDot usando il reset nativo del core Arduino selezionato. |
| `EXEC <runtime-command>` | Esegue immediatamente un singolo comando. |

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
della scheda rilevato dal core Arduino), `wifi`,
`wifiRssi`, `cloud`, `sdCardBytes`, `sdTotalBytes`, `sdUsedBytes`,
`sdFreeBytes`, `ramFreeBytes` e `ramTotalBytes`. `sdCardBytes` è la capacità
fisica della scheda (`cardSize()`); `sdTotalBytes` è la capacità utilizzabile
del filesystem. Le dimensioni vengono restituite quando la scheda espone
l’API corrispondente; altrimenti il valore è `unknown`.

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
| Variabili runtime | 16 |
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

`VARS` produce un output delimitato da `BEGIN_VARS` e `END_VARS`:

```text
BEGIN_VARS
VAR SOIL=1840
END_VARS
```

I valori correnti sono volatili. Per ricreare un valore iniziale dopo
`LOAD`/`RUN`, inserire un comando `SET` nella sequenza salvata.

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
in una condizione o in un log sulla SD. I byte vengono anche stampati in
esadecimale con una riga `I2C DATA`.

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

## Widget cloud

```text
WIDGET READ <type> <key>
WIDGET WRITE <type> <key> <value...>
```

| Tipo | Lettura | Scrittura |
| --- | --- | --- |
| `RAW` / `TEXT` | Testo memorizzato | Testo arbitrario |
| `COLOR` | Tripla RGB | `red green blue` |
| `MAP` | Coordinate memorizzate | `latitude longitude` oppure `TEXT <value>` |
| `LEVEL` | Intero | Intero |
| `SLIDER` | Intero appena ricevuto (`-1` se vuoto) | Intero |
| `SWITCH` | Booleano | `TRUE`/`FALSE` |
| `PUSHBUTTON` | Booleano | `TRUE`/`FALSE` |
| `LED` | Booleano | `TRUE`/`FALSE` |
| `PHOTO` | Testo memorizzato | Testo |

Esempi:

```text
EXEC WIDGET WRITE COLOR light 255 0 0
EXEC WIDGET WRITE SLIDER brightness 128
EXEC WIDGET READ SLIDER brightness
EXEC WIDGET READ MAP location
```

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
