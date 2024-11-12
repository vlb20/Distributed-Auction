/*********SEQUENZIATORE*******/

#include <esp_now.h>
#include <WiFi.h>
#include <vector>

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
std::vector<struct_message> holdBackQueueSeq;           // Hold-back queue Sequenziatore


// Struttura per messaggi
typedef struct struct_message {
    int bid;                                            //bid dell'offerta nel messaggio
    int messageId;                                      //id del messaggio, utile per riconoscere i messaggi in fase di ricezione
    int senderId;                                       //id del mittente del messaggio
    int sequenceNum;                                    //sequence number associato al messaggio
    int vectorClock[NUM_NODES];                         //vector clock inviato nel messaggio
    String messageType;                                 //tipo di messaggio ("bid", "order")
} struct_message;

struct_message auctionMessageToSend;                    //Messaggio da inviare
struct_message auctionMessageToReceive;                 //Messaggio ricevuto


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

    holdBackQueueSeq.push_back(auctionMessageToReceive);                              // Aggiungi il messaggio alla hold-back queue
    Serial.println("Messaggio aggiunto alla hold-back queue.");
    causalControl(auctionMessageToReceive);                                           // Controllo la causalità

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

  buttonPressed = true;                                                             //sto simulando l'inizio di una sola asta

}

/*************************FUNZIONE LOOP***************************************************/
void loop() {

  if(buttonPressed){                                                                 // Se è stato premuto il bottone di inizio asta
    buttonPressed = false;
    startAuction();                                                                  // setto le variabili iniziali, tra cui la variabile che segna l'inizio dell'asta
  }

  if(auctionStarted){                                                               // finchè l'asta non è finita
    checkEndAuction();                                                              // controllo se l'asta è finita
  }

}

bool causalControl(struct_message messageToCheck){
  if((messageToCheck.vectorClock[messageToCheck.senderId] == vectorClock[messageToCheck.senderId] + 1) && isCausallyRead(messageToCheck)){
    holdBackQueueSeq.pop_back();                                                  // Rimuovo il messaggio dalla coda
    CO_Deliver(messageToCheck);                                                   // Invio il messaggio al livello superiore
    return true;
  }
  return false;
}

void CO_Deliver(struct_message message){
  auctionMessageToSend = message;
  vectorClock[messageToCheck.senderId]++;
  sendSequencer(message);
  retrieveMessage();
}

void retrieveMessage(){
  for (auto it = holdBackQueueSeq.begin(); it != holdBackQueueSeq.end(); ) {
        struct_message msg = *it;
        if(causalControl(msg)){
            Serial.println("Messaggio recuperato");
        } else {
            ++it;
        }
    }
}


void sendSequencer(struct_message message) {

    message.messageType = "order";                                             // Setto il tipo di messaggio

    // Controllo se la Highest Bid è cambiata
    if(message.bid > highestBid){                                      // Se la bid che ho prelevato è più grande...
      highestBid = message.bid;                                        // Aggiorno l'offerta più alta la momento
      winnerNodeId = message.senderId;                                 // e il vincitore momentaneo
    }
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &message, sizeof(message));
    message.sequenceNumber = sequenceNumber++;

    restartTimer = millis();                                                // Aggiorno il timer di restart

    if (result == ESP_OK) {
        Serial.println("Messaggio inviato con successo");
    } else {
        Serial.println("Errore nell'invio del messaggio");
    }
}

bool isCausallyRead(struct_message messageToCheck){
  for (int i = 0; i < NUM_NODES; i++) {
    if(i == messageToCheck.senderId){ //Non controllo il nodo che ha inviato il messaggio
      continue;

    }else if (messageToCheck.vectorClock[i] > vectorClock[i]) {
      return false;
    }
  }
  return true;
}

// Funzione che monitora la fine dell'asta
void checkEndAuction(){

  if(millis() - restartTimer >= DURATION_TIME){                                    // Se il tempo attuale (millis()) meno il tempo di inizio asta (restartTimer) è maggiore di Duration
    auctionStarted = false;                                                        // L'asta è finita, tutti a casa, LUKAKU è mio!!
  }

  Serial.println("Ha vinto il nodo " + String(winnerNodeId));                      // Annuncio il vincitore
  Serial.println("con un offerta di " + highestBid);

}
