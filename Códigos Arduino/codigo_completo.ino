#include <DHT.h>
#include <IRremote.hpp>
#include <ctype.h>   

// ===============================================================
// 1. CONFIGURAÇÕES GERAIS
// ===============================================================

const String SSID_REDE  = "DCOMP_UFSJ";
const String SENHA_REDE = "";   

String IP_LOCAL = "";

const char *FIREBASE_HOST     = "arduino-1bd32-default-rtdb.firebaseio.com";
const int   FIREBASE_PORT     = 80;
const int   FIREBASE_LINK_ID  = 4;              
const unsigned long INTERVALO_FIREBASE_MS = 60000UL;  
unsigned long ultimoEnvioClima = 0;

#define DHTPIN   2
#define DHTTYPE  DHT22
DHT dht(DHTPIN, DHTTYPE);

#define IR_RECEIVE_PIN 11
#define IR_SEND_PIN    9

#define MAX_COMANDOS_IR 10

struct ComandoIR {
  char     nome[16];     
  bool     ativo;        
  uint8_t  protocolo;    
  uint16_t endereco;
  uint16_t comando;
  uint32_t rawData;      
};

ComandoIR listaComandos[MAX_COMANDOS_IR];

decode_type_t ultimoProtocolo = UNKNOWN;
uint16_t      ultimoEndereco  = 0;
uint16_t      ultimoComando   = 0;
uint32_t      ultimoRawData   = 0;
bool          ultimoValido    = false;

// (Opcional) comandos fixos pré-carregados
const ComandoIR COMANDOS_FIXOS[] = {
  // {"LigarTV", true, SAMSUNG, 0x7, 0x2, 0xFD020707}
};
const int NUM_COMANDOS_FIXOS = sizeof(COMANDOS_FIXOS) / sizeof(COMANDOS_FIXOS[0]);

// ===============================================================
// 2. PROTÓTIPOS
// ===============================================================

void handleIRRecepcao();
void salvarNovoComando(const String &nome);
void executarComandoIR(int id);
void deletarComando(int id);
void limparTodosComandos();
void mostrarMenuTerminal();
void verificarTerminal();
void carregarComandosFixos();

float lerTemperatura();
float lerUmidade();

void iniciarWifi();
String lerSerial1(int timeoutMs);
void processarWeb(int idConexao);
void enviarHTML(int idConexao);
void sendCMD(const String &cmd, int waitMs);

void enviarClimaFirebase();
void sincronizarComandosFirebase();
void enviarComandoFirebase(int slot);
void removerComandoFirebase(int slot);
bool firebaseHttpRequest(const String &method, const String &path, const String &body, String &response);
bool extrairCampoInt(const String &json, const char *campo, long &out);
bool extrairCampoULong(const String &json, const char *campo, unsigned long &out);
bool extrairCampoString(const String &json, const char *campo, String &out);

void sendCMDSilent(const String &cmd, int waitMs);

// ===============================================================
// 3. SETUP
// ===============================================================
void setup() {
  Serial.begin(9600);     
  Serial1.begin(115200);  

  dht.begin();

  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
  IrSender.begin(IR_SEND_PIN); 

  memset(listaComandos, 0, sizeof(listaComandos));
  ultimoProtocolo = UNKNOWN;
  ultimoEndereco  = 0;
  ultimoComando   = 0;
  ultimoRawData   = 0;
  ultimoValido    = false;

  carregarComandosFixos();

  Serial.println(F("\n========================================"));
  Serial.println(F("   CENTRAL IR + CLIMA (Samsung/NEC/AC)  "));
  Serial.println(F("========================================"));

  mostrarMenuTerminal();
  iniciarWifi();

  enviarClimaFirebase();
  sincronizarComandosFirebase();
  ultimoEnvioClima = millis();
}

// ===============================================================
// 4. LOOP
// ===============================================================
void loop() {
  handleIRRecepcao();

  if (Serial1.available()) {
    if (Serial1.find("+IPD,")) {
      delay(30);
      int idConexao = Serial1.read() - '0'; 
      processarWeb(idConexao);
    }
  }

  verificarTerminal();

  unsigned long agora = millis();
  if (agora - ultimoEnvioClima >= INTERVALO_FIREBASE_MS) {
    ultimoEnvioClima = agora;
    enviarClimaFirebase();   
  }
}

