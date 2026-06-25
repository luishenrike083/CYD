#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <WiFi.h>
#include <ESP32Ping.h>
#include "modelo_ia.h"
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

TFT_eSPI tft = TFT_eSPI();


// ===== telegram bot =====
const char* BOTtoken = "8579086813:AAEqyG7aKjyY8aNv-cWDmhVh_y2OsPMsM_A";
const char* CHAT_ID = "1460313553";
WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);
unsigned long ultimoAlertaTelegram = 0;
const unsigned long INTERVALO_MSG = 60000; // 1 minuto

#define TOUCH_CS 33
#define TOUCH_IRQ 36


// ===== IA ANTIGA =====
const int TAMANHO_JANELA = 10;

int historicoPredicoes[TAMANHO_JANELA];
int indiceJanela = 0;

String statusRedeGlobal = "SEGURO";
bool alertaGlobal = false;

Eloquent::ML::Port::RandomForest ia;

// ===== GATEWAY DINÂMICO =====
IPAddress gatewayIP;

XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);
bool ultimoToque = false;
String redes[20];
int totalRedes = 0;
String ssidSelecionado = "";
String senhaWifi = "";

bool telaWifi = true;
bool telaPrincipal = false;
bool medindo = false;
bool telaSenha = false;
bool telaDesenhada = false;
bool maiusculo = true;
bool mostrarSenha = true;
bool telaMedicao = false;

float rttGlobal = 0;
float jitterGlobal = 0;
float lossGlobal = 0;
float devGlobal = 0;

String statusRede = "SEGURO";

String teclasMaiusculas[4][10] = {
    {"1","2","3","4","5","6","7","8","9","0"},
    {"Q","W","E","R","T","Y","U","I","O","P"},
    {"A","S","D","F","G","H","J","K","L","@"},
    {"Z","X","C","V","B","N","M",".","_","-"}
};

String teclasMinusculas[4][10] = {
    {"1","2","3","4","5","6","7","8","9","0"},
    {"q","w","e","r","t","y","u","i","o","p"},
    {"a","s","d","f","g","h","j","k","l","@"},
    {"z","x","c","v","b","n","m",".","_","-"}
};

void conectarWifi() {

    tft.fillScreen(TFT_BLACK);

    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);

    tft.setCursor(20, 50);
    tft.println("Conectando...");

    WiFi.begin(
        ssidSelecionado.c_str(),
        senhaWifi.c_str()
    );

    unsigned long tempoInicial = millis();

    while (WiFi.status() != WL_CONNECTED &&
           millis() - tempoInicial < 15000) {

        delay(500);
        tft.print(".");
    }

    // CONECTOU
    if (WiFi.status() == WL_CONNECTED) {

        tft.fillScreen(TFT_BLACK);

        tft.setCursor(20, 50);
        tft.println("Conectado!");

        gatewayIP = WiFi.gatewayIP();
        client.setInsecure();

        delay(3000);

        senhaWifi = "";
        telaSenha = false;
        telaPrincipal = true;
    }

    // NÃO CONECTOU
    else {

        tft.fillScreen(TFT_BLACK);

        tft.setTextColor(TFT_RED);
        tft.setCursor(20, 50);
        tft.println("Falha na conexao!");

        delay(3000);

        // continua na tela de senha
        senhaWifi = "";
        telaSenha = true;
        telaPrincipal = false;
        telaDesenhada = false;
    }
}

