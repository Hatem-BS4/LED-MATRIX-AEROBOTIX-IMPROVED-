#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <FastLED.h>

// ================= HOTSPOT CONFIGURATION =================
const char* ap_ssid = "ESP32_LED_Matrix";
const char* ap_password = "12345678";

IPAddress local_IP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);
// =========================================================

// ================= HARDWARE SHIFT CONFIG =================
// Set this to the original number of the LED that was cut.
// 1 = First LED cut at the input connector.
#define CUT_LED 1
// =========================================================

// Safety: limits total current drawn by the strip (set to your PSU capability)
#define MAX_MILLIAMPS 2500

WebServer server(80);

#define MATRIX_WIDTH 34
#define MATRIX_HEIGHT 7
#define ART_WIDTH 17          // pixel-art grid = 17 columns, each one is 2 LEDs wide
#define NUM_LEDS 275
#define DATA_PIN 21
#define NUM_PRESETS 20
#define DEFAULT_SENTENCE "AEROBOTIX INSAT"

CRGB leds[NUM_LEDS];

// ================= STATE =================
enum Mode : uint8_t { MODE_TEXT, MODE_IMAGE, MODE_SLIDESHOW, MODE_SNAKE, MODE_FIRE, MODE_RAINBOW, MODE_BREATH };
enum TextStyle : uint8_t { STYLE_RED_BLUE, STYLE_SOLID, STYLE_RAINBOW };

Mode mode = MODE_TEXT;
TextStyle textStyle = STYLE_RED_BLUE;
CRGB pickedColor = CRGB(255, 0, 0);   // used by "Solid" text and "Breathing"

String currentSentence = DEFAULT_SENTENCE;
bool scrollFrozen = false;
bool needRedraw = true;
int offset = MATRIX_WIDTH - 2;
int verticalOffset = 1;
uint8_t hueBase = 0;

unsigned long lastFrame = 0;
unsigned long scrollSpeed = 150;   // ms between scroll steps (lower = faster)
uint8_t brightness = 80;           // 5..255

// Slideshow
unsigned long slideIntervalMs = 5000;
int slideIdx = 0;

// ================= LED LAYOUT =================
int ledLayout[MATRIX_HEIGHT][MATRIX_WIDTH] = {
  { 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 247, 248, 249, 250, 251, 252, 253, 254, 255, 256, 258, 259, 260, 261, 262, 263, 264, 265, 266, 267, 269, 270 },
  { 230, 229, 228, 227, 226, 225, 223, 222, 221, 220, 219, 218, 217, 216, 215, 214, 213, 212, 210, 209, 208, 207, 206, 205, 204, 203, 202, 201, 200, 199, 197, 196, 195, 194 },
  { 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190 },
  { 152, 151, 150, 149, 148, 147, 145, 144, 143, 142, 141, 140, 139, 138, 137, 136, 135, 134, 133, 132, 131, 130, 128, 127, 126, 125, 124, 123, 122, 121, 120, 119, 118, 117 },
  { 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113 },
  { 75, 74, 73, 72, 71, 70, 69, 68, 66, 65, 64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 49, 48, 47, 46, 45, 44, 43, 42, 41, 40 },
  { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 31, 32, 33, 34, 35, 36 }
};

