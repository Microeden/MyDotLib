# Documentazione della libreria MyDot

Questa è la versione italiana di `MyDot_Documentation.md`. I nomi delle API,
dei comandi, dei file e gli esempi di codice restano invariati per poterli
copiare direttamente negli sketch Arduino.

Versione inglese: [MyDot_Documentation.md](MyDot_Documentation.md).

## Panoramica

MyDot è una libreria Arduino per la carrier Microeden MyDot e per la piattaforma
cloud Microeden. Fornisce un'unica API per le periferiche della scheda e per la
comunicazione opzionale Wi-Fi/MQTT:

- due pulsanti;
- un'uscita relè;
- dodici LED NeoPixel;
- il display OLED integrato;
- il sensore ambientale BME690 integrato, tramite l'API compatibile Adafruit BME680;
- l'interfaccia per la scheda SD;
- il driver per ventola/motore DRV8830;
- Wi-Fi, orario NTP e servizi cloud Microeden.

L'API pubblica è dichiarata in `src/MyDot.h`; l'implementazione si trova in
`src/MyDot.cpp`.

## Requisiti hardware e alimentazione

La carrier deve essere alimentata tramite il jack DC esterno quando si usano il
relè, i NeoPixel, il driver della ventola o altre uscite di potenza. La USB
alimenta la Arduino Nano ESP32, ma non fornisce la potenza necessaria agli
stadi di uscita della carrier.

Il sensore ambientale integrato è un BME690. La libreria usa l'interfaccia
compatibile Adafruit BME680 per inizializzazione e letture.

Le architetture dichiarate sono ESP32, SAMD, RP2040 e le Nano Wi-Fi basate su
Mbed. Tutte le Arduino Nano supportate usano lo stesso connettore Nano fisico
sulla carrier MyDot; le definizioni in `MyDot.h` adattano gli alias al core
selezionato, senza descrivere cablaggi diversi.

Per una Arduino Nano ESP32 seleziona **Arduino Nano ESP32** nell'Arduino IDE.
La libreria supporta sia la numerazione Arduino sia l'impostazione **By GPIO
number (legacy)**. MyDot converte l'alias `D3` della Nano ESP32 nel GPIO ESP32
fisico prima di inizializzare il driver NeoPixel.

## Installazione

Installa **MyDot** dal Library Manager dell'Arduino IDE oppure copia questo
repository nella cartella delle librerie Arduino.

Dipendenze dichiarate:

- Adafruit NeoPixel;
- Adafruit SSD1306;
- Adafruit BME680 Library;
- PubSubClient;
- ArduinoJson;
- SD;
- WiFiNINA.

Procedura tipica nell'Arduino IDE:

1. apri **Sketch > Include Library > Manage Libraries**;
2. cerca `MyDot`;
3. installa la libreria e le dipendenze;
4. apri un esempio da **File > Examples > MyDot**.

## Sketch minimo

```cpp
#include <MyDot.h>

MyDot dot;

void setup() {
  dot.begin();
}

void loop() {
  dot.run();
}
```

`begin()` inizializza le periferiche. Quando usi Wi-Fi o cloud, richiama
`run()` ripetutamente da `loop()`: mantiene le connessioni Wi-Fi/MQTT ed esegue
il callback di sincronizzazione cloud configurato.

## Credenziali e TLS

Gli esempi cloud includono un file locale `microeden_secrets.h` con segnaposto
da modificare localmente:

```cpp
#define SECRET_WIFI_SSID "your-wifi-name"
#define SECRET_WIFI_PASSWORD "your-wifi-password"
#define SECRET_DEVICE_ID "your-device-id"
#define SECRET_DEVICE_TOKEN "your-device-token"
```

Non inserire mai nel repository password Wi-Fi, token dei dispositivi o altre
credenziali private.

Su ESP32 `beginCloud()` configura il certificato ISRG Root X1 incluso e usa la
verifica del certificato invece di TLS insicuro. Chiama `beginWiFi()` prima di
`beginCloud()` e richiama continuamente `run()` da `loop()`.

Ordine di connessione:

```cpp
#include <MyDot.h>
#include "microeden_secrets.h"

MyDot dot;

void setup() {
  Serial.begin(115200);
  dot.begin();
  dot.beginWiFi(SECRET_WIFI_SSID, SECRET_WIFI_PASSWORD);
  dot.beginCloud(SECRET_DEVICE_ID, SECRET_DEVICE_TOKEN);
}

void loop() {
  dot.run();
}
```