// ===============================================================
// 5. MÓDULO IR - RECEPÇÃO E ARMAZENAMENTO
// ===============================================================
void handleIRRecepcao() {
  if (!IrReceiver.decode()) return;

  IRData &data = IrReceiver.decodedIRData;

  if (data.flags & IRDATA_FLAGS_WAS_OVERFLOW) {
    Serial.println(F("\n[IR] Overflow: sinal muito longo (possivel ar-condicionado)."));
    IrReceiver.resume();
    return;
  }

  if (data.protocol == UNKNOWN &&
      data.address  == 0 &&
      data.command  == 0 &&
      data.decodedRawData == 0) {
    IrReceiver.resume();
    return;
  }

  if (!(data.flags & IRDATA_FLAGS_IS_REPEAT)) {
    ultimoProtocolo = data.protocol;
    ultimoEndereco  = data.address;
    ultimoComando   = data.command;
    ultimoRawData   = data.decodedRawData;
    ultimoValido    = true;

    Serial.println(F("\n--- SINAL IR RECEBIDO ---"));
    Serial.print(F("Protocolo: "));    Serial.println((int)ultimoProtocolo);
    Serial.print(F("Endereco:  0x"));  Serial.println(ultimoEndereco, HEX);
    Serial.print(F("Comando:   0x"));  Serial.println(ultimoComando, HEX);
    Serial.print(F("RawData:   0x"));  Serial.println(ultimoRawData, HEX);
    Serial.println(F("--------------------------"));
    Serial.println(F("Digite 'S' no Serial para salvar este sinal."));
  }

  IrReceiver.resume();
}

void salvarNovoComando(const String &nome) {
  if (!ultimoValido) {
    Serial.println(F("Nenhum sinal valido capturado ainda. Aponte o controle e aperte um botao."));
    return;
  }

  if (ultimoProtocolo == UNKNOWN &&
      ultimoEndereco  == 0 &&
      ultimoComando   == 0 &&
      ultimoRawData   == 0) {
    Serial.println(F("Sinal tudo-zero detectado (UNKNOWN). Nao vou salvar isso."));
    ultimoValido = false;
    return;
  }

  int slot = -1;
  for (int i = 0; i < MAX_COMANDOS_IR; i++) {
    if (!listaComandos[i].ativo) {
      slot = i;
      break;
    }
  }

  if (slot == -1) {
    Serial.println(F("Lista de comandos cheia."));
    return;
  }

  Serial.println(F("Salvando comando no slot..."));
  Serial.print(F("  Protocolo: ")); Serial.println((int)ultimoProtocolo);
  Serial.print(F("  Endereco:  0x")); Serial.println(ultimoEndereco, HEX);
  Serial.print(F("  Comando:   0x")); Serial.println(ultimoComando, HEX);
  Serial.print(F("  RawData:   0x")); Serial.println(ultimoRawData, HEX);

  listaComandos[slot].ativo     = true;
  listaComandos[slot].protocolo = (uint8_t)ultimoProtocolo;
  listaComandos[slot].endereco  = ultimoEndereco;
  listaComandos[slot].comando   = ultimoComando;
  listaComandos[slot].rawData   = ultimoRawData;

  nome.toCharArray(listaComandos[slot].nome, sizeof(listaComandos[slot].nome));
  listaComandos[slot].nome[sizeof(listaComandos[slot].nome) - 1] = '\0';

  Serial.print(F("Comando salvo no slot "));
  Serial.println(slot);

  enviarComandoFirebase(slot);

  mostrarMenuTerminal();
}