// ================= FONT =================
const char letterA[][6] = { { 1, 1, 1, 1, 1, 1 }, { 1, 1, 0, 0, 1, 1 }, { 1, 1, 1, 1, 1, 1 }, { 1, 1, 0, 0, 1, 1 }, { 1, 1, 0, 0, 1, 1 } };
const char letterB[][6] = { { 1, 1, 1, 1, 0, 0 }, { 1, 1, 0, 0, 1, 1 }, { 1, 1, 1, 1, 0, 0 }, { 1, 1, 0, 0, 1, 1 }, { 1, 1, 1, 1, 0, 0 } };
const char letterC[][6] = { { 0, 0, 1, 1, 1, 1 }, { 1, 1, 0, 0, 0, 0 }, { 1, 1, 0, 0, 0, 0 }, { 1, 1, 0, 0, 0, 0 }, { 0, 0, 1, 1, 1, 1 } };
const char letterD[][6] = { { 1, 1, 1, 1, 0, 0 }, { 1, 1, 0, 0, 1, 1 }, { 1, 1, 0, 0, 1, 1 }, { 1, 1, 0, 0, 1, 1 }, { 1, 1, 1, 1, 0, 0 } };
const char letterE[][6] = { { 1, 1, 1, 1, 1, 1 }, { 1, 1, 0, 0, 0, 0 }, { 1, 1, 1, 1, 1, 1 }, { 1, 1, 0, 0, 0, 0 }, { 1, 1, 1, 1, 1, 1 } };
const char letterF[][6] = { { 1, 1, 1, 1, 1, 1 }, { 1, 1, 0, 0, 0, 0 }, { 1, 1, 1, 1, 1, 1 }, { 1, 1, 0, 0, 0, 0 }, { 1, 1, 0, 0, 0, 0 } };
const char letterG[][6] = { { 0, 0, 1, 1, 1, 1 }, { 1, 1, 0, 0, 0, 0 }, { 1, 1, 0, 0, 1, 1 }, { 1, 1, 0, 0, 1, 1 }, { 0, 0, 1, 1, 1, 1 } };
const char letterH[][6] = { { 1, 1, 0, 0, 1, 1 }, { 1, 1, 0, 0, 1, 1 }, { 1, 1, 1, 1, 1, 1 }, { 1, 1, 0, 0, 1, 1 }, { 1, 1, 0, 0, 1, 1 } };
const char letterI[][6] = { { 1, 1, 1, 1, 1, 1 }, { 0, 0, 1, 1, 0, 0 }, { 0, 0, 1, 1, 0, 0 }, { 0, 0, 1, 1, 0, 0 }, { 1, 1, 1, 1, 1, 1 } };
const char letterJ[][6] = { { 0, 0, 0, 0, 1, 1 }, { 0, 0, 0, 0, 1, 1 }, { 0, 0, 0, 0, 1, 1 }, { 1, 1, 0, 0, 1, 1 }, { 1, 1, 1, 1, 1, 1 } };
const char letterK[][6] = { { 1, 1, 0, 0, 1, 1 }, { 1, 1, 0, 0, 1, 1 }, { 1, 1, 1, 1, 0, 0 }, { 1, 1, 0, 0, 1, 1 }, { 1, 1, 0, 0, 1, 1 } };
const char letterL[][6] = { { 1, 1, 0, 0, 0, 0 }, { 1, 1, 0, 0, 0, 0 }, { 1, 1, 0, 0, 0, 0 }, { 1, 1, 0, 0, 0, 0 }, { 1, 1, 1, 1, 1, 1 } };
const char letterM[][6] = { { 1, 1, 0, 0, 1, 1 }, { 1, 1, 1, 1, 1, 1 }, { 1, 1, 0, 0, 1, 1 }, { 1, 1, 0, 0, 1, 1 }, { 1, 1, 0, 0, 1, 1 } };
const char letterN[][6] = { { 1, 1, 1, 1, 1, 1 }, { 1, 1, 0, 0, 1, 1 }, { 1, 1, 0, 0, 1, 1 }, { 1, 1, 0, 0, 1, 1 }, { 1, 1, 0, 0, 1, 1 } };
const char letterO[][6] = { { 1, 1, 1, 1, 1, 1 }, { 1, 1, 0, 0, 1, 1 }, { 1, 1, 0, 0, 1, 1 }, { 1, 1, 0, 0, 1, 1 }, { 1, 1, 1, 1, 1, 1 } };
const char letterP[][6] = { { 1, 1, 1, 1, 1, 1 }, { 1, 1, 0, 0, 1, 1 }, { 1, 1, 1, 1, 1, 1 }, { 1, 1, 0, 0, 0, 0 }, { 1, 1, 0, 0, 0, 0 } };
const char letterQ[][6] = { { 1, 1, 1, 1, 1, 1 }, { 1, 1, 0, 0, 1, 1 }, { 1, 1, 1, 1, 1, 1 }, { 0, 0, 0, 0, 1, 1 }, { 0, 0, 0, 0, 1, 1 } };
const char letterR[][6] = { { 1, 1, 1, 1, 0, 0 }, { 1, 1, 0, 0, 1, 1 }, { 1, 1, 1, 1, 0, 0 }, { 1, 1, 0, 0, 1, 1 }, { 1, 1, 0, 0, 1, 1 } };
const char letterS[][6] = { { 0, 0, 1, 1, 1, 1 }, { 1, 1, 0, 0, 0, 0 }, { 1, 1, 1, 1, 0, 0 }, { 0, 0, 0, 0, 1, 1 }, { 1, 1, 1, 1, 0, 0 } };
const char letterT[][6] = { { 1, 1, 1, 1, 1, 1 }, { 0, 0, 1, 1, 0, 0 }, { 0, 0, 1, 1, 0, 0 }, { 0, 0, 1, 1, 0, 0 }, { 0, 0, 1, 1, 0, 0 } };
const char letterU[][6] = { { 1, 1, 0, 0, 1, 1 }, { 1, 1, 0, 0, 1, 1 }, { 1, 1, 0, 0, 1, 1 }, { 1, 1, 0, 0, 1, 1 }, { 1, 1, 1, 1, 1, 1 } };
const char letterV[][6] = { { 1, 1, 0, 0, 1, 1 }, { 1, 1, 0, 0, 1, 1 }, { 1, 1, 0, 0, 1, 1 }, { 1, 1, 0, 0, 1, 1 }, { 0, 0, 1, 1, 0, 0 } };
const char letterW[][6] = { { 1, 1, 0, 0, 1, 1 }, { 1, 1, 0, 0, 1, 1 }, { 1, 1, 0, 0, 1, 1 }, { 1, 1, 1, 1, 1, 1 }, { 1, 1, 0, 0, 1, 1 } };
const char letterX[][6] = { { 1, 1, 0, 0, 1, 1 }, { 1, 1, 0, 0, 1, 1 }, { 0, 0, 1, 1, 0, 0 }, { 1, 1, 0, 0, 1, 1 }, { 1, 1, 0, 0, 1, 1 } };
const char letterY[][6] = { { 1, 1, 0, 0, 1, 1 }, { 1, 1, 0, 0, 1, 1 }, { 1, 1, 1, 1, 1, 1 }, { 0, 0, 1, 1, 0, 0 }, { 0, 0, 1, 1, 0, 0 } };
const char letterZ[][6] = { { 1, 1, 1, 1, 1, 1 }, { 0, 0, 0, 0, 1, 1 }, { 0, 0, 1, 1, 0, 0 }, { 1, 1, 0, 0, 0, 0 }, { 1, 1, 1, 1, 1, 1 } };
const char Space[][6]   = { { 0, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0 } };

