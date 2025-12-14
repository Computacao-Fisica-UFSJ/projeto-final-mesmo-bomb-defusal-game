#include <DHT.h>
#include <IRremote.hpp>
#include <ctype.h>  

// ===============================================================
// 1. CONFIGURAÇÕES GERAIS
// ===============================================================

const String SSID_REDE  = "DCOMP_UFSJ";
const String SENHA_REDE = "";   

String IP_LOCAL = "";

#define DHTPIN   2
#define DHTTYPE  DHT22
DHT dht(DHTPIN, DHTTYPE);

#define IR_RECEIVE_PIN 11
#define IR_SEND_PIN    9

#define MAX_COMANDOS_IR 10

struct ComandoIR {
  char    nome[16];    
  bool    ativo;        
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
  Serial.println(F("   CENTRAL IR + CLIMA (LOCAL APENAS)    "));
  Serial.println(F("========================================"));

  mostrarMenuTerminal();
  iniciarWifi();
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
}

// ===============================================================
// 5. MÓDULO IR - RECEPÇÃO E ARMAZENAMENTO
// ===============================================================
void handleIRRecepcao() {
  if (!IrReceiver.decode()) return;

  IRData &data = IrReceiver.decodedIRData;

  if (data.flags & IRDATA_FLAGS_WAS_OVERFLOW) {
    Serial.println(F("\n[IR] Overflow: sinal muito longo (ignorado)."));
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
    Serial.println(F("Nenhum sinal valido capturado ainda."));
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

  listaComandos[slot].ativo     = true;
  listaComandos[slot].protocolo = (uint8_t)ultimoProtocolo;
  listaComandos[slot].endereco  = ultimoEndereco;
  listaComandos[slot].comando   = ultimoComando;
  listaComandos[slot].rawData   = ultimoRawData;

  nome.toCharArray(listaComandos[slot].nome, sizeof(listaComandos[slot].nome));
  listaComandos[slot].nome[sizeof(listaComandos[slot].nome) - 1] = '\0';

  Serial.print(F("Comando salvo no slot "));
  Serial.println(slot);

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
  Serial.println(F("--------------------------------"));

  if (proto == UNKNOWN) {
    Serial.println(F("Protocolo UNKNOWN nao suportado."));
    return;
  }

  uint8_t repeats = 2;
  IrSender.write(proto, cmd.endereco, cmd.comando, repeats);

  IrReceiver.enableIRIn();
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

  mostrarMenuTerminal();
}

void limparTodosComandos() {
  memset(listaComandos, 0, sizeof(listaComandos));
  Serial.println(F("Todos os comandos foram apagados (RAM)."));
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
  Serial.println(F("\n===== MENU IR / WEB (LOCAL) ====="));
  Serial.println(F("Slots de comandos IR:"));
  for (int i = 0; i < MAX_COMANDOS_IR; i++) {
    if (listaComandos[i].ativo) {
      Serial.print(F(" ["));
      Serial.print(i);
      Serial.print(F("] "));
      Serial.println(listaComandos[i].nome);
    }
  }

  Serial.println(F("\n[0-9] -> Enviar comando IR do slot"));
  Serial.println(F("[S]   -> Salvar ultimo sinal capturado"));
  Serial.println(F("[D]   -> Deletar comando"));
  Serial.println(F("[C]   -> Limpar TODOS os comandos"));
  Serial.println(F("[T]   -> Mostrar clima atual"));
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
    Serial.println(F("Digite o nome do comando e ENTER:"));
    while (!Serial.available()) {}
    String nome = Serial.readStringUntil('\n');
    nome.trim();
    salvarNovoComando(nome);
  }
  else if (c == 'D' || c == 'd') {
    Serial.println(F("Digite numero do slot a apagar e ENTER:"));
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
    Serial.print(F("Temp: ")); Serial.print(t);
    Serial.print(F(" C | Umid: ")); Serial.print(h); Serial.println(F("%"));
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
// 8. WIFI + WEB (ESP-01)
// ===============================================================
void iniciarWifi() {
  Serial.println(F("\n[WiFi] Inicializando ESP-01..."));

  sendCMD("AT+RST", 2000);
  sendCMD("AT+CWMODE=1", 1000);

  Serial.print(F("[WiFi] Conectando a: "));
  Serial.println(SSID_REDE);
  
  String cmdJoin = "AT+CWJAP=\"" + SSID_REDE + "\",\"" + SENHA_REDE + "\"";
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
    Serial.println(F("[WiFi] IP nao encontrado. Verifique conexao."));
  }

  sendCMD("AT+CIPMUX=1", 1000);
  sendCMD("AT+CIPSERVER=1,80", 1000);
  Serial.println(F("[WiFi] Servidor HTTP pronto."));
}

String lerSerial1(int timeoutMs) {
  String r = "";
  long t0 = millis();
  while (millis() - t0 < timeoutMs) {
    while (Serial1.available()) {
      r += (char)Serial1.read();
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
  html.reserve(2500);

  html += F("<!DOCTYPE html><html lang='pt-BR'><head>");
  html += F("<meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>");
  html += F("<title>Central IR Local</title>");
  html += F("<style>");
  html += F("body{margin:0;font-family:sans-serif;background:#222;color:#eee;text-align:center;padding:16px;}");
  html += F(".card{background:#333;border-radius:12px;padding:16px;margin:10px auto;max-width:400px;box-shadow:0 4px 10px rgba(0,0,0,0.5);}");
  html += F(".temp{font-size:36px;font-weight:bold;color:#4db8ff;}");
  html += F(".btn{display:block;width:100%;padding:12px;margin:5px 0;background:#009688;color:#fff;text-decoration:none;border-radius:8px;}");
  html += F(".btn-del{background:#d32f2f;width:20%;display:inline-block;margin-left:2%;}");
  html += F(".btn-exec{width:75%;display:inline-block;}");
  html += F("input{width:100%;padding:10px;margin-bottom:8px;border-radius:5px;border:none;}");
  html += F("</style></head><body>");

  html += F("<div class='card'><h2>Central IR (Local)</h2>");
  if (IP_LOCAL.length() > 0) {
    html += F("<small>IP: "); html += IP_LOCAL; html += F("</small><br>");
  }
  html += F("</div>");

  html += F("<div class='card'>");
  html += F("<div class='temp'>");
  if (isnan(t)) html += F("--"); else html += String(t, 1);
  html += F("&deg;C</div>");
  html += F("<div>Umidade: ");
  if (isnan(h)) html += F("--"); else html += String((int)h);
  html += F("%</div></div>");

  html += F("<div class='card'><h3>Controles</h3>");
  bool tem = false;
  for (int i = 0; i < MAX_COMANDOS_IR; i++) {
    if (listaComandos[i].ativo) {
      tem = true;
      html += F("<div>");
      html += "<a class='btn btn-exec' href='/CMD?ID=" + String(i) + "'>" + String(listaComandos[i].nome) + "</a>";
      html += "<a class='btn btn-del' href='/DEL?ID=" + String(i) + "'>X</a>";
      html += F("</div>");
    }
  }
  if (!tem) html += F("<p>Nenhum comando salvo.</p>");
  html += F("</div>");

  html += F("<div class='card'><h3>Novo Comando</h3>");
  if (ultimoValido) {
    html += F("<p style='color:#0f0'>Sinal Capturado!</p>");
    html += F("<input id='nome' placeholder='Nome do comando' />");
    html += F("<a class='btn' href='#' onclick=\"if(nome.value.trim()!=''){location.href='/SAVE?N='+encodeURIComponent(nome.value)}\">Salvar</a>");
  } else {
    html += F("<p>Aponte o controle e aperte um botao...</p>");
    html += F("<a class='btn' style='background:#555' href='/'>Atualizar</a>");
  }
  html += F("</div></body></html>");

  String header = F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n");
  int totalLen = header.length() + html.length();

  String cmd = "AT+CIPSEND=" + String(idConexao) + "," + String(totalLen);
  sendCMD(cmd, 200);

  Serial1.print(header);
  Serial1.print(html);

  delay(200);
  sendCMD("AT+CIPCLOSE=" + String(idConexao), 100);
}

void sendCMD(const String &cmd, int waitMs) {
  Serial1.println(cmd);
  
  Serial.print(F("[ESP] ")); Serial.println(cmd);

  long t0 = millis();
  while (millis() - t0 < waitMs) {
    while (Serial1.available()) {
      Serial1.read(); 
    }
  }
}