const String SSID_REDE  = "DCOMP_UFSJ";
const String SENHA_REDE = ""; 

void setup() {
  Serial.begin(9600);     
  Serial1.begin(115200);  

  Serial.println("--- Iniciando Conexao WiFi ---");

  enviarComando("AT+RST", 2000);

  enviarComando("AT+CWMODE=1", 1000);

  String cmdConectar = "AT+CWJAP=\"" + SSID_REDE + "\",\"" + SENHA_REDE + "\"";
  enviarComando(cmdConectar, 8000); 

  Serial.println("--- IP OBTIDO ---");
  Serial1.println("AT+CIFSR");
}

void loop() {
  if (Serial1.available()) {
    Serial.write(Serial1.read());
  }
  
  if (Serial.available()) {
    Serial1.write(Serial.read());
  }
}

void enviarComando(String cmd, int tempoEspera) {
  Serial.print("Enviando: "); Serial.println(cmd);
  Serial1.println(cmd);
  delay(tempoEspera);
  
  while (Serial1.available()) {
    Serial.write(Serial1.read());
  }
}