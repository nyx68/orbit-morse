# Orbit Morse

Orbit Morse ist ein IoT Projekt, das im Rahmen des Themas "Verbundenheit über Distanz" entstanden ist. Das Ergebnis ist ein System mit zwei Endpunkten, die über das Internet miteinander verbunden sind und es zwei Personen ermöglichen, über Distanz miteinander zu kommunizieren. Die Kommunikation basiert auf Morsecode. Über einen Touchsensor werden kurze und lange Berührungen registriert, die über MQTT übertragen und beim anderen Endpunkt über einen LED-Ring als Lichtsignale visualisiert werden sowie zusätzlich dekodiert als Text auf einem Display ausgegeben werden. Die übergeordnete Verarbeitung erfolgt in Node-RED, die WLAN Verbindung kann über WiFi-Manager konfiguriert werden und Planetenmodelle schaffen eine atmosphärische Optik. 

---

# DIY-Anleitung

## Benötigte Hardware-Komponenten

Pro Endpunkt wird benötigt:
- 1x ESP32 
- 1x kapazitiver Touchsensor 
- 1x 12-Pixel NeoPixel LED-Ring (z.B. von AZ-Delivery)
- 1x Grove-LCD RGB Backlight (V5.0 empfohlen)
- 3x Kabel
- 1x USB-Kabel
- optional: 1x Planetenmodell (z.B. 3D gedruckt, empfohlener Durchmesser: 75 mm)

Ein Planetenmodell ist für die technische Funktion nicht unbedingt erforderlich, wird für die vorgesehene Gestaltung aber empfohlen. Es dient als Gehäuse für den LED-Ring und unterstützt die atmosphärische Wirkung von Orbit Morse.

## Verkabelung

| Bauteil 	| ESP32 Pins	| benötigte Versorgungsspannung	|
|---------------|---------------|-------------------------------|
| Touchsensor 	| GPIO 34 	| 3,3V				|
| LED-Ring 	| GPIO 16 	| 5V				|
| Display	| I<sup>2</sup>C-Anschluss | 5V bei V4.0 und 3,3V oder 5V bei V5.0 |

### Hinweise:
- Die Pins können im Code geändert werden. 
- Die Anschlüsse für den LED-Ring müssen selbstständig angelötet werden.
- Bei dem Display die benötigte Versorgungsspannung beachten. Wenn der I<sup>2</sup>C Anschluss des ESPs nur 3,3V bereitstellt aber das Display 5V benötigt, muss beides so verkabelt werden dass die Datenübertragung über den I<sup>2</sup>C Anschluss erfolgt aber für die Versorgungsspannung 5V bereitgestellt werden.
- Falls ein Planetenmodell verwendet werden soll, den LED Ring darin platzieren. 

## Installation

1. Repository klonen/herunterladen
2. Projekt in Visual Studio Code mit PlatformIO öffnen. Darauf achten, dass sich die Datei `platformio.ini` im Hauptverzeichnis befindet, welche alle benötigten Bibliotheken enthält, so dass diese nicht manuell installiert werden müssen.
3. ESP32 per USB anschließen

### Software Konfiguration
4. LocalId konfigurieren: für Endpunkt 1 `localId = 1` einstellen, für Endpunkt 2 `localId = 2` einstellen (es können auch individuelle IDs gewählt werden, wichtig ist nur dass beide Endpunkte unterschiedliche localIDs besitzen)

### MQTT Konfiguration
5. MQTT Broker konfigurieren (das Projekt verwendet standardmäßig HiveMQ auf Port 1883); MQTT-Topics anpassen: Unbedingt eigene MQTT-Topics wählen oder zumindest die ProjektID anpassen. Falls ein öffentlicher MQTT-Broker verwendet wird, könnten sonst ungewollte Nebeneffekte entstehen.

6. Code auf den ESPs installieren

## WLAN Konfiguration 

1. ESP32 einschalten
2. Wenn noch keine WLAN-Verbindung vorhanden ist, öffnet der ESP32 einen eigenen Access Point. Vom PC/Smartphone aus mit `OrbitMorse_Setup1` für den ersten Endpunkt und `OrbitMorse_Setup2` für den zweiten Endpunkt verbinden und Passwort `12345678` eingeben.
3. Im Konfigurationsportal das lokale WLAN auswählen.
4. WLAN-Passwort eingeben. Der ESP32 verbindet sich anschließend selbstständig mit dem WLAN.