void executarComandoIR(int id) {
  if (id < 0 || id >= MAX_COMANDOS_IR || !listaComandos[id].ativo) {
    Serial.println(F("Slot vazio ou invalido."));
    return;
  }

  ComandoIR &cmd = listaComandos[id];
  decode_type_t proto = (decode_type_t)cmd.protocolo;

  Serial.println(F("\n--- ENVIANDO COMANDO IR ---"));
  Serial.print(F("Slot: "));       Serial.println(id);
  Serial.print(F("Nome: "));       Serial.println(cmd.nome);
  Serial.print(F("Protocolo: "));  Serial.println((int)proto);
  Serial.print(F("Endereco:  0x")); Serial.println(cmd.endereco, HEX);
  Serial.print(F("Comando:   0x")); Serial.println(cmd.comando, HEX);
  Serial.print(F("RawData:   0x")); Serial.println(cmd.rawData, HEX);
  Serial.println(F("--------------------------------"));

  if (proto == UNKNOWN) {
    Serial.println(F("Protocolo UNKNOWN (raw) ainda nao tratado neste codigo."));
    return;
  }

  uint8_t repeats = 2;
  IrSender.write(proto, cmd.endereco, cmd.comando, repeats);

  IrReceiver.enableIRIn();
  unsigned long t0 = millis();
  bool eco = false;

  while (millis() - t0 < 150) {
    if (IrReceiver.decode()) {
      eco = true;
      IRData &ecoData = IrReceiver.decodedIRData;
      Serial.println(F("[IR] Eco detectado pelo receptor (debug):"));
      Serial.print(F("      Proto=")); Serial.println((int)ecoData.protocol);
      Serial.print(F("      Addr=0x")); Serial.println(ecoData.address, HEX);
      Serial.print(F("      Cmd =0x")); Serial.println(ecoData.command, HEX);
      Serial.print(F("      Raw =0x")); Serial.println(ecoData.decodedRawData, HEX);
      IrReceiver.resume();
      break;
    }
  }

  if (!eco) {
    Serial.println(F("[IR] Nenhum eco detectado (normal se emissor e receptor nao estiverem alinhados)."));
  }
}

void deletarComando(int id) {
  if (id < 0 || id >= MAX_COMANDOS_IR) return;

  listaComandos[id].ativo     = false;
  listaComandos[id].protocolo = 0;
  listaComandos[id].endereco  = 0;
  listaComandos[id].comando   = 0;
  listaComandos[id].rawData   = 0;
  listaComandos[id].nome[0]   = '\0';

  Serial.print(F("Comando do slot "));
  Serial.print(id);
  Serial.println(F(" apagado."));

  removerComandoFirebase(id);

  mostrarMenuTerminal();
}

void limparTodosComandos() {
  for (int i = 0; i < MAX_COMANDOS_IR; i++) {
    removerComandoFirebase(i);
  }

  memset(listaComandos, 0, sizeof(listaComandos));
  Serial.println(F("Todos os comandos foram apagados (local e Firebase)."));
  mostrarMenuTerminal();
}

void carregarComandosFixos() {
  for (int i = 0; i < NUM_COMANDOS_FIXOS && i < MAX_COMANDOS_IR; i++) {
    listaComandos[i] = COMANDOS_FIXOS[i];
  }
}

// ===============================================================
// 6. MENU SERIAL
// ===============================================================
void mostrarMenuTerminal() {
  Serial.println(F("\n===== MENU IR / WEB / FIREBASE ====="));
  Serial.println(F("Slots de comandos IR:"));
  for (int i = 0; i < MAX_COMANDOS_IR; i++) {
    if (listaComandos[i].ativo) {
      Serial.print(F(" ["));
      Serial.print(i);
      Serial.print(F("] "));
      Serial.print(listaComandos[i].nome);
      Serial.print(F("  proto="));
      Serial.print((int)listaComandos[i].protocolo);
      Serial.print(F("  RAW=0x"));
      Serial.println(listaComandos[i].rawData, HEX);
    }
  }

  Serial.println(F("\n[0-9] -> Enviar comando IR do slot"));
  Serial.println(F("[S]   -> Salvar ultimo sinal capturado (vai pedir o nome)"));
  Serial.println(F("[D]   -> Deletar comando (digite D e depois o numero)"));
  Serial.println(F("[C]   -> Limpar TODOS os comandos"));
  Serial.println(F("[T]   -> Mostrar temperatura e umidade atuais"));
  Serial.println(F("[F]   -> Enviar clima e recarregar comandos do Firebase"));
  Serial.println(F("[L]   -> Listar menu novamente"));
  Serial.println(F("=====================================\n"));
}