void desenharTeclado() {

    tft.fillScreen(TFT_BLACK);

    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);

    // SSID

    tft.setCursor(5, 5);
    tft.println(ssidSelecionado);

    // Botão voltar

    tft.drawRect(250, 5, 60, 25, TFT_YELLOW);
    tft.setCursor(255, 12);
    tft.setTextSize(1);
    tft.print("VOLTAR");

    // Caixa da senha

    tft.drawRect(5, 35, 180, 30, TFT_WHITE);

    tft.setCursor(10, 45);

    if (mostrarSenha) {

        tft.println(senhaWifi);

    } else {

        String mascara = "";

        for (int i = 0; i < senhaWifi.length(); i++) {
            mascara += "*";
        }

        tft.println(mascara);
    }
     // Senha

    tft.drawRect(190, 35, 40, 30, TFT_LIGHTGREY);

    if (mostrarSenha)
        tft.drawCentreString("ON", 210, 45, 1);
    else
        tft.drawCentreString("OFF", 210, 45, 1);

    // Teclas

    for (int linha = 0; linha < 4; linha++) {

        for (int coluna = 0; coluna < 10; coluna++) {

            int x = coluna * 24;
            int y = 70 + (linha * 30);

            tft.drawRect(x, y, 24, 30, TFT_WHITE);

            tft.setTextSize(1);
            tft.setCursor(x + 7, y + 10);

            if (maiusculo)
                tft.print(teclasMaiusculas[linha][coluna]);
            else
                tft.print(teclasMinusculas[linha][coluna]);
        }
    }

    // Botões inferiores

    tft.drawRect(5, 195, 50, 25, TFT_YELLOW);
    tft.setCursor(10, 203);
    tft.print("SH");

    tft.drawRect(60, 195, 50, 25, TFT_WHITE);
    tft.setCursor(68, 203);
    tft.print("ESP");

    tft.drawRect(115, 195, 50, 25, TFT_RED);
    tft.setCursor(125, 203);
    tft.print("<-");

    tft.drawRect(170, 195, 60, 25, TFT_GREEN);
    tft.setCursor(190, 203);
    tft.print("OK");
}

void desenharTelaPrincipal() {

    tft.fillScreen(TFT_BLACK);

    // Título

    tft.fillRect(0, 0, 320, 35, TFT_DARKCYAN);

    tft.setTextColor(TFT_WHITE, TFT_DARKCYAN);
    tft.setTextSize(2);
    tft.drawCentreString("WiNet Monitor", 160, 10, 2);

    // Informações

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);

    tft.setCursor(10, 50);
    tft.print("Rede: ");
    tft.println(ssidSelecionado);

    tft.setCursor(10, 70);
    tft.print("Gateway: ");
    tft.println(WiFi.gatewayIP());

    tft.setCursor(10, 90);
    tft.print("Sinal: ");
    tft.print(WiFi.RSSI());
    tft.println(" dBm");


// ================= BOTÕES =================

// Botão iniciar
tft.fillRoundRect(60, 120, 200, 40, 8, TFT_DARKGREEN);
tft.drawRoundRect(60, 120, 200, 40, 8, TFT_GREEN);
tft.drawCentreString("INICIAR", 160, 132, 2);

// Botão voltar
// Botão voltar
tft.fillRoundRect(60, 180, 200, 40, 8, TFT_NAVY);
tft.drawRoundRect(60, 180, 200, 40, 8, TFT_YELLOW);
tft.drawCentreString("VOLTAR", 160, 192, 2);
}

void desenharTelaMedicao() {

    tft.fillScreen(TFT_BLACK);

    // Cabeçalho
    tft.fillRect(0, 0, 320, 35, TFT_DARKCYAN);
    tft.setTextColor(TFT_WHITE, TFT_DARKCYAN);
    tft.drawCentreString("WiNet Monitor", 160, 10, 2);

    // Caixas métricas
    tft.drawRoundRect(10, 45, 145, 55, 8, TFT_CYAN);   // RTT
    tft.drawRoundRect(165, 45, 145, 55, 8, TFT_CYAN);  // Jitter

    tft.drawRoundRect(10, 110, 145, 55, 8, TFT_CYAN);  // Loss
    tft.drawRoundRect(165, 110, 145, 55, 8, TFT_CYAN); // Desvio

    // Títulos
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    tft.drawCentreString("RTT", 82, 52, 2);
    tft.drawCentreString("JITTER", 237, 52, 2);

    tft.drawCentreString("LOSS", 82, 117, 2);
    tft.drawCentreString("DESVIO", 237, 117, 2);

    // Caixa de status
    tft.drawRoundRect(10, 175, 300, 55, 8, TFT_WHITE);

    // Título
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("STATUS IA", 20, 185, 2);

    // Botão VOLTAR ao lado
    tft.fillRoundRect(200, 180, 90, 25, 8, TFT_YELLOW);
    tft.drawRoundRect(200, 180, 90, 25, 8, TFT_NAVY);

    tft.setTextColor(TFT_BLACK, TFT_YELLOW);
    tft.drawCentreString("VOLTAR", 245, 186, 2);
}