// ================= PIXEL ART PRESETS (17 x 7) =================
// Palette: . black  R red  G green  B blue  Y yellow  W white  O orange
//          P purple C cyan  K pink  N brown  E grey
// Order must match the ART list in the web page.
static const char PRESETS[NUM_PRESETS][MATRIX_HEIGHT][ART_WIDTH + 1] = {
  { // 0 Heart
    "..RRR.......RRR..",
    ".RRRRR.....RRRRR.",
    ".RRRRRRRRRRRRRRR.",
    "..RRRRRRRRRRRRR..",
    "...RRRRRRRRRRR...",
    ".....RRRRRRR.....",
    ".......RRR......." },
  { // 1 Smiley
    "......YYYYY......",
    "....YYYYYYYYY....",
    "...YYBYYYYYBYY...",
    "...YYYYYYYYYYY...",
    "...YRYYYYYYYRY...",
    "....YYRRRRRYY....",
    "......YYYYY......" },
  { // 2 Space invader
    ".....G.....G.....",
    "......G...G......",
    ".....GGGGGGG.....",
    "....GG.GGG.GG....",
    "...GGGGGGGGGGG...",
    "...G.GGGGGGG.G...",
    "...G.G.....G.G..." },
  { // 3 Rocket
    "...RR.WWWWW......",
    "..RRRWWWWWWWR....",
    "...OOWWBBWWWWR...",
    "YOOOOWWBBWWWWWR..",
    "...OOWWBBWWWWR...",
    "..RRRWWWWWWWR....",
    "...RR.WWWWW......" },
  { // 4 Pine tree
    "........G........",
    ".......GGG.......",
    "......GGGGG......",
    ".....GGGGGGG.....",
    "....GGGGGGGGG....",
    "........N........",
    "........N........" },
  { // 5 Tunisia flag
    "RRRRRRRRRRRRRRRRR",
    "RRRRRRRWWWRRRRRRR",
    "RRRRRWWWWWWWRRRRR",
    "RRRRRWWRRRWWRRRRR",
    "RRRRRWWWWWWWRRRRR",
    "RRRRRRRWWWRRRRRRR",
    "RRRRRRRRRRRRRRRRR" },
  { // 6 Pac-Man and ghost
    "..YYYYY.....RRR..",
    ".YYYYYY....RRRRR.",
    ".YYYY......RWRWR.",
    ".YYY.....W.RRRRR.",
    ".YYYY......RRRRR.",
    ".YYYYYY....RRRRR.",
    "..YYYYY....R.R.R." },
  { // 7 Wi-Fi
    "...CCCCCCCCCCC...",
    "..CC.........CC..",
    "......CCCCC......",
    "....CC.....CC....",
    ".......CCC.......",
    ".................",
    "........C........" },
  { // 8 Mushroom
    "......RRRRR......",
    ".....RWWRWWR.....",
    "....RRWWRWWRR....",
    "....RRRRRRRRR....",
    ".....WWWWWWW.....",
    "......WBWBW......",
    "......WWWWW......" },
  { // 9 Sun
    "....Y...Y...Y....",
    ".....Y.YYY.Y.....",
    "......YYYYY......",
    "..YY.YYYYYYY.YY..",
    "......YYYYY......",
    ".....Y.YYY.Y.....",
    "....Y...Y...Y...." },
  { // 10 Cat
    "...O.........O...",
    "...OO.......OO...",
    "...OOOOOOOOOOO...",
    "...OOGOOOOOGOO...",
    "...OOOOOKOOOOO...",
    "....OOWOOOWOO....",
    "......OOOOO......" },
  { // 11 Coffee cup
    "......W...W......",
    ".......W...W.....",
    "....NNNNNNNNN....",
    "....WWWWWWWWW.WW.",
    "....WWWWWWWWW..W.",
    ".....WWWWWWW.WW..",
    "......WWWWW......" },
  { // 12 Music note
    ".......PPPPP.....",
    ".......P....P....",
    ".......P....P....",
    ".......P....P....",
    "....PPPP.PPPP....",
    "....PPPP.PPPP....",
    ".....PP...PP....." },
  { // 13 Robot
    "........R........",
    "....EEEEEEEEE....",
    "....ECCEEECCE....",
    "....ECCEEECCE....",
    "....EEEEEEEEE....",
    "....ERERERERE....",
    "....EEEEEEEEE...." },
  { // 14 Rainbow stripes
    "RRRRRRRRRRRRRRRRR",
    "OOOOOOOOOOOOOOOOO",
    "YYYYYYYYYYYYYYYYY",
    "GGGGGGGGGGGGGGGGG",
    "CCCCCCCCCCCCCCCCC",
    "BBBBBBBBBBBBBBBBB",
    "PPPPPPPPPPPPPPPPP" },
  { // 15 Skull
    ".....WWWWWWW.....",
    "....WWWWWWWWW....",
    "....W..WWW..W....",
    "....W..WWW..W....",
    "....WWWW.WWWW....",
    ".....WWWWWWW.....",
    ".....W.W.W.W....." },
  { // 16 Fish
    "...C...CCCCC.....",
    "...CC.CCCCCCCC...",
    "...CCCCCCCCCWCC..",
    "...CCCCCCCCCCCC..",
    "...CC.CCCCCCCC...",
    "...C...CCCCC.....",
    "................." },
  { // 17 Lightning bolt
    "........YYYY.....",
    ".......YYYY......",
    "......YYYY.......",
    ".....YYYYYYY.....",
    "........YYY......",
    ".......YY........",
    "......Y.........." },
  { // 18 Apple
    ".......N.GG......",
    "....RRRRNRRRR....",
    "...RWRRRRRRRRR...",
    "...RRRRRRRRRRR...",
    "...RRRRRRRRRRR...",
    "....RRRRRRRRR....",
    ".....RRR.RRR....." },
  { // 19 UFO
    ".......CCC.......",
    "......CCCCC......",
    "...EEEEEEEEEEE...",
    "..EEYEEYEEYEEYE..",
    "...EEEEEEEEEEE...",
    "....G...G...G....",
    ".....G..G..G....." }
};

