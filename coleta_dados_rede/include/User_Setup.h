// User_Setup.h for TFT_eSPI - example for ILI9341 2.8" SPI displays
// Ajuste os pinos para corresponder ao seu TPM408-2.8 conforme o diagrama da placa.

#define ILI9341_DRIVER

// ESP32 hardware SPI pins (VSPI)
#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_SCLK 18

// Control pins - ajuste se necessário
#define TFT_CS   15  // Chip select control pin
#define TFT_DC   2   // Data Command control pin
#define TFT_RST  4   // Reset pin (ou -1 se ligado ao EN)

// Backlight pin (conforme seu diagrama: Screen backlight LED = IO21)
#define TFT_BL 21
#define TFT_BACKLIGHT_ON HIGH

// Carregar fontes e recursos
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

#define SPI_FREQUENCY  20000000 // reduzido para 20MHz para compatibilidade
#define SPI_READ_FREQUENCY  10000000
#define SPI_TOUCH_FREQUENCY  2500000

// Se a tela continuar branca, tente trocar o driver abaixo (descomente um e comente o atual):
// #define ILI9488_DRIVER
// #define ILI9486_DRIVER

// Também experimente frequências menores (10000000 ou 8000000) se necessário.

// Dimensões do display
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

#define SMOOTH_FONT

// End of User_Setup.h
