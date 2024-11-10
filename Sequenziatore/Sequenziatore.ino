/*********SEQUENZIATORE*******/

#include <esp_now.h>
#include <WiFi.h>
#include <vector>
#include <FreeRTOS.h>

#define DURATION_TIME 10000
#define NUM_NODES 5

// Indirizzo di broadcast per inviare a tutti i nodi
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Parametri asta e variabili globali
int sequenceNumber = 0;                                 //serve al sequenziatore per impartire l'ordine totale
int vectorClock[NUM_NODES] = {0,0,0,0,0};               //serve a tutti i partecipanti per avere un ordine causale
int highestBid = 0                                      //segna il valore dell'offerta più alta attuale
int winnerNodeId = -1                                   //id del vincitore dell'asta attuale
unsigned long auctionEndTime = 0;                       //tempo di fine asta
unsigned long restartTimer = 0;                         // con la funzione millis() aggiorno ogni volta quando mi arriva un offerta, mi serve per implementare un timer
bool auctionStarted = false;                            // Flag per tracciare se l'asta è partita
bool buttonPressed = false;                             //simulazione bottone inizio asta

//Coda dei messaggi in attesa
std::vector<struct_message> holdBackQueue;              // Hold-back queue

// Mutex per proteggere l’accesso alla hold-back queue
SemaphoreHandle_t mutex;

// Struttura per messaggi
typedef struct struct_message {
    int bid;                                            //bid dell'offerta nel messaggio
    int messageId;                                      //id del messaggio, utile per riconoscere i messaggi in fase di ricezione
    int senderId;                                       //id del mittente del messaggio
    int sequenceNum;                                    //sequence number associato al messaggio
    int vectorClock[NUM_NODES];                         //vector clock inviato nel messaggio
} struct_message;

struct_message auctionMessageToSend;                    //Messaggio da inviare
struct_message auctionMessageToReceive;                 //Messaggio ricevuto


/*********************INIZIO FUNZIONI THREAD*********************************/

// Funzione per verificare la causalità
bool compareVectorClock(const struct_message &a, const struct_message &b) {        
    bool atLeastOneStrictlySmaller = false;                                         //variabile per segnare che almeno in una posizione si ha la relazione di minore stretto
    for (int i = 0; i < NUM_NODES; i++) {                                           //scorro tutte le posizioni del vectorClock (ogni nodo ha la propria posizione i)
        if (a.vectorClock[i] > b.vectorClock[i]){                                   //controllo ogni posizione se il numero i-esimo di A è più grande (stretto) di B
            return false;                                                           //se ciò accade esco dalla funzione e ritorno risultato negativo
        }
        if (a.vectorClock[i] < b.vectorClock[i]){                                   //se nessuna posizione è più grande devo controllare se almeno un numero i-esimo è più piccolo del corrispettivo B
          atLeastOneStrictlySmaller = true;                                         //se ne ho trovato uno, setto variabile a TRUE
        } 
    }
    return atLeastOneStrictlySmaller;
}

// Funzione che periodicamente fa il riordinamento causale della holdBack queue
void sortTask(void *parameter){
  
  while(true){                                                                      //ciclo indefinito, poichè ogni volta controlla se ci sono messaggi in coda da riordinare
    
    xSemaphoreTake(mutex, portMAX_DELAY);                                           // Acquisisci il mutex per ordinare la coda

    if(!holdBackQueue.empty() && holdBackQueue.size > 1) {                          //se la coda non è vuota ed ha almeno due elementi...
        std::sort(holdBackQueue.begin(), holdBackQueue.end(), compareVectorClock);  // Ordina la hold-back queue in base al vector clock
        Serial.println("Hold-back queue ordinata.");
    }

    xSemaphoreGive(mutex);                                                          // Rilascia il mutex dopo l’ordinamento

    vTaskDelay(100 / portTICK_PERIOD_MS);                                           // Delay per evitare un ciclo troppo veloce

  }

}


/************************FINE FUNZIONI THREAD********************************/

// Funzione per iniziare l'asta
void startAuction(){
  highestBid = 0;
  winnerNodeId = -1;
  sequenceNumber = 0;
  auctionStarted = true;                                                            // metto a true l'inizio dell'asta
  restartTimer = millis()                                                           // leggo e salvo il tempo di inizio asta
  auctionEndTime = DURATION_TIME;
}

// Callback invio dati
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("\r\nLast Packet Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Failed");
}

