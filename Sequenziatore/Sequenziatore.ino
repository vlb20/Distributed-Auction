#include <esp_now.h>
#include <WiFi.h>
#include <vector>
#include <map>

#define DURATION_TIME 60000
#define NUM_NODES 5

// Indirizzo di broadcast per inviare a tutti i nodi
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
String mac_sequencer = "AC:15:18:E9:9F:4C";

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
bool buttonStartAuction = false;                             //simulazione bottone inizio asta
bool buttonBid = false;                                 //simulazione bottone offerta

// Struttura per messaggi
typedef struct struct_message {
    int bid = 0;                                            //bid dell'offerta nel messaggio
    int highestBid = 0;                                     //offerta più alta attuale
    int messageId = 0;                                      //id del messaggio, utile per riconoscere i messaggi in fase di ricezione
    int senderId = 0;                                       //id del mittente del messaggio
    int sequenceNum = 0;                                    //sequence number associato al messaggio
    int vectorClock[NUM_NODES] = {0,0,0,0,0};                         //vector clock inviato nel messaggio
    String messageType = "";                                 //tipo di messaggio ("bid", "order")
} struct_message;

//Coda dei messaggi in attesa
std::vector<struct_message> holdBackQueueSeq;           // Hold-back queue Sequenziatore
std::vector<struct_message> holdBackQueuePart;          // Hold-back queue Partecipanti
std::vector<struct_message> holdBackQueueOrder;         // Hold-back queue messaggi di ordinamento da parte del sequenziatore
std::vector<struct_message> holdBackQueueCausal;        // Hold-back queue Deliver

struct_message auctionMessageToSend;                    //Messaggio da inviare (bid)
struct_message auctionMessageToReceive;                 //Messaggio ricevuto

esp_now_peer_info_t peerInfo; // Aggiunta dichiarazione della variabile peerInfo

// Funzione per iniziare l'asta - SEQUENZIATORE
void startAuction(){
  highestBid = 0;
  winnerNodeId = -1;
  sequenceNumber = 0;
  auctionStarted = true;                                                            // metto a true l'inizio dell'asta
  restartTimer = millis();                                                           // leggo e salvo il tempo di inizio asta
  auctionEndTime = DURATION_TIME;
}


// Callback invio dati - TUTTI
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Quando invio un'offerta me la registro nella coda
  // Come se inviassi il messaggio a me stesso

  //------- DA CAPIRE SE SERVE, SE FACCIO RECEIVE SU I MIEI INVI QUESTO CODICE NON SERVE-----------
  Serial.println("OnDataSent!");

  if(myMacAddress == mac_sequencer && auctionMessageToSend.messageType == "bid"){
    holdBackQueueSeq.push_back(auctionMessageToSend);                                 // Se sono il sequenziatore pusho nella mia coda
    Serial.println("[Sequencer] Messaggio aggiunto alla holdBackQueue con:");
    Serial.println("SenderId: "+String(auctionMessageToSend.senderId));
    Serial.println("MessageId: "+String(auctionMessageToSend.messageId));
    Serial.println("Bid: "+String(auctionMessageToSend.bid));
    // Stampa il vector clock
    Serial.print("Vector Clock: [ ");
    for (int i = 0; i < NUM_NODES; i++) { // Usa NUM_NODES per la dimensione dinamica
      Serial.print(auctionMessageToSend.vectorClock[i]);
      if (i < NUM_NODES - 1) Serial.print(", "); // Aggiungi virgola tra i valori, ma non alla fine
    }
    Serial.println(" ]");

    processHoldBackQueue(holdBackQueueSeq, true); // Controllo la coda di messaggi

  }else if(myMacAddress != mac_sequencer){
    holdBackQueuePart.push_back(auctionMessageToSend);                                // Se sono un partecipante generico, pusho nella coda partecipanti
    Serial.println("[Partecipant] Messaggio aggiunto alla hold-back queue con:");
    Serial.println("SenderId: "+String(auctionMessageToSend.senderId));
    Serial.println("MessageId: "+String(auctionMessageToSend.messageId));
    Serial.println("Bid: "+String(auctionMessageToSend.bid));
    // Stampa il vector clock
    Serial.print("Vector Clock: [ ");
    for (int i = 0; i < NUM_NODES; i++) { // Usa NUM_NODES per la dimensione dinamica
      Serial.print(auctionMessageToSend.vectorClock[i]);
      if (i < NUM_NODES - 1) Serial.print(", "); // Aggiungi virgola tra i valori, ma non alla fine
    }
    Serial.println(" ]");

    processHoldBackQueue(holdBackQueuePart, false); // Controllo la coda di messaggi
  }

  //--------------ELIMINARE SE NON SERVE--------------------------------------------------------------------

    Serial.println("\r\nLast Packet Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Failed");
}

