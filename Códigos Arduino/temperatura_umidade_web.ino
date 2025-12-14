/*
  FUNCIONALIDADES:
  1. Web Server Local: Mostra temperatura.
  2. Firebase Logger: Envia Temp/Umidade a cada 10s para /sensores/sala.
*/

#include <DHT.h>

// ===============================================================
// 1. CONFIGURAÇÕES WIFI E FIREBASE
// ===============================================================

const String SSID_REDE  = "WIFI_UFSJ";
const String SENHA_REDE = ""; 

const String FIREBASE_HOST = "arduino-1bd32-default-rtdb.firebaseio.com";

const String FIREBASE_PATH = "/sensores/sala.json"; 

const unsigned long INTERVALO_ENVIO = 10000; 
unsigned long timerEnvio = 0;

// ===============================================================
// 2. HARDWARE E PINOS
// ===============================================================
#define DHTPIN   2
#define DHTTYPE  DHT22
DHT dht(DHTPIN, DHTTYPE);

// ===============================================================
// 3. SETUP
// ===============================================================
void setup() {
  Serial.begin(115200);   
  Serial1.begin(115200);  

  dht.begin();

  Serial.println(F("\n=============================================="));
  Serial.println(F("   SISTEMA DE MONITORAMENTO IOT (SEM IR)      "));
  Serial.println(F("=============================================="));

  iniciarWifi();
}

// ===============================================================
// 4. LOOP PRINCIPAL
// ===============================================================
void loop() {
  
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'T' || c == 't') imprimirClimaSerial();
  }
  
  if (Serial1.available()) {
    if (Serial1.find("+IPD,")) {
      delay(30); 
      int id = Serial1.read() - '0';
      processarWeb(id);
    }
  }

  if (millis() - timerEnvio > INTERVALO_ENVIO) {
    enviarFirebaseClima();
    timerEnvio = millis();
  }
}

// ===============================================================
// 5. LÓGICA FIREBASE (PATCH)
// ===============================================================

void enviarFirebaseClima() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println(F("[Erro] DHT nao respondeu. Ignorando envio."));
    return;
  }

  Serial.println(F("\n--- FIREBASE: Atualizando dados (PATCH) ---"));

  String json = "{";
  json += "\"temperatura\":" + String(t) + ",";
  json += "\"umidade\":" + String(h);
  json += "}";

  while(Serial1.available()) Serial1.read();

  String cmdConexao = "AT+CIPSTART=4,\"SSL\",\"" + FIREBASE_HOST + "\",443";
  Serial1.println(cmdConexao);

  long t0 = millis();
  bool conectado = false;
  while(millis() - t0 < 4000) {
    String resp = Serial1.readStringUntil('\n');
    if(resp.indexOf("CONNECT") != -1 || resp.indexOf("OK") != -1 || resp.indexOf("ALREADY") != -1) { 
      conectado = true; break; 
    }
  }

  if(!conectado) {
    Serial.println(F("Falha: Nao foi possivel conectar ao Firebase."));
    Serial1.println("AT+CIPCLOSE=4");
    return;
  }

  String headers = "PATCH " + FIREBASE_PATH + " HTTP/1.1\r\n"; 
  headers += "Host: " + FIREBASE_HOST + "\r\n";
  headers += "User-Agent: ArduinoMega\r\n";
  headers += "Content-Type: application/json\r\n";
  headers += "Content-Length: " + String(json.length()) + "\r\n";
  headers += "Connection: close\r\n\r\n";
  
  int tamanhoTotal = headers.length() + json.length();

  Serial1.print("AT+CIPSEND=4,");
  Serial1.println(tamanhoTotal);
  
  delay(200);

  Serial1.print(headers);
  Serial1.print(json);

  Serial.println(F("Pacote enviado! Aguardando confirmacao..."));

  t0 = millis();
  while(millis() - t0 < 2000) {
    if(Serial1.available()) {
      String line = Serial1.readStringUntil('\n');
      if(line.indexOf("200 OK") != -1) {
          Serial.println(F(">> SUCESSO: Firebase respondeu 200 OK."));
          break;
      }
    }
  }
  
  Serial1.println("AT+CIPCLOSE=4");
}