// Calback ricezione dati
void onDataReceive(const uint8_t *mac, const uint8_t *incomingData, int len){
    memcpy(&auctionMessageToReceive, incomingData, sizeof(auctionMessageToReceive));
    
    
    xSemaphoreTake(mutex, portMAX_DELAY);                                           // Blocca il mutex prima di accedere al vettore condiviso
    holdBackQueue.push_back(auctionMessageToReceive);                               // aggiungo il messagio in arrivo nella coda, dove sarà riordinato dal thread addetto
    xSemaphoreGive(mutex);                                                          // Rilascia il mutex dopo l'accesso

    /*
    DUBBIO
    Probabilmente si risolverà solo provando il codice, ma la questione è questa:
    quando arriva un messaggio dovrei ri-iniziare la variabile "restartTimer" con millis()
    però al sequenziatore possono arrivare anche messaggio non in sequenza causale
    quindi penso che non bisogna farla "restartare" alla ricezione, ma all'effettivo invio del sequenziatore
    così si ha anche un responso grafico più preciso: non si vede che il timer è tornato a dieci, si aspetta che la coda si ordini,
    e solo poi arriva il messaggio a tutti, sarebbe un brutto scenario se la restartassimo qui.
    */

    Serial.println("Messaggio aggiunto alla hold-back queue.");
}

/**********************FUNZIONE DI SETUP**************************************/
void setup() {

  Serial.begin(115200);
  Wifi.mode(WIFI_STA);


  if (esp_now_init() != ESP_OK) {                                                   // Se la connesione esp non è andata a buon fine                                
        Serial.println("Error initializing ESP-NOW");                               // lo segnalo e termino il programma
        return;
  }

  esp_now_register_send_cb(OnDataSent);                                             // registro la funzione "OnDataSent()" come funzione di callback all'invio di un messagio

  memcpy(peerInfo.peer_addr, broadcastAddress, 5);                                  // copio le informazione dei peer nelle locazioni dei peer address
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        return;
  }

  esp_now_register_recv_cb(OnDataRecv);                                             // registro la funzione "OnDataRecv()" come funzione di callback alla ricezione di un messagio

  // Crea il mutex per la coda condivisa
  mutex = xSemaphoreCreateMutex();

  // Crea i task per ricezione e ordinamento
  xTaskCreate(sortTask, "Sort Task", 2048, NULL, 1, NULL);

  buttonPressed = true;                                                             //sto simulando l'inizio di una sola asta

}

/*************************FUNZIONE LOOP***************************************************/
void loop() {
  
  if(buttonPressed){                                                                 // Se è stato premuto il bottone di inizio asta
    buttonPressed = false;                                                               
    startAuction()                                                                  // setto le variabili iniziali, tra cui la variabile che segna l'inizio dell'asta
  }

  if(auctionStarted){                                                               // finchè l'asta non è finita 
    checkForIncomingMessages();
    checkEndAuction();                                                     
  }


}

// Funzione principale
void checkForIncomingMessages(){

  if (!holdBackQueue.empty()) {                                                     // Controlla che la coda non sia vuota
    
    xSemaphoreTake(mutex, portMAX_DELAY);
    auctionMessageToSend = holdBackQueue.front();                                   // Prendi il primo elemento ordinato
    holdBackQueue.erase(holdBackQueue.begin());                                     // Faccio il pop dell'elemento
    xSemaphoreGive(mutex);  

    // Aggiorno tutte le variabili da inviare
    sequenceNumber++;                                                               // Aggiorno il numero di sequenza ogni qual volta invio io sequenziatore
    auctionMessageToSend.sequenceNumber = sequenceNumber;                           // lo aggiorno anche nel messaggio da inviare
    vectorClock = auctionMessageToSend.vectorClock;
    
    // Controllo se la Highest Bid è cambiata
    if(auctionMessageToSend.bid > highestBid){                                      // Se la bid che ho prelevato è più grande...
      highestBid = auctionMessageToSend.bid;                                        // Aggiorno l'offerta più alta la momento
      winnerNodeId = auctionMessageToSend.senderId;                                 // e il vincitore momentaneo
    }

    // Invio il messaggio a tutti
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &auctionMessageToSend, sizeof(auctionMessageToSend));
   
    if (result == ESP_OK) {
      Serial.println("Sent with success");
    }
    else {
      Serial.println("Error sending the data");
    }

  }

}

// Funzione che monitora la fine dell'asta
void checkEndAuction(){

  if(millis() - restartTimer >= DURATION_TIME){                                    // Se il tempo attuale (millis()) meno il tempo di inizio asta (restartTimer) è maggiore di Duration
    auctionStarted = false;                                                        // L'asta è finita, tutti a casa, LUKAKU è mio!!
  }

  Serial.println("Ha vinto il nodo " + String(winnerNodeId));                      // Annuncio il vincitore
  Serial.println("con un offerta di " + highestBid);

}
