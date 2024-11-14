/*********SEQUENZIATORE*******/

#include <esp_now.h>
#include <WiFi.h>
#include <vector>
#include <map>

#define DURATION_TIME 10000
#define NUM_NODES 5

// Indirizzo di broadcast per inviare a tutti i nodi
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
String mac_sequencer = "ac:15:18:e9:9f:4c";

// Crea la mappa per associare MAC address a numeri da 0 a 4
std::map<String, int> macToNumberMap;

// Parametri asta e variabili globali
int sequenceNumber = 0;                                 //serve al sequenziatore per impartire l'ordine totale
int myNodeId = 0;                                       //id del nodo
String myMacAddress = "";
int vectorClock[NUM_NODES] = {0,0,0,0,0};               //serve a tutti i partecipanti per avere un ordine causale
int highestBid = 0;                                      //segna il valore dell'offerta più alta attuale
int winnerNodeId = -1;                                   //id del vincitore dell'asta attuale
unsigned long auctionEndTime = 0;                       //tempo di fine asta
unsigned long restartTimer = 0;                         // con la funzione millis() aggiorno ogni volta quando mi arriva un offerta, mi serve per implementare un timer
bool auctionStarted = false;                            // Flag per tracciare se l'asta è partita
bool buttonPressed = false;                             //simulazione bottone inizio asta
bool buttonBid = false;                                 //simulazione bottone offerta

// Struttura per messaggi
typedef struct struct_message {
    int bid;                                            //bid dell'offerta nel messaggio
    int highestBid;                                     //offerta più alta attuale
    int messageId;                                      //id del messaggio, utile per riconoscere i messaggi in fase di ricezione
    int senderId;                                       //id del mittente del messaggio
    int sequenceNum;                                    //sequence number associato al messaggio
    int vectorClock[NUM_NODES];                         //vector clock inviato nel messaggio
    String messageType;                                 //tipo di messaggio ("bid", "order")
} struct_message;

//Coda dei messaggi in attesa
std::vector<struct_message> holdBackQueueSeq;           // Hold-back queue Sequenziatore
std::vector<struct_message> holdBackQueuePart;          // Hold-back queue Partecipanti
std::vector<struct_message> holdBackQueueOrder;         // Hold-back queue messaggi di ordinamento da parte del sequenziatore
std::vector<struct_message> holdBackQueueCausal;        // Hold-back queue Deliver

struct_message auctionMessageToSend;                    //Messaggio da inviare
struct_message auctionMessageToReceive;                 //Messaggio ricevuto

esp_now_peer_info_t peerInfo; // Aggiunta dichiarazione della variabile peerInfo

// Funzione per iniziare l'asta
void startAuction(){
  highestBid = 0;
  winnerNodeId = -1;
  sequenceNumber = 0;
  auctionStarted = true;                                                            // metto a true l'inizio dell'asta
  restartTimer = millis();                                                           // leggo e salvo il tempo di inizio asta
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

    // Se sono il sequenziatore faccio una receive diversa
    if(myMacAddress == mac_sequencer){
      holdBackQueueSeq.push_back(auctionMessageToReceive);                              // Aggiungi il messaggio alla hold-back queue
      Serial.println("Messaggio aggiunto alla hold-back queue.");
      causalControl(auctionMessageToReceive);                                           // Controllo la causalità    
    }else{

      if(auctionMessageToReceive.messageType == "bid"){                                 // Se il messaggio è di tipo "bid"

        holdBackQueuePart.push_back(auctionMessageToReceive);                              // Aggiungi il messaggio alla hold-back queue
        Serial.println("Messaggio aggiunto alla hold-back queue.");
        causalControlPartecipant(auctionMessageToReceive);

      }else if(auctionMessageToReceive.messageType == "order"){
        if(auctionMessageToReceive.sequenceNum == sequenceNumber && checkCorrispondence(auctionMessageToReceive)){
                                      
          TO_Deliver(auctionMessageToReceive);
        }else{
          holdBackQueueOrder.push_back(auctionMessageToReceive);  
          Serial.println("Messaggio aggiunto alla hold-back queue.");
        } 
      }

    }
}

/**********************FUNZIONE DI SETUP**************************************/
void setup() {

  
  // Aggiungi alcune associazioni MAC address -> numero
  macToNumberMap["ac:15:18:e9:9f:4c"] = 0;
  macToNumberMap["f8:b3:b7:2c:71:80"] = 1;
  macToNumberMap["4c:11:ae:65:af:08"] = 2;
  macToNumberMap["f8:b3:b7:2c:71:80"] = 3;
  macToNumberMap["f8:b3:b7:2c:71:80"] = 4;


  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  myMacAddress = WiFi.macAddress();
  Serial.println("MAC Address: " + myMacAddress);

  myNodeId = macToNumberMap[myMacAddress];                                        // Assegno l'id del nodo in base al MAC address


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

  esp_now_register_recv_cb(esp_now_recv_cb_t(onDataReceive));                                             // registro la funzione "OnDataRecv()" come funzione di callback alla ricezione di un messagio

  buttonPressed = true;                                                             //sto simulando l'inizio di una sola asta
  buttonBid = true;                                                                 //sto simulando l'offerta di un solo nodo

}