// Calback ricezione dati - SEQUENZIATORE
void onDataReceive(const uint8_t *mac, const uint8_t *incomingData, int len){
    memcpy(&auctionMessageToReceive, incomingData, sizeof(auctionMessageToReceive));

    Serial.println("OnDataReceive!");

    // Se sono il sequenziatore faccio una receive diversa
    if(myMacAddress == mac_sequencer){

      holdBackQueueSeq.push_back(auctionMessageToReceive);                              // Aggiungi il messaggio alla hold-back queue

      Serial.println("[Sequencer] Messaggio aggiunto alla holdBackQueue con:");
      Serial.println("SenderId: "+String(auctionMessageToReceive.senderId));
      Serial.println("MessageId: "+String(auctionMessageToReceive.messageId));
      Serial.println("Bid: "+String(auctionMessageToReceive.bid));
      // Stampa il vector clock
      Serial.print("Vector Clock: [ ");
      for (int i = 0; i < NUM_NODES; i++) { // Usa NUM_NODES per la dimensione dinamica
        Serial.print(auctionMessageToReceive.vectorClock[i]);
        if (i < NUM_NODES - 1) Serial.print(", "); // Aggiungi virgola tra i valori, ma non alla fine
      }
      Serial.println(" ]");

      processHoldBackQueue(holdBackQueueSeq, true); // Controllo la coda di messaggi

    }else{

      if(auctionMessageToReceive.messageType == "bid"){                                 // Se il messaggio è di tipo "bid"

        holdBackQueuePart.push_back(auctionMessageToReceive);                              // Aggiungi il messaggio alla hold-back queue
        Serial.println("[Partecipant] Messaggio aggiunto alla hold-back queue con:");
        Serial.println("SenderId: "+String(auctionMessageToReceive.senderId));
        Serial.println("MessageId: "+String(auctionMessageToReceive.messageId));
        Serial.println("Bid: "+String(auctionMessageToReceive.bid));
        // Stampa il vector clock
        Serial.print("Vector Clock: [ ");
        for (int i = 0; i < NUM_NODES; i++) { // Usa NUM_NODES per la dimensione dinamica
          Serial.print(auctionMessageToReceive.vectorClock[i]);
          if (i < NUM_NODES - 1) Serial.print(", "); // Aggiungi virgola tra i valori, ma non alla fine
        }
        Serial.println(" ]");

        // Quando mi arriva un messaggio controllo tutta la coda
        // Così a partire dall'ultimo vedo se è causale, e se il suo invio mi ha "sbloccato" altri in coda
        // Inoltre se durante il controllo faccio un erase di un messaggio causale, mi conviene ricominciare il ciclo
        // infatti è possibile che quelli dietro adesso siano causali e quindi sbloccabili

        processHoldBackQueue(holdBackQueuePart, false); // Controllo la coda di messaggi

      }else if(auctionMessageToReceive.messageType == "order"){
        Serial.println("[Partecipant] Arrivato un messaggio di ordinamento");
        Serial.println("SenderId: "+String(auctionMessageToReceive.senderId));
        Serial.println("MessageId: "+String(auctionMessageToReceive.messageId));
        Serial.println("Sequence Number: "+String(auctionMessageToReceive.sequenceNum));

        if(checkCorrispondence(auctionMessageToReceive,"fromOrderToCausal")){

          TO_Deliver(auctionMessageToReceive);
          Serial.println("[Partecipant] Ho fatto la TO Deliver");
        }else{
          holdBackQueueOrder.push_back(auctionMessageToReceive);
          Serial.println("[Partecipant] Messaggio aggiunto alla hold-back queue di ordinamento.");
        }
      }

    }
}