`beginWiFi()` avvia la connessione in modo asincrono e non blocca il setup in
attesa dell'access point. Se l'access point non è disponibile, `run()` ritenta
in background. Non chiamare `beginWiFi()` ripetutamente da `loop()`.

#### `void stopNetworkServices()`

Disconnette il client MQTT e la stazione Wi-Fi, elimina i messaggi cloud in
attesa e il callback di sincronizzazione, quindi impedisce a `run()` di
riconnettere automaticamente i servizi. Usalo al termine di una sessione di
programmazione interattiva. Non cancella le credenziali memorizzate: una
successiva coppia `beginWiFi()`/`beginCloud()` può riattivare i servizi.

## Mappa dei pin della carrier

La libreria usa questi alias sulla carrier MyDot per tutte le Arduino Nano
supportate:

| Funzione | Alias del connettore Nano |
| --- | --- |
| Pulsante A | `A7` |
| Pulsante B | `D4` |
| Relè | `D2` |
| NeoPixel | `D3` |
| Chip select SD | `D10` |

Driver ventola, OLED e BME690 usano il bus I2C della carrier. Gli indirizzi
predefiniti sono costanti in `MyDot.h`:

| Dispositivo | Costante | Indirizzo |
| --- | --- | --- |
| Driver ventola DRV8830 | `FAN_ADDRESS` | `0x68` |
| OLED | `SCREEN_ADDRESS` | `0x3C` |
| BME690 | `BME_ADDRESS` | `0x77` |

## Esempi

Ogni esempio è autonomo e include un segnaposto `microeden_secrets.h`. Gli
esempi diagnostici usano il Monitor Seriale a **115200 baud**. `Display` è
l'esempio dedicato all'uscita OLED; gli altri usano la seriale per la diagnosi.

| Esempio | Dimostra |
| --- | --- |
| `Basic` | Inizializzazione e ciclo principale |
| `Buttons` | Stato premuto e click dei pulsanti |
| `Relay` | Controllo e commutazione del relè |
| `Pixels` | Colori dei pixel, luminosità e cancellazione |
| `Display` | Logo e testo OLED |
| `Sensors` | Temperatura, pressione, umidità e resistenza del gas BME690 |
| `Fan` | Velocità, arresto e diagnostica DRV8830 |
| `SDCard` | Inizializzazione e operazioni sui file SD |
| `WiFiStatus` | Connessione Wi-Fi e RSSI |
| `Time` | Orario NTP e formattazione |
| `CloudPublish` | Connessione cloud e pubblicazione di valori |
| `CloudWidgets` | Wrapper per i widget cloud |
| `CloudCommands` | Lettura e consumo dei comandi cloud |
| `ColorConversion` | Conversione RGB/esadecimale |
| `HardwareCheck` | Test automatici di relè, NeoPixel e pin |
| `MyDotVase` | Telemetria analogica, pompa, modalità LED e luminosità cloud |
| `MyDotDevStudioBridge` | Bridge seriale, API completa, sequenze SD cifrate ed esecuzione non bloccante |

`MyDotVase` legge il sensore analogico da `A0` e pubblica `soilRaw`,
`soilPercent`, `pumpOn`, `pumpActivations`, `lightsOn`, `lightMode`,
`lightModeIndex`, `brightness` ed `event`. Usa `Slider`, `Switch` e `Level` per
le chiavi `brightness`, `pumpOn`, `lightsOn` e `lightModeIndex`. I comandi
della pompa sono `on`, `off` e `pump`; ogni comando esplicito pubblica lo stato
aggiornato di `pumpOn`. I comandi delle luci sono `lights_on`, `lights_off`,
`lights_warm`, `lights_cool`, `grow_veg`, `grow_bloom` e `grow_full`.

È possibile selezionare anche una modalità numerica con
`{"content":"lights_mode", "mode":2}`: bianco freddo (`0`), bianco caldo
(`1`), crescita vegetativa (`2`), fioritura (`3`) e spettro completo (`4`).
Lo slider cloud deve usare la chiave `brightness` e invia il formato compatto
`key_value`, per esempio `{"content":"brightness_128"}`. Le variazioni dello
slider sono applicate subito; pubblicazione cloud e salvataggio persistente
attendono 300 ms per permettere al valore di stabilizzarsi. Il pulsante A
commuta le luci e B cicla le cinque modalità.