void verificarTerminal() {
  if (!Serial.available()) return;

  char c = Serial.read();

  if (c >= '0' && c <= '9') {
    int id = c - '0';
    executarComandoIR(id);
  }
  else if (c == 'S' || c == 's') {
    Serial.println(F("Digite o nome do comando e pressione ENTER:"));
    while (!Serial.available()) {}
    String nome = Serial.readStringUntil('\n');
    nome.trim();
    salvarNovoComando(nome);
  }
  else if (c == 'D' || c == 'd') {
    Serial.println(F("Digite o numero do slot a apagar e pressione ENTER:"));
    while (!Serial.available()) {}
    int id = Serial.parseInt();
    deletarComando(id);
  }
  else if (c == 'C' || c == 'c') {
    limparTodosComandos();
  }
  else if (c == 'T' || c == 't') {
    float t = lerTemperatura();
    float h = lerUmidade();

    Serial.println(F("\n=== LEITURA DE CLIMA (DHT22) ==="));
    if (isnan(t)) {
      Serial.println(F("Temperatura: erro na leitura"));
    } else {
      Serial.print(F("Temperatura: "));
      Serial.print(t, 1);
      Serial.println(F(" °C"));
    }

    if (isnan(h)) {
      Serial.println(F("Umidade:     erro na leitura"));
    } else {
      Serial.print(F("Umidade:     "));
      Serial.print(h, 1);
      Serial.println(F(" %"));
    }
    Serial.println(F("=================================\n"));
  }
  else if (c == 'F' || c == 'f') {
    enviarClimaFirebase();
    sincronizarComandosFirebase();
  }
  else if (c == 'L' || c == 'l') {
    mostrarMenuTerminal();
  }
}

// ===============================================================
// 7. SENSORES (DHT22)
// ===============================================================
float lerTemperatura() {
  float t = dht.readTemperature();
  if (isnan(t)) return NAN;
  return t;
}

float lerUmidade() {
  float h = dht.readHumidity();
  if (isnan(h)) return NAN;
  return h;
}

// ===============================================================
// 8. WIFI + WEB (ESP-01) – COM LOG NORMAL
// ===============================================================
void iniciarWifi() {
  Serial.println(F("\n[WiFi] Inicializando ESP-01..."));

  sendCMD("AT+RST", 2000);
  sendCMD("AT+CWMODE=1", 1000); 

  Serial.print(F("[WiFi] Conectando à rede: "));
  Serial.println(SSID_REDE);
  String cmdJoin = "AT+CWJAP\"" + SSID_REDE + "\",\"" + SENHA_REDE + "\"";
  cmdJoin = "AT+CWJAP=\"" + SSID_REDE + "\",\"" + SENHA_REDE + "\"";
  sendCMD(cmdJoin, 8000);

  Serial1.println("AT+CIFSR");
  String resp = lerSerial1(2000);

  int ipIndex = resp.indexOf("STAIP,\"");
  if (ipIndex != -1) {
    ipIndex += 7; 
    int ipEnd = resp.indexOf('"', ipIndex);
    if (ipEnd != -1) {
      IP_LOCAL = resp.substring(ipIndex, ipEnd);
      Serial.print(F("[WiFi] IP obtido: "));
      Serial.println(IP_LOCAL);
      Serial.print(F("[WiFi] Acesse: http://"));
      Serial.println(IP_LOCAL);
    }
  } else {
    Serial.println(F("[WiFi] Nao foi possivel obter o IP do ESP-01."));
  }

  sendCMD("AT+CIPMUX=1", 1000);
  sendCMD("AT+CIPSERVER=1,80", 1000);

  Serial.println(F("[WiFi] Servidor HTTP na porta 80 pronto."));
}

String lerSerial1(int timeoutMs) {
  String r = "";
  long t0 = millis();
  while (millis() - t0 < timeoutMs) {
    while (Serial1.available()) {
      char c = Serial1.read();
      r += c;
    }
  }
  return r;
}

void processarWeb(int idConexao) {
  String req = lerSerial1(300);

  if (req.indexOf("GET /CMD") != -1) {
    int pos = req.indexOf("ID=");
    if (pos != -1) {
      int id = req.substring(pos + 3).toInt();
      executarComandoIR(id);
    }
  }
  else if (req.indexOf("GET /SAVE") != -1) {
    int pos = req.indexOf("N=");
    if (pos != -1) {
      String n = req.substring(pos + 2);
      int espaco = n.indexOf(' ');
      if (espaco != -1) n = n.substring(0, espaco);
      n.trim();
      salvarNovoComando(n);
    }
  }
  else if (req.indexOf("GET /DEL") != -1) {
    int pos = req.indexOf("ID=");
    if (pos != -1) {
      int id = req.substring(pos + 3).toInt();
      deletarComando(id);
    }
  }

  enviarHTML(idConexao);
}

