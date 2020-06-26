# SmartBlind
Progetto per l'esame Sistemi Embedded - Dipartimento di Informatica dell'Università degli Studi di Milano.

## Il progetto
SmartBlind è un controller per tapparelle integrato con la piattafoma HAP (https://developer.apple.com/homekit/) di Apple per l'IoT. Lo scopo del progetto sarebbe il poter controllare il funzionamanto di tapparelle (o tende elettriche) dai dispositivi Apple direttamente dall'applicazione Home, cosi da non dover creare un'App ad-hoc.
Il controller riceverà le istruzioni via WiFi e azionerà un motore bifase per tirare su o giù le tapparelle e controllerà gli eventuali input manuali dell'utente mettendosi a valle degli interruttori a muro e quindi intecettando gli azionamenti degli stessi.

### Architettura del sistema
Sebbene Apple abbia rilasciato HomeKit per essere direttamente eseguito sul dispositivo, questo pone diverse lungaggini di presentazione, gestione della sicurezza etc. quindi si è scelto di optare per una complicazione architetturale, che però semplificherà la programmazione.
In particolare utilizzeremo un Raspberry Pi come bridge (un dispositivo che fa da gateway tra le chiamate di HomeKit e i protocolli che si scelgono di utilizzare); per fare questo, basterà installare il pacchetto HomeBridge.

In sunto, il ciclo di vita di una chiamata è questa:
* Utente fa un'operazione sul dispositivo, il dispositivo dispaccia una chiamata seguendo il protocollo HomeKit al HomeBridge,
* l'HomeBridge riceve la chiamata e la gestisce, facendo una chiamata di rete (di tipo HTTP/GET) al ESP32
* l'ESP32, configurato in modalità server, gestisce la chiamata HTTP/GET e risponde opportunamente
* HomeBridge interpreta la risposta e la riporta al dispositivo, seguendo le API di Apple.

### Hardware
* ESP 32 (NodeMCU di AzDelivery, per lo [schema](https://cdn.shopify.com/s/files/1/1509/1638/files/ESP_-_32_NodeMCU_Developmentboard_Pinout_Diagram.jpg?4479111012146266271))
* 2 Relay da 3/5V DC a contatto puro (per 220V AC)
* 2 230v/110v AC Mains Detection Module (in via teorica si potrebbero usare anche dei relay da 220V AC, ma danno problemi di tipo magnetico)
* Opzionale, un RaspberryPI per fare da server

Lo schema del circuito è riportato nel file .png .

### Software preparatorio
La preparazione del layer software è stata tratta dalla repo:

https://github.com/skarim/arduino-homekit-blinds

Che a sua volta sfrutta come base 

https://github.com/Nicnl/homebridge-minimal-http-blinds

Il Codice è stato modificato per adattarsi alle esigenze richieste.


## Configurazione

Nel file `/lib/config.h` possono essere inserite le credenziali di accesso al WiFi e i tempi di salita e discesa delle tapparelle, è in oltre disponibile una modalità di debug, che abilita tutti i print su seriale, settando la variabile ```DEBUG_MODE``` a 1.

Dato che il pacchetto di gestione su HomeBridge ha bisogno dell'indirizzo IP del ESP, bisogna o dichiararlo statico nel codice (o via router), oppure creare un nome di rete del dispositivo.

I pin di input-output sono direttamente nel file `/src/main.cpp` e basta modificare le linee:
```
#define LED_BUILTIN 2
#define INPUT_PIN_UP 32
#define INPUT_PIN_DOWN 33
#define RELAY_PIN_UP 27
#define RELAY_PIN_DOWN 26

#define FIRST_TAP 500
#define SECOND_TAP 1000
```

`FIRST_TAP` e `SECOND_TAP` sono la durata dei due tocchi consecutivi sui pulsanti e usate nella funzione `doubleTap` che invoca `startSpinning` a 0 o 100 in base al pulsante che ha invocato la funzione.

## Connessione ad HomeKit

Per la connessione all'applicazione HomeKit ho utilizzato la piattaforma [homebridge](https://github.com/nfarina/homebridge) in quanto molto semplice da implementare e facile da utilizzare essendo in NodeJS.

In particolare ho utilizzato il plugin [Minimal HTTP Blinds](https://github.com/Nicnl/homebridge-minimal-http-blinds) per comunicare con la board via chiamate HTTP asincrone di tipo GET.

## Chiamate di rete
L'ESP32 mette a disposizione queste chiamate di rete:

* ``` http://IP_ESP/position ```, chiede la posizione puntuale delle tapparelle, la risposta sarà di tipo 200, con testo la posizione (espressa in percentuale).
* ``` http://IP_ESP/state ```, chiede lo stato delle tapparelle, la risposta sarà di tipo 200, con testo:
  * 0 = in apertura
  * 1 = in chiusura
  * 2 = ferma
* ``` http://IP_ESP/set?position=POSIZIONE ```, setta la posizione desiderata della tapparella (espressa in percentuale), la risposta sarà di tipo 204.
* ``` http://IP_ESP/timing?secondsToClose=SECONDI_CHIUSURA ```, setta i secondi impiegati dalla tapparella per chiudersi, la risposta sarà di tipo 204.
* ``` http://IP_ESP/timing?secondsToClose=SECONDI_APERTURA ```, setta i secondi impiegati dalla tapperella per aprirsi, la risposta sarà di tipo 204.