I colori grow sono approssimazioni NeoPixel e non spettri calibrati. Regola
`SOIL_RAW_DRY` e `SOIL_RAW_WET` dopo aver misurato il sensore in terreno asciutto
e bagnato. L'esempio presume che la pompa sia pilotata dal relè MyDot e che la
carrier sia alimentata dal jack DC esterno.

**Nota di sicurezza:** alimenta il sensore di umidità AZ-Delivery a **3,3 V**,
non a 5 V. Un'uscita analogica a 5 V può superare l'intervallo d'ingresso e
danneggiare la scheda.

`MyDotVaseState.h` conserva lo stato delle luci dopo i reset: usa Preferences
ESP32 per Nano ESP32, Mbed KVStore per Nano RP2040 Connect e EEPROM per i core
RP2040 che la forniscono e per Nano 33 IoT. Vengono salvati accensione,
modalità e luminosità.

## Ciclo di vita comune

La maggior parte degli sketch segue questo schema:

```cpp
void setup() {
  Serial.begin(115200);
  dot.begin();
}

void loop() {
  dot.run();
}
```

`run()` è sicuro anche negli sketch senza cloud. È particolarmente importante
negli sketch cloud perché gestisce keep-alive MQTT, riconnessioni, comandi in
ingresso e sincronizzazione programmata.

## Costanti pubbliche

Le seguenti costanti sono disponibili da `MyDot.h`:

| Costante | Valore | Scopo |
| --- | ---: | --- |
| `FAN_ADDRESS` | `0x68` | Indirizzo I2C DRV8830 |
| `SCREEN_ADDRESS` | `0x3C` | Indirizzo OLED SSD1306 |
| `BME_ADDRESS` | `0x77` | Indirizzo BME690 |
| `SCREEN_WIDTH` | `128` | Larghezza OLED in pixel |
| `SCREEN_HEIGHT` | `64` | Altezza OLED in pixel |
| `OLED_RESET` | `-1` | Configurazione reset OLED |
| `NUMPIXELS` | `12` | Numero di NeoPixel sulla carrier |

I macro `BUTTON_A`, `BUTTON_B`, `RELAY`, `PIN` e `SD_CS` vengono selezionati
dal core in fase di compilazione solo per rispettare la numerazione della
scheda; il cablaggio della carrier resta quello standard Nano.

### Capacità della piattaforma

La libreria espone una matrice di capacità sicura tramite i flag `MYDOT_HAS_*`
e la risposta `CAPS` del Bridge:

- `MYDOT_HAS_WIFI`: trasporto Wi-Fi disponibile;
- `MYDOT_HAS_CLOUD`: trasporto My Microeden/MQTT disponibile;
- `MYDOT_HAS_RAM_STATUS`: disponibile un adapter per lo stato della RAM;
- `MYDOT_HAS_SD_CAPACITY`: disponibile la lettura di capacità e spazio libero SD.

La RAM viene rilevata tramite adapter distinti per ESP32, SAMD, RP2040 e Mbed.
Se il core non espone un'API adatta, il valore diventa `unknown` senza
inventare numeri e senza impedire la compilazione. Il Bridge filtra i comandi
di rete, cloud e storage nella risposta `CAPS` in base a questi flag.

## Riferimento API

### Metodi principali

#### `MyDot()`

Costruisce il controller MyDot senza inizializzare l'hardware. Chiama
`begin()` da `setup()`.

#### `void begin()`

Inizializza:

- generatore casuale;
- pulsanti con `INPUT_PULLUP`;
- uscita relè inizialmente disattiva;
- NeoPixel;
- I2C;
- DRV8830, OLED e BME690 quando rilevati.

Se presente, sul display viene disegnato il logo e i NeoPixel vengono spenti.

#### `void run()`

Esegue i servizi della libreria. Quando il cloud è configurato gestisce
riconnessione Wi-Fi, riconnessione MQTT, traffico MQTT e callback periodico.
Chiamalo il più spesso possibile da `loop()` ed evita ritardi bloccanti.

