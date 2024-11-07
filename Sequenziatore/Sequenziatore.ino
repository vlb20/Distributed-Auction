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
int sequenceNumber = 0; //s_g
int vectorClock[NUM_NODES] = {0,0,0,0,0};
int highestBid = 0
int winnerNodeId = -1
unsigned long auctionEndTime = 0;
bool auctionStarted = false; // Flag per tracciare se l'asta è partita

//Coda dei messaggi in attesa
std::vector<struct_message> holdBackQueue; // Hold-back queue

// Mutex per proteggere l’accesso alla hold-back queue
SemaphoreHandle_t mutex;

// Struttura per messaggi
typedef struct struct_message {
    int bid;
    int messageId;
    String senderId;
    int sequenceNum;
    int vectorClock[NUM_NODES];
} struct_message;

struct_message auctionMessageToSend; // Messaggio da inviare
struct_message auctionMessageToReceive; // Messaggio ricevuto


/*******INIZIO FUNZIONI THREAD*******/

// Funzione per verificare la causalità
bool compareVectorClock(const struct_message &a, const struct_message &b) {
    bool atLeastOneStrictlySmaller = false;
    for (int i = 0; i < NUM_NODES; i++) {
        if (a.vectorClock[i] > b.vectorClock[i]){
            return false;
        }
        if (a.vectorClock[i] < b.vectorClock[i]){
          atLeastOneStrictlySmaller = true;
        } 
    }
    return atLeastOneStrictlySmaller;
}

void sortTask(void *parameter){
  
  while(true){
    
    // Acquisisci il mutex per ordinare la coda
    xSemaphoreTake(mutex, portMAX_DELAY);

    if(!holdBackQueue.empty() && holdBackQueue.size > 1) {
        // Ordina la hold-back queue in base al vector clock
        std::sort(holdBackQueue.begin(), holdBackQueue.end(), compareVectorClock);

        Serial.println("Hold-back queue ordinata.");
    }

    xSemaphoreGive(mutex); // Rilascia il mutex dopo l’ordinamento

    // Delay per evitare un ciclo troppo veloce
    vTaskDelay(100 / portTICK_PERIOD_MS);

  }

}


/*******FINE FUNZIONI THREAD*******/

// Funzione per iniziare l'asta
void startAuction(){
  highestBid = 0;
  winnerNodeId = -1;
  sequenceNumber = 0;
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
    
    // Blocca il mutex prima di accedere al vettore condiviso
    xSemaphoreTake(mutex, portMAX_DELAY);
    holdBackQueue.push_back(auctionMessageToReceive)
    xSemaphoreGive(mutex); //Rilascia il mutex dopo l'accesso

    Serial.println("Messaggio aggiunto alla hold-back queue.");
}

void setup() {

  Serial.begin(115200);
  Wifi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
  }

  esp_now_register_send_cb(OnDataSent);

  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        return;
  }
  esp_now_register_recv_cb(OnDataRecv);

  // Crea il mutex per la coda condivisa
  mutex = xSemaphoreCreateMutex();

  // Crea i task per ricezione e ordinamento
  xTaskCreate(sortTask, "Sort Task", 2048, NULL, 1, NULL);

}

void loop() {
  
  //if bottone premuto
    //auctionStarted = True
    startAuction()

  //if auctionStarted == True
    checkForIncomingMessages();();

}

// Funzione principale
void checkForIncomingMessages(){

  //Aggiorno variabili
  sequenceNumber++;

  
  int vectorClock[NUM_NODES] = {0,0,0,0,0};
  int highestBid = 0
  int winnerNodeId = -1
  unsigned long auctionEndTime = 0;
  //Invio

}