// ===============================================================
// 6. WIFI E WEB SERVER
// ===============================================================
void iniciarWifi() {
  Serial.println(F("[WiFi] Inicializando ESP-01..."));
  
  sendCMD("AT+RST", 3000); 
  sendCMD("AT+CWMODE=1", 500); 
  sendCMD("AT+CIPSSLSIZE=4096", 500); 
  
  String cmdWifi = "AT+CWJAP=\"" + SSID_REDE + "\",\"" + SENHA_REDE + "\"";
  Serial1.println(cmdWifi);
  
  Serial.print("Conectando WiFi");
  for(int i=0; i<20; i++) { 
    if(Serial1.available()) {
      String r = Serial1.readString();
      if(r.indexOf("OK") != -1 || r.indexOf("CONNECTED") != -1) break;
    }
    Serial.print(".");
    delay(500);
  }
  Serial.println("");

  Serial1.println("AT+CIFSR"); 
  delay(1000);
  
  sendCMD("AT+CIPMUX=1", 1000);
  sendCMD("AT+CIPSERVER=1,80", 1000);
  
  Serial.println(F("[WiFi] Configurado. IP Mostrado acima."));
}

void processarWeb(int idConexao) {
  String req = lerSerial1(300);
  
  if (req.indexOf("GET /LIGAR") != -1) {
    Serial.println(F("Web: Solicitacao LIGAR recebida (Acao ignorada: Sem IR)"));
  }
  else if (req.indexOf("GET /DESLIGAR") != -1) {
    Serial.println(F("Web: Solicitacao DESLIGAR recebida (Acao ignorada: Sem IR)"));
  }

  float t = dht.readTemperature();
  
  String html = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
  html += "<html><head><meta name='viewport' content='width=device-width'>";
  html += "<style>button{width:100%;padding:15px;margin:10px 0;font-size:18px;border:none;border-radius:10px;color:white;}";
  html += ".btn-on{background-color:#28a745;} .btn-off{background-color:#dc3545;}</style></head>";
  html += "<body style='font-family:sans-serif;text-align:center;padding:20px;'>";
  html += "<h2>Monitor Elgin (Sem IR)</h2>";
  
  if (isnan(t)) html += "<p>Temp: --</p>";
  else html += "<p style='font-size:24px'>Temp: <b>" + String(t, 1) + " C</b></p>";
  
  html += "<a href='/LIGAR'><button class='btn-on'>LIGAR (Log Only)</button></a>";
  html += "<a href='/DESLIGAR'><button class='btn-off'>DESLIGAR (Log Only)</button></a>";
  html += "<p><a href='/'>Atualizar</a></p></body></html>";

  String cmd = "AT+CIPSEND=" + String(idConexao) + "," + String(html.length());
  sendCMD(cmd, 200);
  Serial1.print(html);
  delay(50);
  sendCMD("AT+CIPCLOSE=" + String(idConexao), 100);
}

// ===============================================================
// 7. FUNÇÕES AUXILIARES
// ===============================================================

void imprimirClimaSerial() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  Serial.print(F("Temp: ")); Serial.print(t);
  Serial.print(F(" C | Umid: ")); Serial.print(h); Serial.println(F(" %"));
}

void sendCMD(const String &cmd, int waitMs) {
  Serial1.println(cmd);
  long t0 = millis();
  while (millis() - t0 < waitMs) { while(Serial1.available()) Serial1.read(); }
}

String lerSerial1(int timeoutMs) {
  String r = "";
  long t0 = millis();
  while (millis() - t0 < timeoutMs) { while(Serial1.available()) r += (char)Serial1.read(); }
  return r;
}