#### `bool reboot()` e `bool rebootSupported() const`

`reboot()` richiede un reset software nativo su ESP32, SAMD, Mbed e RP2040
quando il core Arduino espone una primitiva sicura. Restituisce `false` solo
su un core non supportato. Dopo un reset riuscito il metodo non ritorna;
`rebootSupported()` permette di nascondere il comando quando non disponibile.

#### API watchdog cooperativo

```cpp
dot.watchdogBegin(10000);  // timeout in millisecondi
dot.watchdogFeed();        // da richiamare nel loop funzionante
dot.watchdogStop();
```

Il watchdog è portabile e cooperativo: `run()` controlla la scadenza e chiama
`reboot()` quando l'applicazione non lo alimenta entro il tempo previsto. Non
viene eseguito alcun feed automatico, così un loop bloccato può essere rilevato.
`watchdogBegin()` rifiuta timeout inferiori a 100 ms;
`watchdogIsEnabled()` restituisce lo stato attuale.

### Ventola e driver motore DRV8830

Il DRV8830 è collegato al bus I2C all'indirizzo `FAN_ADDRESS` (`0x68`). Per lo
stadio di potenza della carrier è necessaria l'alimentazione esterna.

#### `void setFanSpeed(int speed)`

Imposta la velocità richiesta della ventola o del motore. L'intervallo è da
`-100` a `100`: valori positivi ruotano in avanti, negativi all'indietro e
`0` arresta l'uscita. Il valore richiesto viene conservato da `getFanSpeed()`.

```cpp
dot.setFanSpeed(60);   // avanti, circa 60%
dot.setFanSpeed(-40);  // indietro, circa 40%
dot.setFanSpeed(0);    // stop
```

#### `void stopFan()`

Arresta ventola o motore inviando il comando di stop al DRV8830.

#### `uint8_t getFanFault()`

Legge il registro di errore DRV8830:

| Bit | Significato |
| --- | --- |
| `D7` | Controllo clear/stato |
| `D4` | Evento limite di corrente (`ILIMIT`) |
| `D3` | Spegnimento per temperatura (`OTS`) |
| `D2` | Blocco per sottotensione (`UVLO`) |
| `D1` | Protezione da sovracorrente (`OCP`) |
| `D0` | Indicazione errore (`FAULT`) |

Per esempio `0x04` indica il bit `UVLO`. Controlla alimentazione, cablaggio e
carico prima di riprovare.

#### `void clearFanFault()`

Invia il comando di cancellazione al DRV8830. Cancellare un errore latched non
risolve una condizione ancora attiva di alimentazione, sovracorrente o calore.

#### `int getFanSpeed()`

Restituisce l'ultimo valore richiesto con `setFanSpeed()`, non una misura di RPM
o della tensione di uscita.

### Wi-Fi e orario

#### `void beginWiFi(const char* ssid, const char* password)`

Avvia in modo asincrono la connessione Wi-Fi con le credenziali indicate. Su
ESP32 configura anche i server NTP `pool.ntp.org` e `time.nist.gov`.

Se la connessione fallisce, il metodo ritorna senza bloccare indefinitamente.
`run()` chiude una sessione MQTT obsoleta, ritenta il Wi-Fi ogni 10 secondi e
MQTT ogni 5 secondi. Dopo tre tentativi MQTT falliti riavvia anche la stazione
Wi-Fi. Chiama questo metodo una sola volta da `setup()`.

#### `bool isWiFiConnected()`

Restituisce `true` quando l'interfaccia indica `WL_CONNECTED`, altrimenti
`false`.

#### `long getWiFiRSSI()`

Restituisce la potenza del segnale in dBm. Un valore più negativo indica un
segnale più debole.

#### `unsigned long getEpochTime()`

Restituisce l'orario Unix fornito dal servizio orario della scheda.

#### `String getFormattedTime(int gmtOffset = 1)`

Restituisce l'orario formattato usando l'offset UTC indicato in ore. Il default
è UTC+1.

### Connessione cloud Microeden

#### `void beginCloud(const char* deviceId, const char* token)`

Configura topic MQTT e connessione TLS per un dispositivo MyDot. L'endpoint è
`microeden.io` sulla porta `8243`. Su ESP32 installa il certificato CA ISRG
Root X1 incluso.

