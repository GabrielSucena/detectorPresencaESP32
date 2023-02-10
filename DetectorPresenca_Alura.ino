// --- WIFI ---
#include <WiFi.h>
const char* ssid     = ""; // Nome da rede
const char* password = ""; // Senha da rede
WiFiClient esp32Client;

// --- MQTT ---
#include <PubSubClient.h>
const char* mqtt_Broker = "mqtt.eclipseprojects.io";
const char* mqtt_ClientID = "esp32-01";
PubSubClient client(esp32Client);
const char* mqtt_topico_controle = "lab-sucena/iluminacao";
const char* mqtt_topico_monitoracao = "lab-sucena/monitoracao";
boolean statusMonitor = false;

// --- Bluetooth ---
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
int scanTime = 5; //In seconds
int nivelRSSI = -78; // Fazer teste para saber a ditancia ideal
String dispositivosAutorizados = ""; // MAC DISPOSITIVO
bool dispositivoPresente = false;
unsigned long ultimoTempoMedido = 0;
const long intervaloPublicacao = 20000; // Ajustar o tempo de desligamento

void setup() {
  Serial.begin(115200);
  conectaWifi();
  client.setServer(mqtt_Broker, 1883);
  client.setCallback(monitoraTopico);
  Serial.println("Scanning...");
  BLEDevice::init("");
}

// --- Funções auxiliares ---
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      String dispositivosEncontrados = advertisedDevice.getAddress().toString().c_str();
      if (dispositivosEncontrados == dispositivosAutorizados && advertisedDevice.getRSSI() > nivelRSSI){
        Serial.print("Identificador DETECTADO!");
        Serial.print("RSSI: ");
        Serial.println(advertisedDevice.getRSSI());
        dispositivoPresente = true;
        ultimoTempoMedido = millis();
      }
      else{
        dispositivoPresente = false;
      }
    }
};

// --- Scan Bluetooth LE ---
void scanBLE(){
  BLEScan* pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  BLEScanResults foundDevices = pBLEScan->start(scanTime);
}

// --- Conecta WIFI ---
void conectaWifi(){
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

// --- Conecta ao MQTT ---
void conectaMQTT(){
  while(!client.connected()){
    client.connect(mqtt_ClientID);
    client.subscribe(mqtt_topico_monitoracao);
  }
}

// --- Monitora o Tópico lab-sucena/monitoracao ---
void monitoraTopico(char* mqtt_topico_monitoracao, byte* payload, unsigned int length){
  if((char)payload[0] == '1'){
    statusMonitor = 1;
  }
  else{
    statusMonitor = 0;
  }
}

// --- Desabilita o scan ---
void desabilitaScan(){
  if(statusMonitor == 0){
    Serial.println("Scan ATIVO");
    scanBLE();
    publicaStatusNoTopico();
  }
  else{
    Serial.println("Scan DESLIGADO");
    dispositivoPresente = true;
    publicaStatusNoTopico();
  }
}

// --- Publica no tópico iluminação (liga/desliga o dispositivo) ---
void publicaStatusNoTopico(){
  if(dispositivoPresente == 1){
    client.publish(mqtt_topico_controle, String("on").c_str(), true);
    Serial.println("Power ON");
  }
  else{
    if(millis() - ultimoTempoMedido > intervaloPublicacao){
      client.publish(mqtt_topico_controle, String("off").c_str(), true);
      Serial.println("Power OFF");
    }
  }
}

void loop() {
  if (!client.connected()){
    conectaMQTT();
  }
  client.loop(); // Faz a chamada do callback
  desabilitaScan();
  delay(2000);
}