//Funzione per processare la hold-back queue
bool processHoldBackQueue(std::vector<struct_message> &holdBackQueue, bool isSequencer){
  bool checkPop = false;

  do{ // Ciclo finchè non ho eliminato tutti i messaggi sbloccati
    Serial.println((isSequencer ? "[Sequencer] " : "[Partecipant] ") + String("Nella mia coda ho ") + String(holdBackQueue.size()) + String(" messaggi"));
    checkPop = false;
    for (auto it = holdBackQueue.rbegin(); it != holdBackQueue.rend(); ){
      // Faccio il controllo per il messaggio in coda, ultimo posto (se è arrivato un messaggio è quello in ultima posizione)
      if(isSequencer){
        checkPop = causalControl(*it,it); // Controllo specifico per il sequenziatore
      }else{
        checkPop = causalControlPartecipant(*it,it); // Controllo specifico per i partecipanti
      }

      Serial.println((isSequencer ? "[Sequencer] " : "[Partecipant] ") + String("checkPop: ") + String(checkPop));
      if(checkPop){ // se ho sbloccato un messaggio...
        Serial.println((isSequencer ? "[Sequencer] " : "[Partecipant] ") + String("Il pop era true, ricomnicio il ciclo"));
        break;      // esco dal ciclo per ricominciare dalla fine della coda
      }
      ++it;           // Incremento l'iteratore per scorrere la coda
    }
  }while(checkPop);

  return checkPop;
}


// DA RENDERE POLIMORFA
// Quando arriva un messaggio di ordinamento, devo controllare se c'è il corrispettivo nella holdBackQueueCausal
// Quando arriva un messaggio causale di cui ho fatto CO-Deliver, devo controllare se c'è il corrispettivo nella holdBackQueueOrder
bool checkCorrispondence(struct_message messageToCheck, String corrispondenceType){

  if (corrispondenceType == "fromCausalToOrder"){
    Serial.println("[Partecipant] Controllo corrispondenza tra i messaggi nella coda dei messaggi di Ordinamento");
    Serial.println("[Partecipant] La coda di attesa di ordinamento ha "+String(holdBackQueueOrder.size())+" messaggi");

    try{
    for (int i=0; i<holdBackQueueOrder.size(); i++) {
      if (messageToCheck.messageId == holdBackQueueOrder[i].messageId && messageToCheck.senderId == holdBackQueueOrder[i].senderId && sequenceNumber == holdBackQueueOrder[i].sequenceNum ) {
        return true;
      }
    }
    return false;
    }catch(const std::out_of_range& e){
      Serial.println("[Partecipant] Errore nella funzione checkCorrispondence");
    }
  }else if(corrispondenceType == "fromOrderToCausal"){
    Serial.println("[Partecipant] Controllo corrispondenza tra i messaggi nella coda dei messaggi Causali");
    Serial.println("[Partecipant] La coda di attesa dei causali ha "+String(holdBackQueueCausal.size())+" messaggi");

    try{
    for (int i=0; i<holdBackQueueCausal.size(); i++) {
      if (messageToCheck.messageId == holdBackQueueCausal[i].messageId && messageToCheck.senderId == holdBackQueueCausal[i].senderId && sequenceNumber == messageToCheck.sequenceNum ) {
        return true;
      }
    }
    return false;
    }catch(const std::out_of_range& e){
      Serial.println("[Partecipant] Errore nella funzione checkCorrispondence");
    }
  }
}

