# SmartBlind
Progetto per l'esame Sistemi Embedded - Dipartimento di Informatica dell'Università degli Studi di Milano.

## Il progetto
SmartBlind è un controller per tapparelle integrato con la piattafoma HAP (https://developer.apple.com/homekit/) di Apple per l'IoT. Lo scopo del progetto sarebbe il poter controllare il funzionamanto di tapparelle (o tende elettriche) dai dispositivi Apple direttamente dall'applicazione Home, cosi da non dover creare un'App ad-hoc.
Il controller riceverà le istruzioni via WiFi e azionerà un motore bifase per tirare su o giù le tapparelle e controllerà gli eventuali input manuali dell'utente mettendosi a valle degli interruttori a muro e quindi intecettando gli azionamenti degli stessi.

### Hardware
* ESP 32
* 2 Relay da 3/5V DC a contatto puro (per 220V AC)
* 2 Relay da 220V AC a contatto puro
* Opzionale, un RaspberryPI per fare da server

### Software preparatorio
La preparazione del layer software è stata tratta dalla repo:

https://github.com/skarim/arduino-homekit-blinds

Che a sua volta sfrutta come base 

https://github.com/Nicnl/homebridge-minimal-http-blinds

Il Codice è stato modificato per adattarsi alle esigenze richieste.


## Configurazione

Nel file `/lib/config.h` possono essere inserite le credenziali di accesso al WiFi e i tempi di salita e discesa delle tapparelle.

I pin di input-output sono direttamente nel file `/src/main.cpp` e basta modificare le linee:
```
#define LED_BUILTIN 2
#define INPUT_PIN_UP 25
#define INPUT_PIN_DOWN 26
#define RELAY_PIN_UP 12
#define RELAY_PIN_DOWN 13

#define FIRST_TAP 500
#define SECOND_TAP 1000
```

`FIRST_TAP` e `SECOND_TAP` sono la durata dei due tocchi consecutivi sui pulsanti e usate nella funzione `doubleTap` che invoca `startSpinning` a 0 o 100 in base al pulsante che ha invocato la funzione.

## Connessione ad HomeKit

Per la connessione all'applicazione HomeKit ho utilizzato la piattaforma [homebridge](https://github.com/nfarina/homebridge) in quanto molto semplice da implementare e facile da utilizzare essendo in NodeJS.

In particolare ho utilizzato il plugin [Minimal HTTP Blinds](https://github.com/Nicnl/homebridge-minimal-http-blinds) per comunicare con la board via chiamate HTTP asincrone di tipo GET.
