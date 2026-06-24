#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP32Ping.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include "modelo_ia.h" 

TFT_eSPI tft = TFT_eSPI();

// --- CONFIGURAÇÕES DE REDE E ACESSO ---
const char* ssid = "brisa-3738741"; 
const char* password = "uf8gg6cc";
const char* BOTtoken = "8579086813:AAEqyG7aKjyY8aNv-cWDmhVh_y2OsPMsM_A";
const char* CHAT_ID = "1460313553";
const IPAddress remote_ip(192, 168, 0, 1); 

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);
Eloquent::ML::Port::RandomForest ia;

// --- CONTROLE DE ESTADO E TOUCH ---
bool executandoTeste = false; // false = Tela de Menu | true = Rodando Ping/IA
uint16_t touchX = 0, touchY = 0;

const int TAMANHO_JANELA = 10;
int historicoPredicoes[TAMANHO_JANELA];
int indiceJanela = 0;
unsigned long ultimoAlertaTelegram = 0;
const unsigned long INTERVALO_MSG = 60000; 

// Declaração de funções auxiliares da interface
void desenharMenuInicial();
void desenharTelaMonitoramento();
void displayMetrics(float rtt, float jitter, float perda, float desvio, const String &status);

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    client.setInsecure(); 
    
    for(int i=0; i<TAMANHO_JANELA; i++) historicoPredicoes[i] = 0;
    
    #ifdef TFT_BL
        pinMode(TFT_BL, OUTPUT);
        digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
    #endif

    tft.init();
    tft.setRotation(0); // Orientação retrato vertical
    
    // Inicia desenhando a tela de Menu e esperando o operador
    desenharMenuInicial();
}

void loop() {
    // Captura se houve um toque físico na tela e salva as coordenadas em touchX e touchY
    bool tocou = tft.getTouch(&touchX, &touchY);

    // =================================================================
    // ESTADO 1: SISTEMA EM ESPERA (MENU INICIAL)
    // =================================================================
    if (!executandoTeste) {
        if (tocou) {
            // Hitbox do botão INICIAR (X entre 30 e 210 | Y entre 130 e 190)
            if (touchX >= 30 && touchX <= 210 && touchY >= 130 && touchY <= 190) {
                executandoTeste = true;
                desenharTelaMonitoramento();
                bot.sendMessage(CHAT_ID, "▶️ *TESTE INICIADO* pelo terminal local.", "Markdown");
                delay(400); // Debounce para o dedo não clicar duas vezes
                return;
            }
        }
        delay(50); // Mantém a CPU descansando enquanto ninguém toca na tela
        return;    // Aborta o loop aqui! Impede que o código de ping abaixo rode.
    }

    // =================================================================
    // ESTADO 2: EXECUTANDO MONITORAMENTO DE REDE E IA
    // =================================================================
    
    float rtt_soma = 0, rtt_antigo = 0, jitter_soma = 0;
    float rtt_min = 9999.0, rtt_max = 0.0;
    int sucessos = 0;
    int sinalWifi = WiFi.RSSI();

    for(int i = 0; i < 5; i++) {
        if(Ping.ping(remote_ip, 1)) {
            float rtt_atual = Ping.averageTime();
            rtt_soma += rtt_atual;
            
            if (rtt_atual < rtt_min) rtt_min = rtt_atual;
            if (rtt_atual > rtt_max) rtt_max = rtt_atual;

            if (sucessos > 0) {
                jitter_soma += abs(rtt_atual - rtt_antigo);
            }
            rtt_antigo = rtt_atual;
            sucessos++;
        }
    }

    float rtt_medio = (sucessos > 0) ? (rtt_soma / sucessos) : 1000.0;
    float jitter = (sucessos > 1) ? (jitter_soma / (sucessos - 1)) : 0.0;
    float perda = ((5.0 - sucessos) / 5.0) * 100.0;
    float desvio_assinatura = (sucessos > 0) ? (rtt_max - rtt_min) : 0.0;

    float entrada_ia[] = { rtt_medio, jitter, perda };
    int preda_atual = ia.predict(entrada_ia);

    historicoPredicoes[indiceJanela] = preda_atual;
    indiceJanela = (indiceJanela + 1) % TAMANHO_JANELA;

    int contagemAtaque = 0;
    for(int i=0; i<TAMANHO_JANELA; i++) {
        contagemAtaque += historicoPredicoes[i];
    }

    String statusRede = "SEGURO";
    bool alertaReal = false;

    if (contagemAtaque >= 7) { 
        if (desvio_assinatura < 10.0 && rtt_medio > 80.0) {
            statusRede = "ATAQUE! ASSINATURA CONFIRMADA";
            alertaReal = true;
        } else if (perda > 25.0) {
            statusRede = "FALHA DE CONEXÃO";
            alertaReal = true;
        } else {
            statusRede = "TRAFEGO SUSPEITO";
        }
    }

    displayMetrics(rtt_medio, jitter, perda, desvio_assinatura, statusRede);
    
    // Alerta Telegram
    if (alertaReal && (millis() - ultimoAlertaTelegram > INTERVALO_MSG)) {
        String msg = "🚨 *DETECÇÃO DE INTRUSÃO (DDoS)*\n📍 *ALVO:* `" + remote_ip.toString() + "`\n";
        msg += "└ RTT Médio: `" + String(rtt_medio, 2) + " ms`\n";
        msg += "✅ *Status:* Confirmado por Edge AI (" + String(contagemAtaque) + "/10)";
        if(bot.sendMessage(CHAT_ID, msg, "Markdown")) ultimoAlertaTelegram = millis();
    }

    // --- ESCUTA FRACIONADA DO BOTÃO "PARAR" ---
    // Em vez de dar um delay(1000) cego, fatiamos a espera em 10 pedaços de 100ms.
    // Isso garante que se o usuário tocar no botão PARAR, a tela responda quase instantaneamente.
    for (int d = 0; d < 10; d++) {
        if (tft.getTouch(&touchX, &touchY)) {
            // Hitbox do botão PARAR no rodapé (X: 60 a 180 | Y: 260 a 305)
            if (touchX >= 60 && touchX <= 180 && touchY >= 260 && touchY <= 305) {
                executandoTeste = false;
                bot.sendMessage(CHAT_ID, "⏹️ *TESTE ABORTADO* pelo operador local.", "Markdown");
                desenharMenuInicial();
                delay(400);
                return;
            }
        }
        delay(100);
    }
}

