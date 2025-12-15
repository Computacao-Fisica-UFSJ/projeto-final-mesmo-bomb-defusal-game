# Sistema de Climatização Inteligente via Wi-Fi, com Infravermelho

**Integrantes:**
* Lucas Eduardo Bernardes de Paula
* Messias Feres Curi Melo

## 1. Contextualização do Projeto
Este projeto consiste no desenvolvimento de uma central de automação residencial focada no monitoramento climático e controle de dispositivos via infravermelho (IR). O sistema utiliza um **Arduino Mega** como controlador central, integrando conectividade Wi-Fi para gerenciamento remoto e sensores para leitura ambiental.

O objetivo principal é criar um ecossistema capaz de ler a temperatura e umidade do ambiente e permitir o acionamento remoto de Ar-Condicionados através de uma interface. O projeto explora conceitos de IoT (Internet das Coisas), protocolos de comunicação Serial (UART), manipulação de sinais infravermelhos e servidores Web embarcados.

## 2. Dinâmica e Funcionalidades
O sistema opera executando três tarefas principais simultaneamente:

1. **Monitoramento Climático (DHT22):**
* O sensor lê constantemente a temperatura e a umidade do local.
* Esses dados são exibidos em tempo real na interface Web acessível pelo navegador.


2. **Interface Web e Conectividade (ESP-01):**
* O módulo ESP-01 atua como um servidor Web. Ele gera uma página HTML (estilo "App") que exibe os dados climáticos.

3. **Controle Infravermelho (IR):**
* **Modo de Aprendizado:** O sistema possui um receptor capaz de ler sinais de controles remotos convencionais para "clonar" suas funções.
* **Modo de Envio:** Através de um LED emissor IR, o Arduino replica os códigos salvos para controlar os aparelhos, substituindo o controle físico.



## 3. Lista de Materiais
| Material | Especificação | Quantidade |
| --- | --- | --- |
| **Microcontrolador** | Arduino Mega 2560 | 1 |
| **Módulo Wi-Fi** | ESP8266 (Modelo ESP-01) | 1 |
| **Sensor Climático** | DHT22 (AM2302) | 1 |
| **Receptor IR** | Módulo KY-022 | 1 |
| **Transistor Bipolar** | BC547  | 1 |
| **Emissor IR** | LED Infravermelho (5mm) | 1 |
| **Resistores** | 220Ω e 330Ω (p/ LED) | 2 |
| **Conexões** | Protoboard 400 furos + Jumpers | 1 |

## 4. Diagrama de Montagem
Abaixo está o esquema de ligação dos componentes desenvolvido no software Fritzing.

<img src="./Esquema Fritzing/projeto_final.png" alt="Arduino Mega" width="1000" /> 

## 5. Detalhes da Montagem e Hardware
A escolha do **Arduino Mega 2560** foi estratégica para garantir o desempenho e a estabilidade do sistema, especialmente devido à necessidade de múltiplas portas Seriais de Hardware (HardwareSerial) e o uso de timers específicos exigidos pela biblioteca `IRremote`.

* **Arduino Mega 2560:** Atua como o cérebro do sistema, gerenciando a lógica de controle, leitura de sensores e comunicação com os periféricos. Sua capacidade de memória e número de portas digitais/analógicas facilitam a expansão futura do projeto.
* **ESP-01 (Wi-Fi):** Este módulo é responsável pela conectividade sem fio. Ele está conectado à porta `Serial1` do Arduino Mega (Pinos 18 TX e 19 RX). A utilização da `Serial1` (Hardware Serial) em vez da `SoftwareSerial` (usada no Uno) proporciona uma comunicação muito mais estável, rápida e confiável, essencial para o servidor web.


* **Sensor DHT22:** Conectado ao pino digital `D2`. Este sensor foi escolhido pela sua precisão superior na leitura de temperatura e umidade em comparação ao DHT11, garantindo dados mais confiáveis para o monitoramento ambiental.
* **Receptor IR (KY-022):** Conectado ao pino digital `D11`. Este componente é fundamental para a funcionalidade de "aprendizado" do sistema, permitindo capturar e decodificar os sinais infravermelhos emitidos pelos controles remotos originais dos aparelhos.
* **Emissor IR (LED):** Conectado **obrigatoriamente** ao pino digital `D9`.
* **Nota Técnica Importante:** No Arduino Mega, a biblioteca `IRremote` utiliza especificamente o **Timer 2** do microcontrolador para gerar a frequência portadora de 38kHz necessária para a transmissão do sinal infravermelho. Este timer está fisicamente atrelado ao pino 9, tornando essa conexão mandatória para o funcionamento correto do envio de comandos. Utilizar outro pino resultará na falha da emissão do sinal.



## 6. Interface do Usuário
A interface foi desenvolvida em HTML simples, servida diretamente pelo ESP-01 via comandos AT. Ela permite visualizar a temperatura e acionar os comandos salvos.