CRGB paletteColor(char c) {
  switch (c) {
    case 'R': return CRGB(255, 0, 0);
    case 'G': return CRGB(0, 255, 0);
    case 'B': return CRGB(0, 0, 255);
    case 'Y': return CRGB(255, 255, 0);
    case 'W': return CRGB(255, 255, 255);
    case 'O': return CRGB(255, 90, 0);
    case 'P': return CRGB(160, 0, 255);
    case 'C': return CRGB(0, 255, 255);
    case 'K': return CRGB(255, 60, 140);
    case 'N': return CRGB(110, 55, 8);
    case 'E': return CRGB(110, 110, 110);
    default:  return CRGB::Black;
  }
}

// ===== AUTOMATIC INDEX OFFSET HELPER =====
int getLedIndex(int row, int col) {
  int rawIndex = ledLayout[row][col];
  if (rawIndex < CUT_LED) return rawIndex;
  if (rawIndex == CUT_LED) return 0; // Returns 0 to ignore the cut LED
  return rawIndex - 1; // Shifts all subsequent LEDs down by 1
}

// ===== UNIFIED PIXEL WRITER =====
// Takes a TRUE RGB color and applies the color-order correction of your strip
// (same correction your old text + image code applied, now in one place).
void setPix(int row, int col, CRGB c) {
  if (row < 0 || row >= MATRIX_HEIGHT || col < 0 || col >= MATRIX_WIDTH) return;
  int idx = getLedIndex(row, col);
  if (idx < 1 || idx > NUM_LEDS) return;
  if ((idx - 1) > 196) leds[idx - 1] = CRGB(c.g, c.b, c.r);
  else                 leds[idx - 1] = CRGB(c.g, c.r, c.b);
}

// Draws one "art pixel" (2 LEDs wide)
void setArtPix(int x, int y, CRGB c) {
  setPix(y, x * 2, c);
  setPix(y, x * 2 + 1, c);
}

void clearAll() {
  fill_solid(leds, NUM_LEDS, CRGB(0, 0, 0));
}

void drawPreset(int id) {
  if (id < 0 || id >= NUM_PRESETS) return;
  clearAll();
  for (int y = 0; y < MATRIX_HEIGHT; y++) {
    for (int x = 0; x < ART_WIDTH; x++) {
      char c = PRESETS[id][y][x];
      if (c != '.' && c != 0) setArtPix(x, y, paletteColor(c));
    }
  }
  FastLED.show();
}

// ================= TEXT =================
const char (*convertChar(char c))[6] {
  switch (c) {
    case 'A': return letterA; case 'B': return letterB; case 'C': return letterC;
    case 'D': return letterD; case 'E': return letterE; case 'F': return letterF;
    case 'G': return letterG; case 'H': return letterH; case 'I': return letterI;
    case 'J': return letterJ; case 'K': return letterK; case 'L': return letterL;
    case 'M': return letterM; case 'N': return letterN; case 'O': return letterO;
    case 'P': return letterP; case 'Q': return letterQ; case 'R': return letterR;
    case 'S': return letterS; case 'T': return letterT; case 'U': return letterU;
    case 'V': return letterV; case 'W': return letterW; case 'X': return letterX;
    case 'Y': return letterY; case 'Z': return letterZ; case ' ': return Space;
    default: return nullptr;
  }
}

CRGB textColor(int charIdx, int col) {
  switch (textStyle) {
    case STYLE_SOLID:
      return pickedColor;
    case STYLE_RAINBOW: {
      CRGB c;
      hsv2rgb_rainbow(CHSV((uint8_t)(hueBase + charIdx * 24 + col * 5), 255, 255), c);
      return c;
    }
    default: // STYLE_RED_BLUE: even chars blue, odd chars red (same as before)
      return (charIdx % 2 == 0) ? CRGB(0, 0, 150) : CRGB(150, 0, 0);
  }
}

void displayLetter(const char letter[5][6], int charIdx, int index, int height) {
  if (letter == nullptr) return;
  for (int row = 0; row < 5; row++) {
    for (int col = 0; col < 6; col++) {
      if (letter[row][col] != 1) continue;
      int targetCol = col + index;
      if (targetCol < 0 || targetCol >= MATRIX_WIDTH) continue;
      setPix(row + height, targetCol, textColor(charIdx, col));
    }
  }
}

void renderText() {
  clearAll();
  int len = currentSentence.length();
  if (offset + len * 8 <= 0) offset = MATRIX_WIDTH - 2;
  for (int i = 0; i < len; i++) {
    displayLetter(convertChar(currentSentence[i]), i, i * 8 + offset, verticalOffset);
  }
  FastLED.show();
}

void enterTextMode() {
  mode = MODE_TEXT;
  needRedraw = true;
  lastFrame = 0;
}

// ================= SNAKE =================
#define SNAKE_W ART_WIDTH
#define SNAKE_H MATRIX_HEIGHT
#define SNAKE_MAX (SNAKE_W * SNAKE_H)

uint8_t snakeX[SNAKE_MAX], snakeY[SNAKE_MAX];
int snakeLen = 3;
int8_t snakeDx = 1, snakeDy = 0;     // direction applied at last step
int8_t pendDx = 1, pendDy = 0;       // direction requested by the D-pad
uint8_t foodX = 0, foodY = 0;
bool snakeDead = false;
bool snakeFlash = false;
unsigned long snakeDeadTime = 0;
int snakeStepMs = 220;

