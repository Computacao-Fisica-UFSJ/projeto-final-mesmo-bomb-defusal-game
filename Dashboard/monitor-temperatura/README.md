# Dashboard de Monitoramento de Sensores

Este projeto é uma aplicação front-end desenvolvida em **React** (Vite) para monitoramento em tempo real de dados ambientais (Temperatura e Umidade) provenientes de um **Firebase Realtime Database**.

O sistema exibe gráficos históricos, calcula médias dinâmicas e analisa tendências de variação, além de contar com um sistema de alerta por e-mail para temperaturas críticas.

---

## Tecnologias Utilizadas

- **React.js** (Hooks: `useState`, `useEffect`, `useRef`)
- **Firebase SDK** (Realtime Database)
- **Recharts** (Visualização de dados gráficos)
- **Lucide React** (Ícones dinâmicos)
- **FormSubmit** (API para envio de alertas por e-mail)
- **CSS Modules/Grid** (Layout responsivo)

---

## Fluxo de Exibição de Temperatura

O núcleo da exibição de temperatura ocorre no componente `SensorDashboard.jsx`. Abaixo está o detalhamento de como a informação é processada desde a coleta até a interface:

### 1. Coleta de Dados (Fetching)
A aplicação estabelece um intervalo de atualização a cada **10 segundos** (`setInterval` inside `useEffect`).
- **Fonte:** Nó `sensores/sala` do Firebase Realtime Database.
- **Dados brutos:** O sistema espera receber um objeto contendo `temperatura` e `umidade`.

### 2. Armazenamento e Histórico
Os dados não são apenas sobrescritos, mas acumulados em um array de estado (`historico`):
- O sistema mantém as **últimas 10 leituras** na memória para plotagem do gráfico.
- Cada leitura recebe um *timestamp* local (`hora`) no momento da busca.

### 3. Lógica de Processamento e Análise
A função `processarDados` é executada a cada nova leitura para gerar métricas visuais:

* **Média:** Calcula a média aritmética das leituras presentes no histórico atual.
* **Status de Variação:** Compara a leitura atual com a anterior para definir o status:
    * **Estável:** Variação entre -5°C e +5°C (Cor: Amarelo).
    * **Esquentando Rápido:** Aumento superior a 5°C (Cor: Vermelho).
    * **Resfriando Rápido:** Queda superior a 5°C (Cor: Azul).

### 4. Sistema de Alerta Crítico
Antes de atualizar o estado visual, o sistema verifica se a temperatura excede o limite de segurança:
- **Gatilho:** `temperatura > 35°C`.
- **Ação:** Chama `enviarAlertaEmail`.
- **Debounce:** Utiliza `useRef` para garantir que e-mails só sejam enviados a cada **15 minutos**, evitando spam.

---

## Componentes Visuais (UI)

A interface é dividida em painéis (Cards), onde o painel de Temperatura exibe:

1.  **Valor Atual:** Exibição numérica grande da última leitura.
2.  **Gráfico de Linha (Recharts):**
    * Eixo X: Horário da leitura.
    * Eixo Y: Temperatura (escala automática ou fixa 0-50°C).
    * Linha: Vermelha (`#ff6b6b`) com pontos de marcação.
3.  **Box de Estatísticas:**
    * Exibe a média calculada.
    * Ícone dinâmico (Seta para cima/baixo ou traço) baseado no status de variação.

---

## Configuração do Ambiente

Para rodar o projeto, crie um arquivo `.env` na raiz com as credenciais do Firebase:

```env
VITE_API_KEY=sua_api_key
VITE_AUTH_DOMAIN=seu_projeto.firebaseapp.com
VITE_DATABASE_URL=https://seu_projeto.firebaseio.com
VITE_PROJECT_ID=seu_project_id
VITE_STORAGE_BUCKET=seu_bucket
VITE_MESSAGING_SENDER_ID=seu_id
VITE_APP_ID=seu_app_id