bool causalControl(struct_message messageToCheck, std::vector<struct_message>::reverse_iterator it){
  if((messageToCheck.vectorClock[messageToCheck.senderId] == vectorClock[messageToCheck.senderId] + 1) && isCausallyRead(messageToCheck)){
    Serial.println("[Sequencer] Messaggio causale da parte di "+String(messageToCheck.senderId)+" con offerta "+String(messageToCheck.bid)+" sbloccato");
    holdBackQueueSeq.erase(it.base());                                                                  // Rimuovo il messaggio dalla coda
    Serial.println("[Sequencer] Ho eliminato questo messaggio causale");
    CO_Deliver(messageToCheck);                                                                   // Invio il messaggio al livello superiore
    return true;
  }
  return false;
}

bool causalControlPartecipant(struct_message messageToCheck, std::vector<struct_message>::reverse_iterator it){ //TUTTI
  if((messageToCheck.vectorClock[messageToCheck.senderId] == vectorClock[messageToCheck.senderId] + 1) && isCausallyRead(messageToCheck)){
  Serial.println("[Partecipant] Messaggio causale da parte di "+String(messageToCheck.senderId)+" con offerta "+String(messageToCheck.bid)+" sbloccato");
    holdBackQueuePart.erase(it.base());                                                                 // Rimuovo il messaggio dalla coda
    Serial.println("[Partecipant] Ho eliminato questo messaggio causale");
    holdBackQueueCausal.push_back(messageToCheck);                                                // Invio il messaggio al livello superiore
    Serial.println("[Partecipant] Ho aggiunto il messaggio alla seconda coda di attesa, aspetto mess di ordinamento");
    CO_DeliverPartecipant(messageToCheck);
    Serial.println("[Partecipant] Ho fatto CO Deliver, aggiornato il vector clock");
    // FAI PARTIRE L'ORDER DELIVER
    if(checkCorrispondence(messageToCheck,"fromCausalToOrder")){
      Serial.println("[Partecipant] ho ordinamento e causale, posso fare la TO Deliver");
      TO_Deliver(messageToCheck);
    }
    return true;
  }
  return false;
}

void CO_DeliverPartecipant(struct_message message){
  vectorClock[message.senderId]++;
}

// CO-Deliver del sequenziatore [SEQUENZIATORE]
void CO_Deliver(struct_message message){
  auctionMessageToSend = message;                 // Oltre ad aggiornare il vector clock, devo anche inviare a tutti un mess di ordinamento
  vectorClock[message.senderId]++;                // Aggiorno il vector clock alla mia posizione
  sendSequencer(auctionMessageToSend);                         // Invio il messaggio di ordinamento
}

void TO_Deliver(struct_message message){
  auctionMessageToReceive = message;
  int idToDelete = message.messageId;
  int senderIdToDelete = message.senderId;

  //elimino in entrambe le code, sia di ordinamento che in quella causale
  for(int i=0; i<holdBackQueueOrder.size(); i++){
    if(holdBackQueueOrder[i].messageId == idToDelete && holdBackQueueOrder[i].senderId == senderIdToDelete){
      holdBackQueueOrder.erase(holdBackQueueOrder.begin()+i);
    }
  }

  for(int i=0; i<holdBackQueueCausal.size(); i++){
    if(holdBackQueueCausal[i].messageId == idToDelete && holdBackQueueCausal[i].senderId == senderIdToDelete){
      holdBackQueueCausal.erase(holdBackQueueCausal.begin()+i);
    }
  }

  //aggiorno il sequence number
  sequenceNumber++;
  Serial.println("[Partecipant] Messaggio consegnato");
  Serial.println("[Partecipant] Bid offerta " + String(auctionMessageToReceive.bid) + "da parte di " + String(auctionMessageToReceive.senderId));
  Serial.println("[Partecipant] Il mio Sequence Number ora è " + String(sequenceNumber));

}