Chiama prima `beginWiFi()` e poi `run()` in modo continuativo. Se il Wi-Fi
cade, `run()` chiude MQTT e riconnette entrambi i servizi.

#### `bool isCloudConnected()`

Restituisce `true` quando MQTT è collegato al cloud Microeden.

#### `void setCloudBufferSize(uint16_t size)`

Imposta la dimensione del buffer MQTT/JSON. Il default configurato da
`beginCloud()` è 1024 byte.

#### `bool sendCloud()`

Serializza e pubblica il payload cloud quando MQTT è connesso. Dopo una
pubblicazione riuscita il payload viene svuotato; restituisce `false` se il
client è disconnesso o la pubblicazione fallisce.

#### `void setCloudSync(unsigned long interval, CloudSyncCallback callback)`

Registra un callback che `run()` esegue periodicamente mentre il cloud è attivo.

```cpp
void syncValues() {
  dot.writeKeyWord("temperature", dot.getTemperature());
  dot.sendCloud();
}

void setup() {
  dot.begin();
  dot.beginWiFi(SECRET_WIFI_SSID, SECRET_WIFI_PASSWORD);
  dot.beginCloud(SECRET_DEVICE_ID, SECRET_DEVICE_TOKEN);
  dot.setCloudSync(10000, syncValues);  // ogni 10 secondi
}
```

L'intervallo è espresso in millisecondi. Mantieni breve il callback ed evita
operazioni di rete bloccanti al suo interno.

### Payload cloud chiave/valore

#### `void writeKeyWord(const char* key, const char* value)`

Aggiunge o sostituisce una stringa nel documento JSON in uscita.

#### `void writeKeyWord(const char* key, double value)`

Aggiunge o sostituisce un numero a doppia precisione.

#### `void writeKeyWord(const char* key, float value)`

Aggiunge o sostituisce un numero floating point.

#### `void writeKeyWord(const char* key, int value)`

Aggiunge o sostituisce un intero.

#### `void writeKeyWord(const char* key, bool value)`

Aggiunge o sostituisce un booleano.

#### `void writeKeyWord(const char* key, const String& value)`

Aggiunge o sostituisce una `String` Arduino.

Chiama `sendCloud()` dopo aver aggiunto i valori da pubblicare.

#### `bool onCommand(const char* expectedCmd, const char* key = "content")`

Controlla la coda dei comandi cloud in ingresso (chiave predefinita `content`).
Ignora maiuscole/minuscole e spazi esterni, consuma il comando corrispondente
e non segnala ripetutamente lo stesso valore. I messaggi di reset vuoti non
eliminano i comandi già ricevuti.

#### `template <typename T> T readKeyWord(const char* key = "content")`

Legge il valore della chiave nel documento cloud ricevuto più recentemente e
lo converte nel tipo `T` richiesto.

```cpp
if (dot.onCommand("ON")) {
  dot.setRelay(true);
}

int requestedLevel = dot.readKeyWord<int>("level");
```

### Conversione colori

#### `static void hexToRGB(String hex, uint8_t& r, uint8_t& g, uint8_t& b)`

Converte un colore esadecimale a sei cifre in componenti rosso, verde e blu.
Accetta il prefisso opzionale `#`; input non valido produce `0, 0, 0`.

```cpp
uint8_t r, g, b;
MyDot::hexToRGB("#3366CC", r, g, b);
dot.setAllPixels(r, g, b);
```

#### `static String rgbToHex(uint8_t r, uint8_t g, uint8_t b)`

Converte componenti RGB nella stringa maiuscola `#RRGGBB`.

### Pulsanti

I pulsanti usano le resistenze interne di pull-up e sono attivi a livello basso.

#### `bool isButtonAPressed()`

Restituisce `true` mentre A è fisicamente premuto.

#### `bool isButtonBPressed()`

Restituisce `true` mentre B è fisicamente premuto.

#### `bool isButtonAClicked()`

Restituisce `true` una sola volta per ogni nuova pressione debounced di A. Un
nuovo click viene rilevato solo dopo il rilascio.

#### `bool isButtonBClicked()`

Equivalente a `isButtonAClicked()` per B.

I metodi click applicano un debounce di circa 50 ms; interrogali frequentemente
da `loop()`.

### Relè

L'uscita relè è collegata al pin `RELAY` della carrier e richiede alimentazione
esterna.