## Node RED Konfiguration

1. Node RED installieren und starten (node-red über die Kommandozeile eingeben)
2. Node RED im Browser öffnen. Im Menü die Funktion "Import" auswählen und den Inhalt der Datei `orbit-morse-flow.json` importieren
3. Topics konsistent entsprechend der Topics im Code anpassen
4. Dann auf "Deploy" klicken um die Änderungen zu übernehmen

## System testen

1. Beide ESP anschließen
2. Prüfen, ob beide mit dem WLAN verbunden sind
3. Falls noch nicht erfolgt, Node RED starten
4. Prüfen, ob MQTT verbunden ist
5. Prüfen, ob Display und LED-Ring funktionieren. Auf dem Display sollte nach dem Einschalten "Orbit Morse" stehen und der LED-Ring sollte leuchten und langsam die Farbe ändern. 
6. Prüfen, ob das übertragen einer Nachricht funktioniert. Dafür den Touchsensor an einem der beiden ESP berühren. Währendessen sollten beide LED-Ringe hellweiß leuchten. Nach kurzer Zeit sollte auf dem Display ein Buchstabe oder ein "?" stehen.

---

# Verwendung von Orbit Morse

Die Kommunikation über Orbit Morse basiert auf Morse Code, das heißt kurze Berührungen werden als Punkt und lange Berührungen als Strich interpretiert. Nachrichten werden automatisch dekodiert. Orbit Morse unterstützt standardmäßig alle Buchstaben von A - Z. 

### Morse Code Tabelle

| Buchstabe | Morsecode |
|-----------|-----------|
| A | .- |
| B | -... |
| C | -.-. |
| D | -.. |
| E | . |
| F | ..-. |
| G | --. |
| H | .... |
| I | .. |
| J | .--- |
| K | -.- |
| L | .-.. |
| M | -- |
| N | -. |
| O | --- |
| P | .--. |
| Q | --.- |
| R | .-. |
| S | ... |
| T | - |
| U | ..- |
| V | ...- |
| W | .-- |
| X | -..- |
| Y | -.-- |
| Z | --.. |

### Nachricht senden
Zum Senden einer Nachricht einfach den Touchsensor berühren. Nachrichtenbeginn und Ende werden automatisch erkannt. Sobald der Touchsensor berührt wird, beginnt eine Nachricht. Es kann immer nur über einen Endpunkt eine Nachricht aufgenommen werden. Die zuletzt eingegeben Morsesequenz bzw. der zuletzt dekodierte Buchstabe können durch eine sehr lange Berührung gelöscht werden. Erfolgt nach einer gewissen Zeit keine Eingabe mehr, gilt die Nachricht automatisch als abgeschlossen. Danach sind keine weiteren Eingaben von diesem Endpunkt aus möglich, bis der andere Endpunkt die Nachricht bestätigt.

### Nachricht bestätigen
Zum Bestätigen der Nachricht entweder einmal den Touchsensor sehr lange gedrückt halten oder selbst Morsen. Erfolgt die Bestätigung nicht innerhalb von 60 Sekunden, gilt die Nachricht als nicht wahrgenommen und der entfernte Endpunkt signalisiert über eine Animation, dass eine Nachricht wartet.

### Ausstehende Nachricht abspielen
Zum Abspielen einer ausstehenden Nachricht den Touchsensor lange berühren. Danach wird die Nachricht genauso abgespielt wie ursprünglich aufgenommen. Anschließend kann die Kommunikation  wieder wie bisher fortgeführt werden.

---

# Anpassungsmöglichkeiten

Die Zeiten für Morsezeichen können im Code nach Bedarf angepasst werden. 

| Variable 		| Default 	| Bedeutung/Funktion		|
|-----------------------|---------------|-----------------------|
| dashThreshold 	| 400 ms 	| Grenze Punkt/Strich	|
| characterPauseThreshold 	| 1000 ms | Zeit nach der Morse Sequenz als beendet gilt und dekodiert wird	|
| wordPauseThreshold 	| 3000 ms 	| Zeit nach der Wort als beendet gilt	|
| messageEndThreshold	| 10000 ms 	| Zeit nach der Nachricht als beendet gilt	|
| longTouchThreshold 	| 2000 ms 	| Nachricht bestätigen/Nachricht abspielen/Buchstabe löschen	|