void sendSequencer(struct_message message) {

    auctionMessageToSend = message;
    auctionMessageToSend.messageType = "order";                                             // Setto il tipo di messaggio

    // Controllo se la Highest Bid è cambiata
    if(auctionMessageToSend.bid > highestBid){                                      // Se la bid che ho prelevato è più grande...
      highestBid = auctionMessageToSend.bid;                                        // Aggiorno l'offerta più alta la momento
      winnerNodeId = auctionMessageToSend.senderId;                                 // e il vincitore momentaneo
      auctionMessageToSend.highestBid = highestBid;                                 // Aggiorno il messaggio con l'offerta più alta
    }

    auctionMessageToSend.sequenceNum = sequenceNumber;
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &auctionMessageToSend, sizeof(auctionMessageToSend));
    sequenceNumber++;

    restartTimer = millis();                                                // Aggiorno il timer di restart

    if (result == ESP_OK) {
        Serial.println("[Sequencer] Offerta inviata di " + String(auctionMessageToSend.bid) + " da parte di " + String(auctionMessageToSend.senderId));
    } else {
        Serial.println("[Sequencer] Errore nell'invio del messaggio di ordinamento");
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
      Serial.println("[Partecipante] Messaggio inviato con successo con bid: "+String(auctionMessageToSend.bid)+" da "+String(auctionMessageToSend.senderId));
  } else {
      Serial.println("[Partecipante] Errore nell'invio del messaggio da parte di "+String(auctionMessageToSend.senderId));
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
  Serial.println("Ha vinto il nodo " + String(winnerNodeId));                      // Annuncio il vincitore
    Serial.println("con un offerta di " + String(highestBid));
  }

}

/**********************FUNZIONE DI SETUP**************************************/
void setup() {


  // Aggiungi alcune associazioni MAC address -> numero
  macToNumberMap["AC:15:18:E9:9F:4C"] = 0;
  macToNumberMap["F8:B3:B7:2C:71:80"] = 1;
  macToNumberMap["4C:11:AE:65:AF:08"] = 2;
  macToNumberMap["F8:B3:B7:2C:71:80"] = 3;
  macToNumberMap["F8:B3:B7:2C:71:80"] = 4;


  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  delay(2000);
  myMacAddress = WiFi.macAddress();
  Serial.println("MAC Address: " + myMacAddress);

  myNodeId = macToNumberMap[myMacAddress];                                        // Assegno l'id del nodo in base al MAC address


  if (esp_now_init() != ESP_OK) {                                                   // Se la connesione esp non è andata a buon fine
        Serial.println("Error initializing ESP-NOW");                               // lo segnalo e termino il programma
        return;
  }

  esp_now_register_send_cb(OnDataSent);                                             // registro la funzione "OnDataSent()" come funzione di callback all'invio di un messagio

  memcpy(peerInfo.peer_addr, broadcastAddress, 6);                                  // copio le informazione dei peer nelle locazioni dei peer address
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        return;
  }

  esp_now_register_recv_cb(esp_now_recv_cb_t(onDataReceive));                                             // registro la funzione "OnDataRecv()" come funzione di callback alla ricezione di un messagio

  buttonStartAuction = true;                                                             //sto simulando l'inizio di una sola asta
  buttonBid = true;                                                                 //sto simulando l'offerta di un solo nodo
  delay(5000);

}

/*************************FUNZIONE LOOP***************************************************/
void loop() {

  if(buttonStartAuction){                                                                 // Se è stato premuto il bottone di inizio asta
    buttonStartAuction = false;
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
      delay(3000);
      sendBid();                                                                      // invio l'offerta

    }

  }

}