#### `void setRelay(bool state)`

Imposta il relè: `true` lo eccita, `false` lo disattiva.

#### `void toggleRelay()`

Inverte lo stato dell'uscita relè.

#### `bool isRelayOn()`

Restituisce `true` se l'uscita è eccitata, `false` se è disattiva. La lettura
non modifica l'uscita.

```cpp
if (dot.isButtonAClicked()) {
  dot.toggleRelay();
}
```

### NeoPixel

La libreria controlla `NUMPIXELS` (12) LED indirizzabili. I colori usano
intervalli da 0 a 255.

#### `void setAllPixels(uint8_t r, uint8_t g, uint8_t b)`

Imposta lo stesso colore RGB su tutti i pixel e aggiorna subito la striscia.

#### `void setPixel(uint16_t n, uint8_t r, uint8_t g, uint8_t b)`

Imposta un pixel tramite indice da zero e aggiorna la striscia. Gli indici fuori
intervallo vengono ignorati.

#### `void showPixels()`

Invia ai LED i colori presenti in memoria. Usalo dopo più modifiche per ottenere
un solo aggiornamento.

#### `void setBrightness(uint8_t b)`

Imposta la luminosità globale da `0` (spento) a `255` (massimo).

#### `void setRandomPixels()`

Assegna colori casuali a tutti i pixel e aggiorna la striscia.

#### `void clearPixels()`

Imposta tutti i pixel a nero e aggiorna la striscia.

```cpp
dot.setBrightness(80);
dot.setAllPixels(0, 40, 180);
dot.setPixel(0, 255, 0, 0);
dot.showPixels();
```

### Display OLED

Il display usa il controller SSD1306 all'indirizzo `SCREEN_ADDRESS` (`0x3C`),
con dimensioni `128x64`. È opzionale: controlla `isDisplayPresent()` prima di
usarlo.

#### `void drawLogo()`

Disegna il logo MyDot nel buffer e lo invia all'OLED.

#### `void updateSensorDisplay()`

Legge il BME690 e aggiorna l'OLED con temperatura, pressione, umidità e
resistenza del gas. Non disegna se display assente o lettura fallita.

#### `void clearDisplay()`

Cancella il buffer del display.

#### `void showDisplay()`

Trasferisce il buffer corrente all'OLED.

#### `void setCursor(int16_t x, int16_t y)`

Imposta la posizione del cursore in pixel.

#### `void setTextSize(uint8_t s)`

Imposta la scala del testo Adafruit GFX.

#### `void setTextColor(uint16_t c)`

Imposta il colore, normalmente `SSD1306_WHITE` o `SSD1306_BLACK`.

#### `void displayPrint(const String& text, bool clear = true)`

Stampa il testo usando cursore, dimensione e colore correnti. Se `clear` è
`true`, cancella il buffer prima della stampa e invia il risultato all'OLED.

#### `void displayPrint(const String& text, int x, int y, uint8_t size = 1, bool clear = true)`

Imposta cursore e dimensione, stampa il testo e può cancellare prima il buffer.

#### `Adafruit_SSD1306& getDisplay()`

Restituisce un riferimento all'oggetto Adafruit SSD1306 sottostante per disegni
avanzati.

#### `bool isDisplayPresent()`

Restituisce `true` quando l'OLED è stato rilevato e inizializzato.

#### `template <typename T> void print(T val)`

Stampa un valore nel buffer tramite Adafruit GFX.

#### `template <typename T> void println(T val)`

Stampa un valore seguito da un ritorno a capo.

#### `void println()`

Stampa solo un ritorno a capo.

Dopo operazioni a basso livello chiama `showDisplay()` per trasferire il buffer.

### Sensore ambientale BME690

L'API espone temperatura, pressione, umidità relativa e resistenza del gas
tramite l'interfaccia di misura compatibile Adafruit BME680.

#### `bool readSensors()`

Avvia una nuova misura e restituisce `true` quando è disponibile.

#### `float getTemperature()`

Restituisce la temperatura in gradi Celsius dell'ultima lettura valida.

#### `float getPressure()`

Restituisce la pressione atmosferica in hPa.

#### `float getHumidity()`

Restituisce l'umidità relativa in percentuale.

#### `float getGasResistance()`

Restituisce la resistenza del gas in kOhm.

