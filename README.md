# SmartBlind
Progetto per l'esame Sistemi Embedded - Dipartimento di Informatica dell'Università degli Studi di Milano.

## Il progetto
SmartBlind è un controller per tapparelle integrato con la piattafoma HAP (https://developer.apple.com/homekit/) di Apple per l'IoT. Lo scopo del progetto sarebbe il poter controllare il funzionamanto di tapparelle (o tende elettriche) dai dispositivi Apple direttamente dall'applicazione Home, cosi da non dover creare un'App ad-hoc.
Il controller riceverà le istruzioni via WiFi e azionerà un motore bifase per tirare su o giù le tapparelle e controllerà gli eventuali input manuali dell'utente mettendosi a valle degli interruttori a muro e quindi intecettando gli azionamenti degli stessi.

### Hardware
* ESP 32
* 2 Relè da 220V
* 2 Step-down da 220V a 3V

### Software preparatorio
La preparazione del layer software è stata tratta dalla repo:

https://github.com/maximkulkin/esp-homekit-demo

E dal sito:

https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html#get-started-get-esp-idf