---

## 7. Diário de Bordo 
📅Abaixo, o registro das atividades desenvolvidas durante o cronograma do projeto:

**18/11 - Planejamento e Primeiros Testes:**
* Definição do escopo do projeto.
* Levantamento de materiais e estudo do datasheet do Arduino Mega.
* Primeiros testes com o sensor DHT22 para leitura de temperatura no Serial Monitor.


**19/11 - Integração do Módulo Wi-Fi:**
* Início dos testes com o ESP-01.
* Configuração dos comandos AT para conectar na rede Wi-Fi.
* Dificuldades iniciais com a alimentação de 3.3V do ESP (instabilidade).


**25/11 - Servidor Web Básico:**
* Criação do código HTML básico dentro do Arduino para ser enviado pelo ESP-01.
* Implementação da comunicação `Serial1` no Arduino Mega para conversar com o ESP.
* Sucesso ao acessar o IP do Arduino pelo navegador do celular.


**26/11 - Infravermelho (Recepção):**
* Instalação da biblioteca `IRremote` (versão 4.x).
* Montagem do receptor KY-022.
* Sucesso na leitura e clonagem de códigos de um controle remoto de TV Samsung (Protocolo NEC).


**09/12 - O Desafio do Ar-Condicionado:**
* Tentativa de clonar o controle do Ar-Condicionado (Split).
* **Problema:** O código recebido era identificado como `UNKNOWN` ou `OVERFLOW`.
* Descobrimos que o pacote de dados do AC é muito maior que o da TV, contendo todas as informações de estado (Temp, Fan, Mode) em um único envio.


**10/12 - Tentativas de Solução e Buffer:**
* Tentativa de aumentar o buffer de leitura da biblioteca IR.
* Análise dos códigos binários.
* Ajustes no circuito do emissor (troca de resistores para aumentar potência do LED).


**11/12 - Finalização e Documentação:**
* Refinamento da interface Web.
* Montagem final na protoboard para apresentação.
* Redação deste relatório e documentação dos problemas enfrentados com o protocolo do Ar-Condicionado.


## 8. Problemas Encontrados e Soluções
Durante o desenvolvimento, enfrentamos desafios significativos, especialmente na clonagem de sinais infravermelhos.

### 1. Sucesso com TV vs. Falha com Ar-CondicionadoO sistema funcionou perfeitamente para clonar e replicar comandos de uma TV Samsung e receptores de mídia genéricos. Conseguimos ler os códigos (ex: protocolo NEC), salvá-los e reenviá-los com sucesso.

No entanto, ao tentar controlar um **Ar-Condicionado (Split)**, o sistema falhou.

* **O Problema:** Diferente de TVs que enviam um código simples (ex: 32 bits, `0x2FD48B7`) indicando apenas "Botão Power", os controles de Ar-Condicionado enviam o **estado completo** do aparelho em cada aperto de botão (Temperatura desejada + Modo + Velocidade Fan + Swing + Checksum).
* **Consequência:** Isso gera um código "RAW" (bruto) extremamente longo, com centenas de variações de pulsos.
* **Resultado:** A biblioteca muitas vezes interpretava o sinal como `UNKNOWN` (Desconhecido) ou o buffer de memória do Arduino estourava ao tentar armazenar a sequência completa, resultando em um envio incompleto que o ar-condicionado ignorava.

### 2. Alimentação do ESP-01
O módulo ESP-01 consome picos de corrente que a saída 3.3V do Arduino às vezes não suporta, causando reinicializações e desconexões do Wi-Fi.

* *Solução:* O ideal em projetos definitivos é usar uma fonte externa de 3.3V ou um capacitor eletrolítico junto aos pinos de alimentação do ESP para estabilizar a tensão.

### 3. Conflito de Timers
Inicialmente, tentamos usar o emissor IR em outros pinos PWM. O código compilava, mas o LED não emitia o sinal modulado corretamente. Foi necessário estudar a documentação da biblioteca `IRremote` para descobrir que, no Arduino Mega, o uso do pino 9 é mandatório para a modulação de hardware correta.

## 9. Referências
* **Medição de Temperatura:** [Arduino and DHT22 Temperature Measurement](https://www.instructables.com/Arduino-and-DHT22-AM2302-Temperature-Measurement/)
* **Conectividade Wi-Fi:** [Conectando o Arduino à internet com ESP-01](https://curtocircuito.com.br/blog/IoT/conectando-o-arduino-a-internet-com-esp01)
* **Infravermelho:** [Guia Completo do Controle Remoto IR](https://blog.eletrogate.com/guia-completo-do-controle-remoto-ir-receptor-ir-para-arduino/)
* **Biblioteca Utilizada:** [IRremote by ArminJo](https://github.com/Arduino-IRremote/Arduino-IRremote)