```cpp
if (dot.readSensors()) {
  Serial.print("Temperature: ");
  Serial.print(dot.getTemperature());
  Serial.println(" C");
  Serial.print("Pressure: ");
  Serial.print(dot.getPressure());
  Serial.println(" hPa");
  Serial.print("Humidity: ");
  Serial.print(dot.getHumidity());
  Serial.println(" %");
  Serial.print("Gas resistance: ");
  Serial.print(dot.getGasResistance());
  Serial.println(" kOhm");
}
```

### Scheda SD

Il chip select SD è `SD_CS` (`D10` sul connettore Nano standard).

#### `bool beginSD()`

Inizializza l'interfaccia SD e restituisce l'esito.

#### `File openFile(const char* filename, const char* mode = "r")`

Apre un file e restituisce il relativo oggetto `File`. Usa `"r"` per lettura,
`"w"` per sostituzione/scrittura o `"a"` per aggiunta; MyDot converte queste
stringhe nel formato nativo del core selezionato.

#### `bool fileExists(const char* filename)`

Restituisce `true` se il percorso esiste sulla SD.

#### `void removeFile(const char* filename)`

Rimuove il file indicato dalla SD.

#### `bool writeFile(const String& path, const String& message)`

Scrive un messaggio sostituendo il file esistente.

#### `bool appendFile(const String& path, const String& message)`

Aggiunge un messaggio, creando o aprendo il file secondo quanto supportato dal
driver SD del core.

### Widget cloud

Le classi widget forniscono wrapper tipizzati associati a una chiave cloud e a
un riferimento `MyDot`.

#### `template <typename T> CloudWidget<T>(const char* key, MyDot& device)`

Crea un widget generico associato a `key`.

#### `void CloudWidget<T>::write(T value)`

Aggiunge un valore tipizzato al payload cloud in uscita. Usa `sendCloud()` per
pubblicarlo.

#### `T CloudWidget<T>::read()`

Legge il valore tipizzato della chiave cloud. `Slider::read()` restituisce
l'ultimo valore compatto `key_value` come intero e `-1` finché non arriva un
valore; quindi `0` resta un valore valido.

#### `ColorWheel`

`ColorWheel(const char* key, MyDot& device)` crea un widget colore.

`getRGB(uint8_t& r, uint8_t& g, uint8_t& b)` legge il colore corrente e lo
converte in RGB. `write(uint8_t r, uint8_t g, uint8_t b)` aggiunge il colore
al payload in uscita; usa `sendCloud()` per pubblicarlo.

#### `Level`, `Slider`, `Switch`, `Pushbutton`, `Led` e `Photo`

Ogni classe ha il costruttore:

```cpp
ClassName(const char* key, MyDot& device);
```

Usa `write()` e `read()` ereditati con il tipo appropriato al widget.

#### `Map`

`Map(const char* key, MyDot& device)` crea un widget mappa.

`write(double lat, double lng)` aggiunge latitudine e longitudine; l'overload
`write(const String& value)` aggiunge un valore mappa già formattato. Usa
`sendCloud()` per pubblicarlo.

## Risoluzione dei problemi

### Relè, NeoPixel o ventola non funzionano

Alimenta la carrier dal jack DC esterno. La sola USB può alimentare la Nano
ESP32 ma non gli stadi di potenza della carrier.

### Comportamento inatteso dei pin della Nano ESP32

Seleziona **Arduino Nano ESP32** e verifica l'opzione di numerazione dei pin
nell'Arduino IDE. MyDot usa gli alias Nano della tabella e converte internamente
il pin NeoPixel ESP32.

### `getFanFault()` restituisce `0x04`

Lo stato `0x04` indica `UVLO` (blocco per sottotensione). Controlla tensione di
alimentazione, cablaggio, polarità del connettore e cadute sotto carico. Dopo
aver risolto la causa, cancella il fault e rileggilo.

### La connessione cloud non parte

Controlla i quattro valori in `microeden_secrets.h`, chiama `beginWiFi()` prima
di `beginCloud()` e continua a richiamare `run()` da `loop()`. Su ESP32 la
libreria verifica il certificato del server usando la CA ISRG Root X1 inclusa.

## Licenza

Consulta il file `LICENSE` del repository per i termini applicabili alla
libreria MyDot.