void snakePlaceFood() {
  for (int tries = 0; tries < 200; tries++) {
    int fx = random(0, SNAKE_W);
    int fy = random(0, SNAKE_H);
    bool onSnake = false;
    for (int i = 0; i < snakeLen; i++) {
      if (snakeX[i] == fx && snakeY[i] == fy) { onSnake = true; break; }
    }
    if (!onSnake) { foodX = fx; foodY = fy; return; }
  }
}

void snakeReset() {
  snakeLen = 3;
  for (int i = 0; i < snakeLen; i++) { snakeX[i] = 8 - i; snakeY[i] = 3; }
  snakeDx = pendDx = 1;
  snakeDy = pendDy = 0;
  snakeDead = false;
  snakeFlash = false;
  snakeStepMs = 220;
  snakePlaceFood();
}

void snakeStep() {
  snakeDx = pendDx;
  snakeDy = pendDy;
  int nx = (snakeX[0] + snakeDx + SNAKE_W) % SNAKE_W;   // walls wrap around
  int ny = (snakeY[0] + snakeDy + SNAKE_H) % SNAKE_H;

  bool grow = (nx == foodX && ny == foodY);
  int checkLen = grow ? snakeLen : snakeLen - 1;       // tail moves away if not growing
  for (int i = 0; i < checkLen; i++) {
    if (snakeX[i] == nx && snakeY[i] == ny) {
      snakeDead = true;
      snakeDeadTime = millis();
      snakeFlash = true;
      return;
    }
  }
  if (grow && snakeLen < SNAKE_MAX) snakeLen++;
  for (int i = snakeLen - 1; i > 0; i--) { snakeX[i] = snakeX[i - 1]; snakeY[i] = snakeY[i - 1]; }
  snakeX[0] = nx;
  snakeY[0] = ny;
  if (grow) {
    snakePlaceFood();
    snakeStepMs = max(90, 220 - (snakeLen - 3) * 4);   // speeds up as it grows
  }
}

void snakeRender() {
  clearAll();
  CRGB bodyC = CRGB(0, 140, 0);
  CRGB headC = CRGB(120, 255, 120);
  if (snakeDead) {
    bodyC = snakeFlash ? CRGB(180, 0, 0) : CRGB(0, 0, 0);
    headC = bodyC;
  } else {
    setArtPix(foodX, foodY, CRGB(255, 40, 0));
  }
  for (int i = snakeLen - 1; i >= 0; i--) setArtPix(snakeX[i], snakeY[i], i == 0 ? headC : bodyC);
  FastLED.show();
}

// ================= AMBIENT EFFECTS =================
#define FIRE_COOLING 40
#define FIRE_SPARKING 130

void fireFrame() {
  static uint8_t heat[MATRIX_WIDTH][MATRIX_HEIGHT]; // y = 0 top ... y = 6 bottom
  for (int x = 0; x < MATRIX_WIDTH; x++) {
    for (int y = 0; y < MATRIX_HEIGHT; y++) {
      heat[x][y] = qsub8(heat[x][y], random8(0, ((FIRE_COOLING * 10) / MATRIX_HEIGHT) + 2));
    }
    for (int y = 0; y <= MATRIX_HEIGHT - 3; y++) {        // heat drifts upward
      heat[x][y] = (heat[x][y + 1] + heat[x][y + 2] + heat[x][y + 2]) / 3;
    }
    if (random8() < FIRE_SPARKING) {                      // new sparks at the bottom
      int y = MATRIX_HEIGHT - 1 - random8(2);
      heat[x][y] = qadd8(heat[x][y], random8(160, 255));
    }
  }
  clearAll();
  for (int x = 0; x < MATRIX_WIDTH; x++)
    for (int y = 0; y < MATRIX_HEIGHT; y++)
      setPix(y, x, HeatColor(heat[x][y]));
  FastLED.show();
}

void rainbowWaveFrame() {
  clearAll();
  for (int x = 0; x < MATRIX_WIDTH; x++) {
    for (int y = 0; y < MATRIX_HEIGHT; y++) {
      CRGB c;
      hsv2rgb_rainbow(CHSV((uint8_t)(hueBase + x * 7 + y * 3), 255, 255), c);
      setPix(y, x, c);
    }
  }
  FastLED.show();
  hueBase += 2;
}

void breathingFrame() {
  uint8_t v = beatsin8(14, 20, 255);      // ~4 s breathing cycle
  CRGB c = pickedColor;
  c.nscale8(v);
  clearAll();
  for (int x = 0; x < MATRIX_WIDTH; x++)
    for (int y = 0; y < MATRIX_HEIGHT; y++)
      setPix(y, x, c);
  FastLED.show();
}