void enviarHTML(int idConexao) {
  float t = lerTemperatura();
  float h = lerUmidade();

  while (Serial1.available()) Serial1.read();

  String html;
  html.reserve(3000);

  html += F("<!DOCTYPE html><html lang='pt-BR'><head>");
  html += F("<meta charset='utf-8'>");
  html += F("<meta name='viewport' content='width=device-width, initial-scale=1'>");
  html += F("<title>Central IR + Clima</title>");
  html += F("<style>");
  html += F("body{margin:0;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;"
            "background:linear-gradient(135deg,#1e3c72,#2a5298);color:#fff;text-align:center;padding:16px;}");
  html += F(".container{max-width:480px;margin:0 auto;}");
  html += F(".card{background:rgba(0,0,0,0.25);border-radius:18px;padding:18px;margin:10px 0;"
            "box-shadow:0 8px 24px rgba(0,0,0,0.3);backdrop-filter:blur(6px);}");
  html += F(".title{font-size:24px;font-weight:700;margin-bottom:4px;}");
  html += F(".subtitle{font-size:13px;opacity:.8;margin-bottom:10px;}");
  html += F(".clima{display:flex;justify-content:space-around;align-items:center;margin-top:10px;}");
  html += F(".temp{font-size:42px;font-weight:700;}");
  html += F(".umid{font-size:18px;opacity:.9;}");
  html += F(".chip{display:inline-block;padding:6px 12px;border-radius:999px;font-size:12px;"
            "background:rgba(255,255,255,0.12);margin:2px 4px;}");
  html += F(".btn{display:block;width:100%;padding:12px 10px;margin:6px 0;border-radius:12px;"
            "border:none;font-size:16px;font-weight:600;color:#fff;text-decoration:none;"
            "background:linear-gradient(135deg,#00b09b,#96c93d);box-shadow:0 4px 12px rgba(0,0,0,0.25);}");
  html += F(".btn:active{transform:scale(.98);}");
  html += F(".btn-del{background:linear-gradient(135deg,#ff416c,#ff4b2b);width:24%;display:inline-block;margin-left:2%;}");
  html += F(".btn-exec{width:73%;display:inline-block;}");
  html += F("input{width:100%;padding:10px 12px;border-radius:10px;border:none;font-size:14px;"
            "box-sizing:border-box;margin-bottom:8px;}");
  html += F(".cmd-list small{font-size:11px;opacity:.8;display:block;margin-top:2px;}");
  html += F("</style></head><body><div class='container'>");

  html += F("<div class='card'>");
  html += F("<div class='title'>Central IR + Clima</div>");
  html += F("<div class='subtitle'>Samsung / NEC / Pulse Distance • DHT22 • ESP-01</div>");
  html += F("<span class='chip'>WiFi: ");
  html += SSID_REDE;
  html += F("</span>");
  if (IP_LOCAL.length() > 0) {
    html += F("<span class='chip'>IP: ");
    html += IP_LOCAL;
    html += F("</span>");
  }
  html += F("</div>");

  html += F("<div class='card'>");
  html += F("<div class='title'>Ambiente</div>");
  html += F("<div class='clima'>");
  html += F("<div class='temp'>");
  if (isnan(t)) html += F("--.- &deg;C");
  else { html += String(t, 1); html += F(" &deg;C"); }
  html += F("</div>");
  html += F("<div class='umid'>Umidade<br>");
  if (isnan(h)) html += F("-- %");
  else { html += String((int)h); html += F(" %"); }
  html += F("</div></div></div>");

  html += F("<div class='card cmd-list'>");
  html += F("<div class='title'>Controles IR</div>");
  bool tem = false;
  for (int i = 0; i < MAX_COMANDOS_IR; i++) {
    if (listaComandos[i].ativo) {
      tem = true;
      html += F("<div>");
      html += "<a class='btn btn-exec' href='/CMD?ID=" + String(i) + "'>";
      html += String(listaComandos[i].nome);
      html += F("</a>");
      html += "<a class='btn btn-del' href='/DEL?ID=" + String(i) + "'>X</a>";
      html += "<small>Slot ";
      html += String(i);
      html += F(" • RAW 0x");
      html += String(listaComandos[i].rawData, HEX);
      html += F("</small></div>");
    }
  }
  if (!tem) {
    html += F("<p>Nenhum comando IR salvo ainda.</p>");
  }
  html += F("</div>");

  html += F("<div class='card'>");
  html += F("<div class='title'>Adicionar novo comando</div>");
  if (ultimoValido) {
    html += F("<p style='color:#7CFC00;font-weight:bold;'>Sinal capturado!</p>");
    html += F("<small>Protocolo: ");
    html += String((int)ultimoProtocolo);
    html += F(" • RAW 0x");
    html += String(ultimoRawData, HEX);
    html += F("</small><br><br>");
    html += F("<input id='nome' placeholder='Nome do comando (ex: Ligar TV)' />");
    html += F("<a class='btn' href='#' onclick=\"if(nome.value.trim()!=''){location.href='/SAVE?N='+encodeURIComponent(nome.value)}\">Salvar</a>");
  } else {
    html += F("<p>Aguardando captura de um sinal IR...</p>");
    html += F("<a class='btn' href='/'>Atualizar</a>");
  }
  html += F("</div>");

  html += F("</div></body></html>");

  String header = F("HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/html\r\n"
                    "Connection: close\r\n\r\n");

  int totalLen = header.length() + html.length();

  String cmd = "AT+CIPSEND=" + String(idConexao) + "," + String(totalLen);
  sendCMD(cmd, 200);

  Serial1.print(header);
  Serial1.print(html);

  delay(200);
  sendCMD("AT+CIPCLOSE=" + String(idConexao), 100);
}