void executarDeteccaoIA() {

    float entrada_ia[] = {
        rttGlobal,
        jitterGlobal,
        lossGlobal
    };

    int pred = ia.predict(entrada_ia);

    historicoPredicoes[indiceJanela] = pred;
    indiceJanela = (indiceJanela + 1) % TAMANHO_JANELA;

    int contagemAtaque = 0;

    for (int i = 0; i < TAMANHO_JANELA; i++) {
        contagemAtaque += historicoPredicoes[i];
    }

    int sinalWifi = WiFi.RSSI();

    statusRedeGlobal = "SEGURO";
    alertaGlobal = false;

    if (contagemAtaque >= 7) {

        if (sinalWifi < -80) {
            statusRedeGlobal = "INSTAVEL (SINAL FRACO)";
        }

        else if (devGlobal < 10.0 && rttGlobal > 80.0) {
            statusRedeGlobal = "ATAQUE! ASSINATURA CONFIRMADA";
            alertaGlobal = true;
        }

        else if (lossGlobal > 25.0) {
            statusRedeGlobal = "FALHA DE CONEXAO";
            alertaGlobal = true;
        }

        else {
            statusRedeGlobal = "TRAFEGO SUSPEITO";
        }
    }

    Serial.printf("STATUS: %s (%d/10)\n",
        statusRedeGlobal.c_str(),
        contagemAtaque
    );

    if (alertaGlobal &&
        millis() - ultimoAlertaTelegram > INTERVALO_MSG) {

        String msg = "🚨 *ALERTA DE INTRUSÃO*\n\n";

        msg += "📶 Rede: `" + ssidSelecionado + "`\n";
        msg += "🌐 Gateway: `" + gatewayIP.toString() + "`\n\n";

        msg += "📊 *Métricas:*\n";
        msg += "RTT: `" + String(rttGlobal,2) + " ms`\n";
        msg += "Jitter: `" + String(jitterGlobal,2) + " ms`\n";
        msg += "Loss: `" + String(lossGlobal,2) + " %`\n";
        msg += "DEV: `" + String(devGlobal,2) + " ms`\n\n";

        msg += "⚠️ Status: *" + statusRedeGlobal + "*";

        if (bot.sendMessage(CHAT_ID, msg, "Markdown")) {
            ultimoAlertaTelegram = millis();
        }
    }

}

void realizarMedicao() {

    float rtt_soma = 0;
    float rtt_antigo = 0;
    float jitter_soma = 0;

    float rtt_min = 9999.0;
    float rtt_max = 0.0;

    int sucessos = 0;
    int sinalWifi = WiFi.RSSI();

    for (int i = 0; i < 5; i++) {

        if (Ping.ping(gatewayIP, 1)){

            float rtt_atual = Ping.averageTime();

            rtt_soma += rtt_atual;

            if (rtt_atual < rtt_min)
                rtt_min = rtt_atual;

            if (rtt_atual > rtt_max)
                rtt_max = rtt_atual;

            if (sucessos > 0)
                jitter_soma += abs(rtt_atual - rtt_antigo);

            rtt_antigo = rtt_atual;
            sucessos++;
        }
    }

    rttGlobal = (sucessos > 0) ?
                 (rtt_soma / sucessos) : 1000.0;

    jitterGlobal = (sucessos > 1) ?
                    (jitter_soma / (sucessos - 1)) : 0.0;

    lossGlobal = ((5.0 - sucessos) / 5.0) * 100.0;

    devGlobal = (sucessos > 0) ?
                 (rtt_max - rtt_min) : 0.0;

    // continua exatamente sua IA daqui para baixo
}