// ================= HTTP HANDLERS =================
void parseColor(String hex) {
  if (hex.startsWith("#")) hex.remove(0, 1);
  uint32_t v = strtoul(hex.c_str(), nullptr, 16);
  pickedColor = CRGB((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
}

void handleSentence() {
  Serial.println("\n[HTTP] ---> Received call on /sentence endpoint!");
  String t = currentSentence;
  if (server.hasArg("text")) t = server.arg("text");
  else if (server.hasArg("plain")) t = server.arg("plain");
  t.toUpperCase();
  currentSentence = t;
  Serial.println(currentSentence);

  scrollFrozen = false;
  offset = MATRIX_WIDTH - 2;
  enterTextMode();
  if (currentSentence.length() == 0) { clearAll(); FastLED.show(); }
  server.send(200, "text/plain", "OK");
}

// Reset: back to the default phrase, blue & red, scrolling
void handleReset() {
  currentSentence = DEFAULT_SENTENCE;
  textStyle = STYLE_RED_BLUE;
  scrollFrozen = false;
  offset = MATRIX_WIDTH - 2;
  enterTextMode();
  server.send(200, "text/plain", "OK");
}

void handleStyle() {
  String m = server.arg("mode");
  if (m == "rb") textStyle = STYLE_RED_BLUE;
  else if (m == "solid") {
    textStyle = STYLE_SOLID;
    if (server.hasArg("color")) parseColor(server.arg("color"));
  } else if (m == "rainbow") textStyle = STYLE_RAINBOW;
  else { server.send(400, "text/plain", "Bad style"); return; }
  enterTextMode();
  server.send(200, "text/plain", "OK");
}

void handleFreeze() {
  scrollFrozen = !scrollFrozen;
  if (mode != MODE_TEXT) enterTextMode();
  needRedraw = true;
  server.send(200, "text/plain", scrollFrozen ? "frozen" : "running");
}

void handlePreset() {
  int id = server.arg("id").toInt();
  if (id < 0 || id >= NUM_PRESETS) { server.send(400, "text/plain", "Bad id"); return; }
  mode = MODE_IMAGE;
  drawPreset(id);
  server.send(200, "text/plain", "OK");
}

void handleSlideshow() {
  int s = server.hasArg("interval") ? server.arg("interval").toInt() : 5;
  s = constrain(s, 1, 60);
  slideIntervalMs = (unsigned long)s * 1000UL;
  slideIdx = 0;
  mode = MODE_SLIDESHOW;
  drawPreset(slideIdx);
  lastFrame = millis();
  server.send(200, "text/plain", "OK");
}

void handleMode() {
  String m = server.arg("m");
  if (server.hasArg("color")) parseColor(server.arg("color"));
  if (m == "text") enterTextMode();
  else if (m == "fire") { mode = MODE_FIRE; lastFrame = 0; }
  else if (m == "rainbow") { mode = MODE_RAINBOW; lastFrame = 0; }
  else if (m == "breath") { mode = MODE_BREATH; lastFrame = 0; }
  else if (m == "snake") { snakeReset(); mode = MODE_SNAKE; lastFrame = millis(); snakeRender(); }
  else { server.send(400, "text/plain", "Bad mode"); return; }
  server.send(200, "text/plain", "OK");
}

void handleSnake() {
  if (mode != MODE_SNAKE) { snakeReset(); mode = MODE_SNAKE; lastFrame = millis(); snakeRender(); }
  if (server.hasArg("start")) { snakeReset(); lastFrame = millis(); snakeRender(); }
  if (server.hasArg("dir")) {
    String d = server.arg("dir");
    int dx = 0, dy = 0;
    if (d == "U") dy = -1;
    else if (d == "D") dy = 1;
    else if (d == "L") dx = -1;
    else if (d == "R") dx = 1;
    // ignore a 180-degree reversal
    if ((dx != 0 || dy != 0) && !(dx == -snakeDx && dy == -snakeDy)) { pendDx = dx; pendDy = dy; }
  }
  server.send(200, "text/plain", "OK");
}

void handleState() {
  String s = "{\"mode\":" + String((int)mode) + ",\"frozen\":" + String(scrollFrozen ? "true" : "false") +
             ",\"brightness\":" + String((int)brightness) + ",\"speed\":" + String(scrollSpeed) + "}";
  server.send(200, "application/json", s);
}

void handleBrightness() {
  brightness = constrain(server.arg("v").toInt(), 5, 255);
  FastLED.setBrightness(brightness);
  FastLED.show();                       // re-apply to whatever is on screen
  server.send(200, "text/plain", "OK");
}

void handleSpeed() {
  scrollSpeed = constrain(server.arg("ms").toInt(), 20, 500);
  server.send(200, "text/plain", "OK");
}

void handlePost() {
  Serial.println("\n[HTTP] ---> Received call on /image endpoint!");

  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "Bad Request");
    return;
  }

  String message = server.arg("plain");
  server.send(200, "text/plain", "OK");

  DynamicJsonDocument doc(8192);   // 119 colour strings need more than 2048
  DeserializationError error = deserializeJson(doc, message);
  if (error) return;

  JsonArray tableData = doc["tableData"];

  int i = 0;
  int j = 0;

  mode = MODE_IMAGE;
  clearAll();

  for (JsonVariant color : tableData) {
    if (i >= MATRIX_HEIGHT) break;
    if (color != "b") {
      int r = 0, g = 0, b = 0;
      String colorString = color.as<String>();
      sscanf(colorString.c_str(), "rgb(%d,%d,%d)", &r, &g, &b);
      setArtPix(j, i, CRGB(r, g, b));
    }
    j++;
    if (j == ART_WIDTH) {
      i++;
      j = 0;
    }
  }
  FastLED.show();
}