void sendCMD(const String &cmd, int waitMs) {
  Serial.print(F("[ESP-01 <<] "));
  Serial.println(cmd);
  Serial1.println(cmd);

  String resp = "";
  long t0 = millis();
  while (millis() - t0 < waitMs) {
    while (Serial1.available()) {
      char c = Serial1.read();
      resp += c;
    }
  }

  resp.trim();
  if (resp.length() > 0) {
    Serial.print(F("[ESP-01 >>] "));
    Serial.println(resp);
  }
}

// ===============================================================
// 9. FIREBASE - REST VIA ESP-01 (SILENCIADO)
// ===============================================================
void sendCMDSilent(const String &cmd, int waitMs) {
  Serial1.println(cmd);
  long t0 = millis();
  while (millis() - t0 < waitMs) {
    while (Serial1.available()) {
      Serial1.read(); // descarta
    }
  }
}

bool firebaseHttpRequest(const String &method, const String &path, const String &body, String &response) {
  String req = method + " " + path + " HTTP/1.1\r\n";
  req += "Host: " + String(FIREBASE_HOST) + "\r\n";
  if (body.length() > 0) {
    req += "Content-Type: application/json\r\n";
    req += "Content-Length: " + String(body.length()) + "\r\n";
  }
  req += "Connection: close\r\n\r\n";
  if (body.length() > 0) {
    req += body;
  }

  sendCMDSilent("AT+CIPCLOSE=" + String(FIREBASE_LINK_ID), 50);

  String cmd = "AT+CIPSTART=" + String(FIREBASE_LINK_ID) +
               ",\"TCP\",\"" + FIREBASE_HOST + "\"," + String(FIREBASE_PORT);
  sendCMDSilent(cmd, 3000);

  cmd = "AT+CIPSEND=" + String(FIREBASE_LINK_ID) + "," + String(req.length());
  sendCMDSilent(cmd, 200);

  delay(80);
  Serial1.print(req);

  response = lerSerial1(5000);

  int idx = response.indexOf("HTTP/1.1");
  if (idx >= 0) {
    int end = response.indexOf("\r\n", idx);
    String status = response.substring(idx, (end > idx ? end : response.length()));
    Serial.print(F("[Firebase] "));
    Serial.println(status);
  }

  return true;
}

void enviarClimaFirebase() {
  float t = lerTemperatura();
  float h = lerUmidade();

  if (isnan(t) || isnan(h)) {
    Serial.println(F("[Firebase] Erro ao ler DHT, nao enviando clima."));
    return;
  }

  unsigned long segundos = millis() / 1000UL;
  String ts = String(segundos); 

  String body = "{";
  body += "\"temperatura\":" + String(t, 1) + ",";
  body += "\"umidade\":" + String(h, 1) + ",";
  body += "\"ultima_leitura\":\"" + ts + "\"";
  body += "}";

  String resp;
  firebaseHttpRequest("PUT", "/sensores/sala.json", body, resp);
  Serial.println(F("[Firebase] Clima enviado para /sensores/sala."));
}