/*************************FUNZIONE LOOP***************************************************/
void loop() {

  if(buttonPressed){                                                                 // Se è stato premuto il bottone di inizio asta
    buttonPressed = false;
    startAuction();                                                                  // setto le variabili iniziali, tra cui la variabile che segna l'inizio dell'asta
  }

  // Tutti se l'asta è iniziata
  if(auctionStarted){                                                               // finchè l'asta non è finita
    
    //parte sequeziatore
    if(myMacAddress == mac_sequencer){      
      checkEndAuction();                                                              // controllo se l'asta è finita
    }

    // Se bottone premuto per fare offerta
    if(buttonBid){

      // #########FARE DEBOUNCER##########
      buttonBid = false;
      sendBid();                                                                      // invio l'offerta

    }

  }

}

bool checkCorrispondence(struct_message messageToCheck){
  for (int i=0; i<holdBackQueueCausal.size(); i++) {
     if (messageToCheck.messageId == holdBackQueueCausal[i].messageId && messageToCheck.senderId == holdBackQueueCausal[i].senderId) {
       return true;
     }  
    }
  return false;
}

bool causalControl(struct_message messageToCheck){
  if((messageToCheck.vectorClock[messageToCheck.senderId] == vectorClock[messageToCheck.senderId] + 1) && isCausallyRead(messageToCheck)){
    holdBackQueueSeq.pop_back();                                                  // Rimuovo il messaggio dalla coda
    CO_Deliver(messageToCheck);                                                   // Invio il messaggio al livello superiore
    return true;
  }
  return false;
}

bool causalControlPartecipant(struct_message messageToCheck){
  if((messageToCheck.vectorClock[messageToCheck.senderId] == vectorClock[messageToCheck.senderId] + 1) && isCausallyRead(messageToCheck)){
    holdBackQueuePart.pop_back();                                                  // Rimuovo il messaggio dalla coda
    holdBackQueueCausal.push_back(messageToCheck);                                                  // Invio il messaggio al livello superiore
    CO_DeliverPartecipant(messageToCheck);
    // FAI PARTIRE L'ORDER DELIVER
    if(messageToCheck.sequenceNum == sequenceNumber && checkCorrispondence(messageToCheck)){                                
      TO_Deliver(messageToCheck);
    }
    return true;
  }
  return false;
}

void CO_DeliverPartecipant(struct_message message){
  vectorClock[message.senderId]++;
  retrieveMessagePartecipant();
}

void CO_Deliver(struct_message message){
  auctionMessageToSend = message;
  vectorClock[message.senderId]++;
  sendSequencer(message);
  retrieveMessage();
}

void TO_Deliver(struct_message message){
  auctionMessageToReceive = message; 
  int idToDelete = message.messageId;

  //elimino in entrambe le code, sia di ordinamento che in quella causale
  holdBackQueueOrder.pop_back();
  for (int i=0; i<holdBackQueueCausal.size(); i++) {
    if (idToDelete == holdBackQueueCausal[i].messageId) {
      holdBackQueueCausal.pop_back();
    }
  }

  //aggiorno il sequence number
  sequenceNumber++;
  Serial.println("Messaggio consegnato");
  Serial.println("Bid offerta " + String(auctionMessageToReceive.bid) + "da parte di " + String(auctionMessageToReceive.senderId));

} 

void retrieveMessagePartecipant(){
  for (auto it = holdBackQueuePart.begin(); it != holdBackQueuePart.end(); ) {
        struct_message msg = *it;
        if(causalControlPartecipant(msg)){
            Serial.println("Messaggio recuperato");
        } else {
            ++it;
        }
    }
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
      message.highestBid = highestBid;                                 // Aggiorno il messaggio con l'offerta più alta
    }
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &message, sizeof(message));
    message.sequenceNum = sequenceNumber++;

    restartTimer = millis();                                                // Aggiorno il timer di restart

    if (result == ESP_OK) {
        Serial.println("Offerta inviata di " + String(message.bid) + " da parte di " + String(message.senderId));
    } else {
        Serial.println("Errore nell'invio dell'offerta");
    }
}

void sendBid(){

  auctionMessageToSend.bid = highestBid+1;                                              // Setto il valore dell'offerta
  auctionMessageToSend.senderId = myNodeId;                                              // Setto il mittente
  auctionMessageToSend.messageId = auctionMessageToSend.messageId+1;                                     // 
  auctionMessageToSend.vectorClock[myNodeId] = vectorClock[myNodeId] + 1;                          
  auctionMessageToSend.messageType = "bid";

  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &auctionMessageToSend, sizeof(auctionMessageToSend));

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