// ================= WEB PAGE =================
const char htmlContent[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>Matrix Controller</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
  body { font-family: sans-serif; text-align: center; background: #222; color: #fff; margin: 0; padding: 8px; }
  h2 { margin: 8px 0; }
  .card { background: #2e2e2e; border-radius: 10px; padding: 8px; margin: 10px auto; max-width: 520px; }
  .card h3 { margin: 4px 0 6px; font-size: 16px; color: #9cf; }
  input, button { padding: 8px; margin: 3px; font-size: 16px; border-radius: 6px; border: none; }
  button { background: #444; color: #fff; cursor: pointer; }
  button:active { background: #07f; }
  input[type=text] { width: 55%; }
  input[type=number] { width: 70px; }
  input[type=range] { width: 60%; vertical-align: middle; padding: 0; }
  .row { margin: 6px 0; }
  input[type=color] { height: 38px; width: 50px; padding: 2px; }
  .grid { display: grid; grid-template-columns: repeat(4, 1fr); gap: 4px; }
  .grid button { margin: 0; padding: 6px 2px; font-size: 13px; }
  .grid button span { display: block; font-size: 22px; }
  .dpad { display: grid; grid-template-columns: repeat(3, 70px); grid-template-rows: repeat(3, 60px); gap: 6px; justify-content: center; margin: 8px auto; }
  .dpad button { font-size: 26px; margin: 0; touch-action: manipulation; }
  table { border-collapse: collapse; margin: 10px auto; }
  td { width: 16px; height: 16px; border: 1px solid #444; background-color: black; }
  .red { background: #b22; }
  .green { background: #2a2; }
</style>
</head>
<body>
<h2>ESP32 LED Matrix</h2>

<div class="card">
  <h3>Display</h3>
  <div class="row">&#128262; Brightness
    <input type="range" id="bri" min="5" max="255" value="80" oninput="setBri(false)" onchange="setBri(true)">
    <span id="briVal">31%</span></div>
  <div class="row">&#9193; Scroll speed
    <input type="range" id="spd" min="1" max="10" value="7" oninput="setSpd(false)" onchange="setSpd(true)">
    <span id="spdVal">7</span></div>
</div>

<div class="card">
  <h3>Scrolling text</h3>
  <input type="text" id="sentenceInput" placeholder="Enter text">
  <button onclick="sendSentence()">Send</button><br>
  <button id="freezeBtn" onclick="toggleFreeze()">&#9208;&#65039; Freeze Scroll</button>
  <button class="red" onclick="resetText()">&#8635; Reset</button><br>
  <span>Style:</span>
  <button onclick="post('/style?mode=rb')">&#128308;&#128309; Red &amp; Blue</button>
  <button onclick="setSolid()">&#127912; Solid</button>
  <button onclick="post('/style?mode=rainbow')">&#127752; Rainbow Flow</button><br>
  <span>Color (Solid / Breathing):</span>
  <input type="color" id="colorPicker" value="#ff0000">
</div>

<div class="card">
  <h3>Pixel art presets</h3>
  <div class="grid" id="artGrid"></div>
  <div style="margin-top:8px">
    <span>Slideshow every</span>
    <input type="number" id="slideInterval" min="1" max="60" value="5"> <span>s</span>
    <button class="green" onclick="startSlideshow()">&#9654; Start</button>
    <button onclick="post('/mode?m=text')">&#9209; Stop</button>
  </div>
</div>

<div class="card">
  <h3>Ambient lighting</h3>
  <button onclick="post('/mode?m=fire')">&#128293; Fire</button>
  <button onclick="post('/mode?m=rainbow')">&#127752; Rainbow Wave</button>
  <button onclick="startBreath()">&#128168; Breathing</button><br>
  <button onclick="post('/mode?m=text')">&#128172; Back to text</button>
</div>

<div class="card">
  <h3>Snake</h3>
  <button class="green" onclick="post('/snake?start=1')">&#127918; New game</button>
  <button onclick="post('/mode?m=text')">&#10006; Quit</button>
  <div class="dpad">
    <span></span><button onpointerdown="snakeDir('U')">&#9650;</button><span></span>
    <button onpointerdown="snakeDir('L')">&#9664;</button><span></span><button onpointerdown="snakeDir('R')">&#9654;</button>
    <span></span><button onpointerdown="snakeDir('D')">&#9660;</button><span></span>
  </div>
  <small>Arrow keys also work on a computer.</small>
</div>

<div class="card">
  <h3>Draw pixels</h3>
  <input type="color" id="drawColor" value="#ff0000">
  <button onclick="clearTable()">Clear</button>
  <button onclick="sendData()">Draw Pixels</button>
  <table id="grid"></table>
</div>

<script>
var ART = [["&#10084;&#65039;","Heart"],["&#128512;","Smiley"],["&#128126;","Invader"],["&#128640;","Rocket"],["&#127794;","Tree"],
           ["&#127481;&#127475;","Tunisia"],["&#128993;","Pac-Man"],["&#128246;","Wi-Fi"],["&#127812;","Mushroom"],["&#9728;&#65039;","Sun"],
           ["&#128049;","Cat"],["&#9749;","Coffee"],["&#127925;","Music"],["&#129302;","Robot"],["&#127752;","Rainbow"],
           ["&#128128;","Skull"],["&#128031;","Fish"],["&#9889;","Bolt"],["&#127822;","Apple"],["&#128760;","UFO"]];
var frozen = false;

function post(url) { return fetch(url, { method: "POST" }); }

var lastSend = {};
function allow(name, final) {            // throttle slider spam to ~8 requests/s
  var now = Date.now();
  if (final || now - (lastSend[name] || 0) > 120) { lastSend[name] = now; return true; }
  return false;
}
function setBri(final) {
  var v = parseInt(document.getElementById("bri").value);
  document.getElementById("briVal").textContent = Math.round(v * 100 / 255) + "%";
  if (allow("bri", final)) post("/brightness?v=" + v);
}
function speedToMs(v) { return Math.round(420 - v * 38); }   // 1 = slow (382 ms) ... 10 = fast (40 ms)
function setSpd(final) {
  var v = parseInt(document.getElementById("spd").value);
  document.getElementById("spdVal").textContent = v;
  if (allow("spd", final)) post("/speed?ms=" + speedToMs(v));
}

function buildUI() {
  var g = document.getElementById("artGrid");
  ART.forEach(function (a, i) {
    var b = document.createElement("button");
    b.innerHTML = "<span>" + a[0] + "</span>" + a[1];
    b.onclick = function () { post("/preset?id=" + i); };
    g.appendChild(b);
  });
  var t = document.getElementById("grid");
  for (var i = 0; i < 7; i++) {
    var tr = document.createElement("tr");
    for (var j = 0; j < 17; j++) {
      var td = document.createElement("td");
      td.onclick = function () { colorCell(this); };
      tr.appendChild(td);
    }
    t.appendChild(tr);
  }
}

function updateFreeze() {
  document.getElementById("freezeBtn").innerHTML = frozen ? "&#9654;&#65039; Resume Scroll" : "&#9208;&#65039; Freeze Scroll";
}
function toggleFreeze() {
  fetch("/freeze", { method: "POST" }).then(function (r) { return r.text(); }).then(function (t) {
    frozen = (t === "frozen"); updateFreeze();
  });
}
function resetText() {
  post("/reset").then(function () {
    frozen = false; updateFreeze();
    document.getElementById("sentenceInput").value = "";
  });
}
function hexOf() { return document.getElementById("colorPicker").value.substring(1); }
function setSolid() { post("/style?mode=solid&color=" + hexOf()); }
function startBreath() { post("/mode?m=breath&color=" + hexOf()); }
function startSlideshow() {
  var v = parseInt(document.getElementById("slideInterval").value) || 5;
  v = Math.max(1, Math.min(60, v));
  document.getElementById("slideInterval").value = v;
  post("/slideshow?interval=" + v);
}
function snakeDir(d) { post("/snake?dir=" + d); }
document.addEventListener("keydown", function (e) {
  var m = { ArrowUp: "U", ArrowDown: "D", ArrowLeft: "L", ArrowRight: "R" }[e.key];
  if (m && document.activeElement.tagName !== "INPUT") { e.preventDefault(); snakeDir(m); }
});

function sendSentence() {
  var s = document.getElementById("sentenceInput").value;
  post("/sentence?text=" + encodeURIComponent(s)).then(function () { frozen = false; updateFreeze(); });
}

function colorCell(td) { td.style.backgroundColor = document.getElementById("drawColor").value; }
function clearTable() {
  var cells = document.getElementsByTagName("td");
  for (var i = 0; i < cells.length; i++) cells[i].style.backgroundColor = "black";
}
function sendData() {
  var cells = document.getElementsByTagName("td");
  var data = [];
  for (var i = 0; i < cells.length; i++) {
    var c = cells[i].style.backgroundColor;
    data.push((c == "black" || c == "") ? "b" : c);
  }
  var xhr = new XMLHttpRequest();
  xhr.open("POST", "/image", true);
  xhr.setRequestHeader("Content-Type", "application/json");
  xhr.send(JSON.stringify({ tableData: data }));
}

buildUI();
fetch("/state").then(function (r) { return r.json(); }).then(function (s) {
  frozen = s.frozen; updateFreeze();
  document.getElementById("bri").value = s.brightness;
  document.getElementById("briVal").textContent = Math.round(s.brightness * 100 / 255) + "%";
  var v = Math.max(1, Math.min(10, Math.round((420 - s.speed) / 38)));
  document.getElementById("spd").value = v;
  document.getElementById("spdVal").textContent = v;
}).catch(function () {});
</script>
</body>
</html>
)rawliteral";

// ================= SETUP / LOOP =================
void setup() {
  Serial.begin(9600);

  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  FastLED.setBrightness(brightness);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, MAX_MILLIAMPS);
  FastLED.clear();
  FastLED.show();

  randomSeed(esp_random());

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(ap_ssid, ap_password);

  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", htmlContent); });
  server.on("/state", HTTP_GET, handleState);
  server.on("/image", HTTP_POST, handlePost);
  server.on("/sentence", HTTP_POST, handleSentence);
  server.on("/reset", HTTP_POST, handleReset);
  server.on("/style", HTTP_POST, handleStyle);
  server.on("/freeze", HTTP_POST, handleFreeze);
  server.on("/preset", HTTP_POST, handlePreset);
  server.on("/slideshow", HTTP_POST, handleSlideshow);
  server.on("/mode", HTTP_POST, handleMode);
  server.on("/snake", HTTP_POST, handleSnake);
  server.on("/brightness", HTTP_POST, handleBrightness);
  server.on("/speed", HTTP_POST, handleSpeed);

  server.begin();
}

void loop() {
  server.handleClient();
  unsigned long now = millis();

  switch (mode) {
    case MODE_TEXT:
      if (currentSentence.length() == 0) break;
      if (scrollFrozen) {
        if (needRedraw) { renderText(); needRedraw = false; }
      } else if (now - lastFrame >= scrollSpeed) {
        lastFrame = now;
        needRedraw = false;
        offset -= 2;
        hueBase += 6;
        renderText();
      }
      break;

    case MODE_SLIDESHOW:
      if (now - lastFrame >= slideIntervalMs) {
        lastFrame = now;
        slideIdx = (slideIdx + 1) % NUM_PRESETS;
        drawPreset(slideIdx);
      }
      break;

    case MODE_SNAKE:
      if (snakeDead) {
        if (now - snakeDeadTime >= 2000) {
          snakeReset();
          lastFrame = now;
          snakeRender();
        } else if (now - lastFrame >= 200) {
          lastFrame = now;
          snakeFlash = !snakeFlash;
          snakeRender();
        }
      } else if (now - lastFrame >= (unsigned long)snakeStepMs) {
        lastFrame = now;
        snakeStep();
        snakeRender();
      }
      break;

    case MODE_FIRE:
      if (now - lastFrame >= 60) { lastFrame = now; fireFrame(); }
      break;

    case MODE_RAINBOW:
      if (now - lastFrame >= 40) { lastFrame = now; rainbowWaveFrame(); }
      break;

    case MODE_BREATH:
      if (now - lastFrame >= 30) { lastFrame = now; breathingFrame(); }
      break;

    case MODE_IMAGE:
    default:
      break;
  }
}