void sincronizarComandosFirebase() {
  Serial.println(F("[Firebase] Carregando comandos IR..."));

  for (int i = 0; i < MAX_COMANDOS_IR; i++) {
    String path = "/comandos_ir/" + String(i) + ".json";
    String resp;
    firebaseHttpRequest("GET", path, "", resp);

    if (resp.indexOf("null") != -1 || resp.length() == 0) {
      listaComandos[i].ativo = false;
      continue;
    }

    String nome;
    long protocolo = 0;
    long endereco  = 0;
    long comando   = 0;
    unsigned long raw = 0;

    if (!extrairCampoString(resp, "nome", nome)) {
      listaComandos[i].ativo = false;
      continue;
    }
    extrairCampoInt(resp, "protocolo", protocolo);
    extrairCampoInt(resp, "endereco",  endereco);
    extrairCampoInt(resp, "comando",   comando);
    extrairCampoULong(resp, "rawData", raw);

    listaComandos[i].ativo     = true;
    listaComandos[i].protocolo = (uint8_t)protocolo;
    listaComandos[i].endereco  = (uint16_t)endereco;
    listaComandos[i].comando   = (uint16_t)comando;
    listaComandos[i].rawData   = (uint32_t)raw;

    nome.toCharArray(listaComandos[i].nome, sizeof(listaComandos[i].nome));
    listaComandos[i].nome[sizeof(listaComandos[i].nome) - 1] = '\0';

    Serial.print(F("[Firebase] Slot "));
    Serial.print(i);
    Serial.print(F(" <- "));
    Serial.println(listaComandos[i].nome);
  }

  mostrarMenuTerminal();
}

void enviarComandoFirebase(int slot) {
  if (slot < 0 || slot >= MAX_COMANDOS_IR) return;
  if (!listaComandos[slot].ativo) return;

  ComandoIR &c = listaComandos[slot];

  String path = "/comandos_ir/" + String(slot) + ".json";

  String body = "{";
  body += "\"nome\":\"" + String(c.nome) + "\",";
  body += "\"protocolo\":" + String((int)c.protocolo) + ",";
  body += "\"endereco\":" + String((unsigned long)c.endereco) + ",";
  body += "\"comando\":" + String((unsigned long)c.comando) + ",";
  body += "\"rawData\":" + String((unsigned long)c.rawData);
  body += "}";

  String resp;
  firebaseHttpRequest("PUT", path, body, resp);

  Serial.print(F("[Firebase] Comando salvo em /comandos_ir/"));
  Serial.println(slot);
}

void removerComandoFirebase(int slot) {
  if (slot < 0 || slot >= MAX_COMANDOS_IR) return;

  String path = "/comandos_ir/" + String(slot) + ".json";
  String resp;
  firebaseHttpRequest("DELETE", path, "", resp);

  Serial.print(F("[Firebase] Comando removido de /comandos_ir/"));
  Serial.println(slot);
}

bool extrairCampoInt(const String &json, const char *campo, long &out) {
  String chave = "\"" + String(campo) + "\":";
  int idx = json.indexOf(chave);
  if (idx < 0) return false;
  idx += chave.length();

  while (idx < (int)json.length() && (json[idx] == ' ' || json[idx] == '\r' || json[idx] == '\n')) idx++;

  int end = idx;
  while (end < (int)json.length() && (isDigit(json[end]) || json[end] == '-')) end++;

  String num = json.substring(idx, end);
  out = num.toInt();
  return true;
}

bool extrairCampoULong(const String &json, const char *campo, unsigned long &out) {
  String chave = "\"" + String(campo) + "\":";
  int idx = json.indexOf(chave);
  if (idx < 0) return false;
  idx += chave.length();

  while (idx < (int)json.length() && (json[idx] == ' ' || json[idx] == '\r' || json[idx] == '\n')) idx++;

  int end = idx;
  while (end < (int)json.length() && isDigit(json[end])) end++;

  String num = json.substring(idx, end);
  out = (unsigned long)num.toInt();
  return true;
}

bool extrairCampoString(const String &json, const char *campo, String &out) {
  String chave = "\"" + String(campo) + "\":\"";
  int idx = json.indexOf(chave);
  if (idx < 0) return false;
  idx += chave.length();

  int end = json.indexOf('"', idx);
  if (end < 0) return false;

  out = json.substring(idx, end);
  return true;
}