// =================================================================
// FUNÇÕES DE DESENHO DA INTERFACE (IHM)
// =================================================================

void desenharMenuInicial() {
    tft.fillScreen(TFT_BLACK);
    
    // Cabeçalho
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawCentreString("CYD SECURE EDGE", 120, 30, 2);
    
    tft.setTextSize(1);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawCentreString("Alvo: " + remote_ip.toString(), 120, 60, 2);

    // Botão INICIAR (Retângulo verde centralizado)
    tft.fillRoundRect(30, 130, 180, 60, 8, TFT_GREEN);
    tft.drawRoundRect(30, 130, 180, 60, 8, TFT_WHITE);
    
    tft.setTextSize(2);
    tft.setTextColor(TFT_BLACK, TFT_GREEN);
    tft.drawCentreString("INICIAR", 120, 150, 2);

    // Instrução de rodapé
    tft.setTextSize(1);
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawCentreString("Toque para injetar pacotes", 120, 280, 1);
}

void desenharTelaMonitoramento() {
    tft.fillScreen(TFT_BLACK);
    
    // Desenha o botão vermelho de PARAR fixo no rodapé
    tft.fillRoundRect(60, 260, 120, 45, 6, TFT_RED);
    tft.drawRoundRect(60, 260, 120, 45, 6, TFT_WHITE);
    
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.drawCentreString("PARAR", 120, 275, 2);
}

void displayMetrics(float rtt, float jitter, float perda, float desvio, const String &status) {
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    
    tft.setCursor(5, 5, 2);
    tft.printf("RTT: %-6.1f ms   ", rtt); 
    
    tft.setCursor(5, 35, 2); 
    tft.printf("JIT: %-6.1f ms   ", jitter);
    
    tft.setCursor(5, 65, 2);
    tft.printf("LOSS: %-3.0f %%    ", perda);
    
    tft.setCursor(5, 95, 2);
    tft.printf("DEV: %-6.1f ms   ", desvio);

    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(5, 140, 1);
    tft.print("STATUS:                 "); 
    
    tft.setCursor(5, 165, 1);
    if (status == "SEGURO") tft.setTextColor(TFT_GREEN, TFT_BLACK);
    else if (status.indexOf("ATAQUE") != -1) tft.setTextColor(TFT_RED, TFT_BLACK);
    else tft.setTextColor(TFT_ORANGE, TFT_BLACK); 
    
    tft.print(status + "                 "); 
}