void setup() {

    Serial.begin(115200);

    tft.init();
    tft.setRotation(1);

    SPI.begin(25, 39, 32, 33);

    touch.begin();

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    totalRedes = WiFi.scanNetworks();


    for (int i = 0; i < totalRedes && i < 20; i++) {
        redes[i] = WiFi.SSID(i);
    }
}

void atualizarMetricas() {

    // RTT
    tft.fillRect(20, 70, 120, 20, TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawCentreString(String(rttGlobal,2) + " ms", 82, 75, 2);

    // Jitter
    tft.fillRect(175, 70, 120, 20, TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawCentreString(String(jitterGlobal,2) + " ms", 237, 75, 2);

    // Loss
    tft.fillRect(20, 135, 120, 20, TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawCentreString(String(lossGlobal,2) + " %", 82, 140, 2);

    // Desvio
    tft.fillRect(175, 135, 120, 20, TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawCentreString(String(devGlobal,2) + " ms", 237, 140, 2);

    // Status colorido
    // Limpa apenas a área do texto do status
    tft.fillRect(20, 205, 170, 18, TFT_BLACK);

    uint16_t corStatus = TFT_GREEN;

    if (statusRedeGlobal.indexOf("SUSPEITO") >= 0)
        corStatus = TFT_ORANGE;

    if (statusRedeGlobal.indexOf("ATAQUE") >= 0)
        corStatus = TFT_RED;

    if (statusRedeGlobal.indexOf("FALHA") >= 0)
        corStatus = TFT_RED;

    tft.setTextColor(corStatus, TFT_BLACK);
    tft.drawString(statusRedeGlobal, 20, 208, 2);
}

void loop() {


    if (telaMedicao) {

        static unsigned long ultimaMedicao = 0;

        if (medindo && millis() - ultimaMedicao > 1000) {

            realizarMedicao();      // coleta RTT, jitter, loss
            executarDeteccaoIA();   // roda lógica antiga

            ultimaMedicao = millis();
            atualizarMetricas();
        }

        if (!telaDesenhada) {

            desenharTelaMedicao();
            telaDesenhada = true;
        }

        if (touch.touched()) {

            TS_Point p = touch.getPoint();

            while (touch.touched()) delay(10);

            int x = map(p.x, 461, 3733, 0, 320) + 20;
            int y = map(p.y, 344, 3563, 0, 240) - 15;

            Serial.printf("Medicao -> X:%d Y:%d\n", x, y);

            // VOLTAR
            if (x > 200 && x < 290 &&
                y > 180 && y < 205) {

                telaMedicao = false;
                telaPrincipal = true;
                telaDesenhada = false;
            }
        }

        return;
    }


    if (telaPrincipal) {

        if (!telaDesenhada) {

            desenharTelaPrincipal();
            telaDesenhada = true;
        }

        if (touch.touched()) {

            TS_Point p = touch.getPoint();

            while (touch.touched()) {
                delay(10);
            }

            int x = map(p.x, 461, 3733, 0, 320) + 20;
            int y = map(p.y, 344, 3563, 0, 240) - 15;

            Serial.printf("Tela Principal -> X:%d Y:%d\n", x, y);
            tft.fillCircle(x, y, 4, TFT_RED);

            // INICIAR

            if (x > 60 && x < 260 &&
                y > 120 && y < 160) {

                medindo = true;
                telaMedicao = true;
                telaDesenhada = false;

                String msg = "🛡️ Monitoramento iniciado!\n";
                msg += "📶 Rede: " + ssidSelecionado + "\n";
                msg += "🌐 Gateway: " + gatewayIP.toString();
                bot.sendMessage(CHAT_ID, msg, "");

                Serial.println("Iniciando medicao...");
                delay(250);
            }

            // VOLTAR

            if (x > 60 && x < 260 &&
                y > 180 && y < 220) {   

                WiFi.disconnect(true);

                telaPrincipal = false;
                telaSenha = false;
                telaDesenhada = false;

                totalRedes = WiFi.scanNetworks();

                for (int i = 0; i < totalRedes && i < 20; i++)
                    redes[i] = WiFi.SSID(i);

                Serial.println("Retornando ao menu.");
                delay(250);
            }
        }

        return;
    }

    
    // ---------------- TELA DE SENHA ----------------

    if (telaSenha) {

        if (!telaDesenhada) {

            desenharTeclado();

            telaDesenhada = true;
        }


        if (touch.touched()) {

            TS_Point p = touch.getPoint();

                while (touch.touched()) {
                    delay(10);
                }

            int x = map(p.x, 461, 3733, 0, 320) +20;
            int y = map(p.y, 344, 3563, 0, 240) -15;

            tft.fillCircle(x, y, 4, TFT_RED);

            Serial.printf(
                "RAW X:%d RAW Y:%d -> Tela X:%d Y:%d\n",
                p.x,
                p.y,
                x,
                y
            );

            delay(500);

            // Descobrir tecla pressionada

            for (int linha = 0; linha < 4; linha++) {

                for (int coluna = 0; coluna < 10; coluna++) {

                    int teclaX = coluna * 24;
                    int teclaY = 70 + (linha * 30);

                    if (x >= teclaX &&
                        x <= teclaX + 24 &&
                        y >= teclaY &&
                        y <= teclaY + 30) {

                        if (maiusculo)
                            senhaWifi += teclasMaiusculas[linha][coluna];
                        else
                            senhaWifi += teclasMinusculas[linha][coluna];

                        telaDesenhada = false;
                    }
                }
            }


            // Mostrar/Ocultar senha

            if (x > 190 && x < 230 &&
                y > 35 && y < 65) {

                mostrarSenha = !mostrarSenha;

                telaDesenhada = false;
            }
            // SHIFT
            if (x > 5 && x < 55 &&
                y > 195 && y < 220) {

                maiusculo = !maiusculo;
                telaDesenhada = false;
            }

            // ESPAÇO
            if (x > 60 && x < 110 &&
                y > 195 && y < 220) {

                senhaWifi += " ";
                telaDesenhada = false;
            }
            // BACKSPACE
            if (x > 115 && x < 165 &&
                y > 195 && y < 220) {

                if (senhaWifi.length() > 0)
                    senhaWifi.remove(senhaWifi.length() - 1);

                telaDesenhada = false;
            }

            // OK
            if (x > 170 && x < 230 &&
                y > 195 && y < 220) {

                conectarWifi();

                telaDesenhada = false;
            }

            // VOLTAR PARA LISTA DE REDES
            if (x > 250 && x < 310 &&
                y > 5 && y < 30) {

                senhaWifi = "";
                telaSenha = false;
                telaPrincipal = false;
                telaDesenhada = false;

                totalRedes = WiFi.scanNetworks();

                for (int i = 0; i < totalRedes && i < 20; i++) {
                    redes[i] = WiFi.SSID(i);
                }

                delay(300);
            }


        }

        return;
    }


    // ---------------- LISTA WIFI ----------------

    if (!telaDesenhada) {

        tft.fillScreen(TFT_BLACK);

        tft.setCursor(10, 5);
        tft.println("Selecione a rede:");

        for (int i = 0; i < totalRedes && i < 8; i++) {

            int y = 40 + (i * 30);

            tft.drawRect(5, y, 230, 25, TFT_WHITE);

            tft.setCursor(10, y + 5);
            tft.println(redes[i]);
        }

        telaDesenhada = true;
    }

    if (touch.touched()) {

        TS_Point p = touch.getPoint();

        while (touch.touched()) {
            delay(10);
        }

        int yTela = map(p.y, 344, 3563, 0, 240);

        Serial.printf(
            "RAW -> X:%d Y:%d | Tela Y:%d\n",
            p.x,
            p.y,
            yTela
        );

        for (int i = 0; i < totalRedes && i < 8; i++) {

            int yCaixa = 40 + (i * 30);

            if (yTela >= yCaixa &&
                yTela <= yCaixa + 25) {

                ssidSelecionado = redes[i];

                Serial.println("Rede selecionada:");
                Serial.println(ssidSelecionado);

                telaSenha = true;
                telaDesenhada = false;

                delay(300);
            }
        }
    }
}