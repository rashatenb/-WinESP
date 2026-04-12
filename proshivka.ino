#include <M5StickCPlus2.h>
#include <WiFi.h>
#include <EEPROM.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "time.h"

// ---------- НАСТРОЙКИ ----------
#define EEPROM_SIZE 256
#define THEME_ADDR 0
#define WIFI_SSID_ADDR 10
#define WIFI_PASS_ADDR 50
#define NEWS_URL_ADDR 90
#define EGG_SCORE_ADDR 170  // Новый адрес для счёта Egg

#define APP_COUNT 9
String appNames[APP_COUNT] = {"Info", "Bat", "Snake", "WiFi", "Accel", "News", "Egg", "Settings", "Themes"};
int currentApp = 0;

// Темы (5 тем)
int currentTheme = 0;
#define THEME_COUNT 5
String themeNames[THEME_COUNT] = {"Standart", "Temnaya", "Poloski", "Volny", "Kuby"};

// Для Змейки
int snakeX[100], snakeY[100];
int snakeLen = 3;
int foodX, foodY;
int dir = 0;
bool gameOver = false;
unsigned long lastMove = 0;
int gameSpeed = 200;

// Для Egg Clicker
int eggScore = 0;
bool eggCracked = false;

// Состояния
int currentScreen = 0;
int settingsPos = 0;
#define SETTINGS_COUNT 4
String settingsMenu[SETTINGS_COUNT] = {"Yarkost", "GMT", "News URL", "Nazad"};
int brightness = 100;
int gmtOffset = 3;
String newsURL = "http://192.168.1.100:8080/news.json";
unsigned long screenEnterTime = 0;

// Для выключения
bool pwrHeld = false;
unsigned long pwrStart = 0;
bool shuttingDown = false;
float lastRemaining = -1;
bool pwrForKeyboard = false;

// Блокировка
bool locked = true;
unsigned long lastActivity = 0;
#define SCREEN_LOCK 99

// WiFi состояние
int wifiState = 0;
int selectedNetwork = 0;
int numNetworks = 0;
String networkSSIDs[10];
bool wifiConnected = false;
unsigned long wifiStatusTime = 0;
bool timeSynced = false;
String savedSSID = "";
String savedPass = "";
bool wifiAutoConnectStarted = false;

// Клавиатура (6x6)
String password = "";
String keyboardChars = "1234567890qwertyuiopasdfghjklzxc.:/";
int keyIndex = 0;
const int keysPerRow = 6;
unsigned long bPressStart = 0;
bool bLongPressActive = false;
unsigned long aPressStart = 0;
bool aLongPressActive = false;
unsigned long aRepeatTimer = 0;

// Новости
String newsContent = "";
int newsScroll = 0;
unsigned long lastNewsScroll = 0;
bool newsLoading = false;

// NTP сервер
const char* ntpServer = "pool.ntp.org";

// Время
unsigned long lastTimeSync = 0;
bool rtcInitialized = false;

// Флаг для принудительной перерисовки
bool needFullRedraw = false;

// ---------- ФУНКЦИИ ДЛЯ ТЕМ ----------
uint16_t getThemeColor(int theme, bool isLockScreen) {
    if(theme == 0) return TFT_NAVY;
    if(theme == 1) return TFT_BLACK;
    return TFT_BLACK;
}

void drawThemeBackground(int theme, bool isLockScreen) {
    if(theme == 0 || theme == 1) {
        M5.Lcd.fillScreen(getThemeColor(theme, isLockScreen));
        return;
    }
    
    M5.Lcd.fillScreen(TFT_BLACK);
    
    if(theme == 2) {
        for(int x = 0; x < 240; x += 8) {
            M5.Lcd.fillRect(x, 0, 4, 135, TFT_NAVY);
        }
    }
    else if(theme == 3) {
        for(int x = 0; x < 240; x += 2) {
            int y = 67 + sin(x * 0.05) * 30;
            M5.Lcd.drawPixel(x, y, TFT_CYAN);
            M5.Lcd.drawPixel(x, y + 20, TFT_CYAN);
            M5.Lcd.drawPixel(x, y - 20, TFT_CYAN);
        }
    }
    else if(theme == 4) {
        for(int x = 0; x < 240; x += 12) {
            for(int y = 0; y < 135; y += 12) {
                if((x/12 + y/12) % 2 == 0) {
                    M5.Lcd.fillRect(x, y, 12, 12, TFT_BLUE);
                } else {
                    M5.Lcd.fillRect(x, y, 12, 12, TFT_PURPLE);
                }
            }
        }
    }
}

uint16_t getTextColor(int theme) {
    return TFT_WHITE;
}

// ---------- EEPROM ----------
void saveTheme(int theme) {
    EEPROM.write(THEME_ADDR, theme);
    EEPROM.commit();
}

int loadTheme() {
    int theme = EEPROM.read(THEME_ADDR);
    if(theme < 0 || theme >= THEME_COUNT) theme = 0;
    return theme;
}

void saveWiFiCredentials(String ssid, String pass) {
    for(int i = 0; i < 40; i++) {
        EEPROM.write(WIFI_SSID_ADDR + i, i < ssid.length() ? ssid[i] : 0);
        EEPROM.write(WIFI_PASS_ADDR + i, i < pass.length() ? pass[i] : 0);
    }
    EEPROM.commit();
}

void loadWiFiCredentials() {
    char ssid[40] = {0};
    char pass[40] = {0};
    for(int i = 0; i < 40; i++) {
        ssid[i] = EEPROM.read(WIFI_SSID_ADDR + i);
        pass[i] = EEPROM.read(WIFI_PASS_ADDR + i);
    }
    savedSSID = String(ssid);
    savedPass = String(pass);
}

void saveNewsURL(String url) {
    for(int i = 0; i < 80; i++) {
        EEPROM.write(NEWS_URL_ADDR + i, i < url.length() ? url[i] : 0);
    }
    EEPROM.commit();
}

void loadNewsURL() {
    char url[80] = {0};
    for(int i = 0; i < 80; i++) {
        url[i] = EEPROM.read(NEWS_URL_ADDR + i);
    }
    if(strlen(url) > 0) newsURL = String(url);
}

// Сохранение и загрузка счёта Egg
void saveEggScore(int score) {
    EEPROM.write(EGG_SCORE_ADDR, score & 0xFF);
    EEPROM.write(EGG_SCORE_ADDR + 1, (score >> 8) & 0xFF);
    EEPROM.write(EGG_SCORE_ADDR + 2, (score >> 16) & 0xFF);
    EEPROM.write(EGG_SCORE_ADDR + 3, (score >> 24) & 0xFF);
    EEPROM.commit();
}

int loadEggScore() {
    int score = EEPROM.read(EGG_SCORE_ADDR);
    score |= EEPROM.read(EGG_SCORE_ADDR + 1) << 8;
    score |= EEPROM.read(EGG_SCORE_ADDR + 2) << 16;
    score |= EEPROM.read(EGG_SCORE_ADDR + 3) << 24;
    return score;
}

void autoConnectWiFi() {
    if(savedSSID.length() > 0 && !wifiConnected && !wifiAutoConnectStarted) {
        wifiAutoConnectStarted = true;
        WiFi.begin(savedSSID.c_str(), savedPass.c_str());
    }
}

// ---------- СИНХРОНИЗАЦИЯ ВРЕМЕНИ (ТОЛЬКО ПО WiFi) ----------
void syncTime() {
    if(wifiConnected) {
        configTime(gmtOffset * 3600, 0, ntpServer);
        struct tm timeinfo;
        if(getLocalTime(&timeinfo)) {
            M5.Rtc.setDateTime(&timeinfo);
            timeSynced = true;
            rtcInitialized = true;
        }
    }
}

void initRTC() {
    if(!rtcInitialized) {
        M5.Rtc.setDateTime({{2026, 4, 12}, {12 + gmtOffset, 0, 0}});
        rtcInitialized = true;
    }
}

// ---------- НОВОСТИ ----------
void fetchNews() {
    if(!wifiConnected || newsLoading) return;
    newsLoading = true;
    
    HTTPClient http;
    http.begin(newsURL);
    http.setTimeout(5000);
    int httpCode = http.GET();
    
    if(httpCode == 200) {
        String payload = http.getString();
        StaticJsonDocument<4096> doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if(!error) {
            newsContent = "";
            JsonArray items = doc["news"].as<JsonArray>();
            for(JsonVariant item : items) {
                newsContent += "> " + item["title"].as<String>() + "\n";
                newsContent += "  " + item["date"].as<String>() + "\n\n";
            }
        } else {
            newsContent = "Oshibka formata JSON";
        }
    } else {
        newsContent = "Server nedostupen: " + String(httpCode);
    }
    http.end();
    newsLoading = false;
    newsScroll = 0;
}

// ---------- ЗАСТАВКА ----------
void bootSplash() {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(3);
    M5.Lcd.setCursor(30, 30);
    M5.Lcd.print("WInESP");
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(30, 60);
    M5.Lcd.print("by Assembler");
    M5.Lcd.drawRect(30, 90, 180, 15, TFT_WHITE);
    
    for(int i = 0; i <= 180; i += 5) {
        M5.Lcd.fillRect(32, 92, i, 11, TFT_GREEN);
        int dots = (i / 30) % 6;
        M5.Lcd.setTextSize(2);
        M5.Lcd.setCursor(30 + i/2, 115);
        String dotStr = "";
        for(int d=0; d<dots; d++) dotStr += ".";
        M5.Lcd.print(dotStr);
        delay(80);
    }
    M5.Lcd.setCursor(50, 115);
    M5.Lcd.print("DONE!");
    delay(800);
}

// ---------- ЭКРАН БЛОКИРОВКИ ----------
void drawLockScreen() {
    drawThemeBackground(currentTheme, true);
    
    auto dt = M5.Rtc.getDateTime();
    int hour = dt.time.hours;
    
    uint16_t textColor = getTextColor(currentTheme);
    M5.Lcd.setTextColor(textColor);
    M5.Lcd.setTextSize(4);
    M5.Lcd.setCursor(30, 30);
    M5.Lcd.printf("%02d:%02d", hour, dt.time.minutes);
    
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(40, 70);
    M5.Lcd.printf("%02d/%02d/%04d", dt.date.date, dt.date.month, dt.date.year);
    
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(10, 110);
    M5.Lcd.print("A/B = Vhod");
    
    int bat = M5.Power.getBatteryLevel();
    M5.Lcd.setCursor(180, 5);
    M5.Lcd.printf("%d%%", bat);
    M5.Lcd.drawRoundRect(177, 3, 58, 12, 3, textColor);
}

// ---------- ЛЕНТА ----------
void drawAppRibbon() {
    drawThemeBackground(currentTheme, false);
    
    uint16_t textColor = getTextColor(currentTheme);
    M5.Lcd.setTextColor(textColor);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(10, 5);
    M5.Lcd.print("WInESP");
    
    auto dt = M5.Rtc.getDateTime();
    int hour = dt.time.hours;
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(120, 5);
    M5.Lcd.printf("%02d:%02d GMT+%d", hour, dt.time.minutes, gmtOffset);
    
    int bat = M5.Power.getBatteryLevel();
    M5.Lcd.setCursor(195, 5);
    M5.Lcd.printf("%d%%", bat);
    M5.Lcd.drawRoundRect(192, 3, 45, 12, 3, textColor);
    
    M5.Lcd.setCursor(120, 18);
    M5.Lcd.printf("%02d/%02d/%04d", dt.date.date, dt.date.month, dt.date.year);
    
    if(wifiConnected) {
        M5.Lcd.setTextColor(TFT_GREEN);
        M5.Lcd.setCursor(10, 18);
        M5.Lcd.print("WiFi");
    }
    
    for(int i = 0; i < APP_COUNT; i++) {
        int x = 10 + i * 70 - (currentApp * 70);
        int y = 50;
        
        if(x < -50 || x > 240) continue;
        
        if(i == currentApp) {
            M5.Lcd.fillRoundRect(x-3, y-3, 56, 56, 8, TFT_YELLOW);
        }
        
        uint16_t col;
        if(i == 0) col = TFT_CYAN;
        else if(i == 1) col = TFT_ORANGE;
        else if(i == 2) col = TFT_PINK;
        else if(i == 3) col = TFT_BLUE;
        else if(i == 4) col = TFT_PURPLE;
        else if(i == 5) col = TFT_YELLOW;
        else if(i == 6) col = TFT_GREEN;
        else if(i == 7) col = TFT_DARKGRAY;
        else col = TFT_MAGENTA;
        
        M5.Lcd.fillRoundRect(x, y, 50, 50, 8, col);
        
        M5.Lcd.setTextColor(TFT_WHITE);
        M5.Lcd.setTextSize(1);
        M5.Lcd.setCursor(x + 5, y + 55);
        M5.Lcd.print(appNames[i]);
    }
    
    M5.Lcd.fillRoundRect(5, 115, 230, 15, 5, TFT_DARKGREEN);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setCursor(10, 117);
    M5.Lcd.print("< B > = Listat'  A = Zapusk  PWR(2.5s) = Vyk");
}

// ---------- ТЕМЫ ----------
void drawThemes() {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(10, 10);
    M5.Lcd.print("Temy");
    
    for(int i = 0; i < THEME_COUNT; i++) {
        M5.Lcd.setTextSize(1);
        M5.Lcd.setCursor(20, 40 + i * 18);
        if(i == currentTheme) M5.Lcd.print("> ");
        else M5.Lcd.print("  ");
        M5.Lcd.print(themeNames[i]);
    }
    
    M5.Lcd.setCursor(0, 125);
    M5.Lcd.println("B=Listat'  A=Vybrat'");
}

// ---------- НАСТРОЙКИ ----------
void drawSettings() {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(10, 10);
    M5.Lcd.print("Nastroiki");
    
    for(int i = 0; i < SETTINGS_COUNT; i++) {
        M5.Lcd.setTextSize(1);
        M5.Lcd.setCursor(20, 40 + i * 18);
        if(i == settingsPos) M5.Lcd.print("> ");
        else M5.Lcd.print("  ");
        
        if(i == 0) M5.Lcd.printf("%s: %d%%", settingsMenu[i].c_str(), brightness);
        else if(i == 1) M5.Lcd.printf("%s: +%d", settingsMenu[i].c_str(), gmtOffset);
        else M5.Lcd.print(settingsMenu[i]);
    }
    
    M5.Lcd.setCursor(0, 125);
    M5.Lcd.println("B=Listat'  A=Vybrat'");
}

// ---------- NEWS URL INPUT ----------
void drawNewsURLInput() {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println("News URL:");
    M5.Lcd.println(newsURL);
    M5.Lcd.println("");
    
    int startX = 10;
    int startY = 45;
    int btnW = 34;
    int btnH = 14;
    int cols = 6;
    
    for(int i = 0; i < keyboardChars.length(); i++) {
        int row = i / cols;
        int col = i % cols;
        int x = startX + col * (btnW + 2);
        int y = startY + row * (btnH + 2);
        
        if(y > 100) continue;
        
        if(i == keyIndex) {
            M5.Lcd.fillRect(x-1, y-1, btnW+2, btnH+2, TFT_YELLOW);
        }
        M5.Lcd.fillRect(x, y, btnW, btnH, TFT_DARKGRAY);
        M5.Lcd.setTextColor(TFT_WHITE);
        M5.Lcd.setCursor(x + 12, y + 3);
        M5.Lcd.print(keyboardChars[i]);
    }
    
    M5.Lcd.setCursor(0, 115);
    M5.Lcd.println("A:Next B:Add PWR:Del HoldB:Save");
}

// ---------- О СИСТЕМЕ ----------
void drawAbout() {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println("WInESP by Assembler");
    M5.Lcd.println("");
    M5.Lcd.println("M5StickC Plus2");
    M5.Lcd.println("ESP32-S3 @ 240MHz");
    M5.Lcd.println("PSRAM: 8MB, Flash: 16MB");
    M5.Lcd.println("Disp: 135x240 TFT");
    M5.Lcd.println("");
    M5.Lcd.println("t.me/WinEspAssembler");
    M5.Lcd.println("");
    M5.Lcd.println("<< B = Nazad >>");
}

// ---------- ИНФО ----------
void drawInfo() {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println("WInESP");
    M5.Lcd.setTextSize(1);
    M5.Lcd.println("by Assembler");
    M5.Lcd.println("");
    M5.Lcd.println("Sistema:");
    M5.Lcd.println("M5StickC Plus2");
    M5.Lcd.println("ESP32-S3 @ 240MHz");
    M5.Lcd.println("PSRAM: 8MB");
    M5.Lcd.println("Flash: 16MB");
    M5.Lcd.println("Disp: 135x240 TFT");
    M5.Lcd.println("");
    M5.Lcd.println("Batareya: " + String(M5.Power.getBatteryLevel()) + "%");
    M5.Lcd.println("GMT: +" + String(gmtOffset));
    if(wifiConnected) {
        M5.Lcd.println("WiFi: " + WiFi.SSID());
        M5.Lcd.println("IP: " + WiFi.localIP().toString());
    }
    M5.Lcd.println("");
    M5.Lcd.println("Telegram:");
    M5.Lcd.println("t.me/WinEspAssembler");
    M5.Lcd.println("");
    M5.Lcd.println("<< B = Nazad >>");
}

// ---------- БАТАРЕЯ ----------
void drawBat() {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println("Batareya:");
    int bat = M5.Power.getBatteryLevel();
    M5.Lcd.setTextSize(3);
    M5.Lcd.setCursor(50, 50);
    M5.Lcd.printf("%d%%", bat);
    M5.Lcd.drawRect(20, 90, 200, 30, TFT_WHITE);
    M5.Lcd.fillRect(22, 92, bat * 2 - 4, 26, bat > 20 ? TFT_GREEN : TFT_RED);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(0, 125);
    M5.Lcd.println("<< B = Nazad >>");
}

// ---------- WIFI СКАНЕР И ПОДКЛЮЧЕНИЕ ----------
void scanWiFiNetworks() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    numNetworks = WiFi.scanNetworks();
    for(int i = 0; i < min(numNetworks, 10); i++) {
        networkSSIDs[i] = WiFi.SSID(i);
    }
}

void drawWiFiScanner() {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println("WiFi Scaner:");
    M5.Lcd.printf("Naideno: %d setei\n", numNetworks);
    
    for(int i = 0; i < min(numNetworks, 5); i++) {
        M5.Lcd.setCursor(0, 25 + i * 12);
        M5.Lcd.printf("%d: %s (%d)", i+1, networkSSIDs[i].c_str(), WiFi.RSSI(i));
    }
    
    M5.Lcd.setCursor(0, 125);
    M5.Lcd.println("A=Podkl  PWR=Nazad");
}

void drawWiFiSelect() {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println("Vyberi set':");
    
    for(int i = 0; i < min(numNetworks, 8); i++) {
        M5.Lcd.setCursor(10, 20 + i * 14);
        if(i == selectedNetwork) M5.Lcd.print("> ");
        else M5.Lcd.print("  ");
        M5.Lcd.print(networkSSIDs[i]);
    }
    
    M5.Lcd.setCursor(0, 125);
    M5.Lcd.println("B=Listat'  A=Vybrat'  PWR=Nazad");
}

void drawPasswordInput() {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println("WiFi: " + networkSSIDs[selectedNetwork]);
    M5.Lcd.print("Pass: ");
    M5.Lcd.println(password);
    
    int startX = 10;
    int startY = 45;
    int btnW = 34;
    int btnH = 14;
    int cols = 6;
    
    for(int i = 0; i < keyboardChars.length(); i++) {
        int row = i / cols;
        int col = i % cols;
        int x = startX + col * (btnW + 2);
        int y = startY + row * (btnH + 2);
        
        if(y > 100) continue;
        
        if(i == keyIndex) {
            M5.Lcd.fillRect(x-1, y-1, btnW+2, btnH+2, TFT_YELLOW);
        }
        M5.Lcd.fillRect(x, y, btnW, btnH, TFT_DARKGRAY);
        M5.Lcd.setTextColor(TFT_WHITE);
        M5.Lcd.setCursor(x + 12, y + 3);
        M5.Lcd.print(keyboardChars[i]);
    }
    
    M5.Lcd.setCursor(0, 115);
    M5.Lcd.println("A:Next B:Add PWR:Del HoldB:OK");
}

void drawWiFiConnecting() {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println("Podklyucheniye k:");
    M5.Lcd.println(networkSSIDs[selectedNetwork]);
    M5.Lcd.println("");
    
    if(wifiConnected) {
        M5.Lcd.setTextColor(TFT_GREEN);
        M5.Lcd.println("Podklyucheno!");
        M5.Lcd.println("IP: " + WiFi.localIP().toString());
        syncTime();
    } else {
        M5.Lcd.println("Podklyucheniye...");
    }
    
    M5.Lcd.setCursor(0, 125);
    M5.Lcd.println("B=Nazad");
}

// ---------- EGG CLICKER (СОХРАНЕНИЕ SCORE) ----------
void resetEgg() {
    eggScore = 0;
    eggCracked = false;
    saveEggScore(eggScore);
}

void drawEgg() {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.printf("Score: %d", eggScore);
    
    if(!eggCracked) {
        M5.Lcd.fillCircle(120, 70, 25, TFT_WHITE);
        M5.Lcd.fillCircle(115, 60, 8, TFT_YELLOW);
        M5.Lcd.setCursor(0, 125);
        M5.Lcd.println("A=Razbit'  B=Vyhod");
    } else {
        M5.Lcd.fillCircle(110, 70, 15, TFT_WHITE);
        M5.Lcd.fillCircle(130, 70, 15, TFT_WHITE);
        M5.Lcd.fillCircle(120, 80, 10, TFT_YELLOW);
        M5.Lcd.setCursor(0, 125);
        M5.Lcd.println("A=Novoe  B=Vyhod");
    }
}

// ---------- НОВОСТИ ----------
void drawNews() {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println("=== NOVOSTI ===");
    
    if(newsContent.length() == 0) {
        M5.Lcd.setCursor(0, 20);
        M5.Lcd.println("Zagruzka...");
    } else {
        int y = 20;
        int startLine = newsScroll;
        int lineCount = 0;
        String temp = "";
        
        for(int i = 0; i < newsContent.length(); i++) {
            temp += newsContent[i];
            if(newsContent[i] == '\n') {
                if(lineCount >= startLine) {
                    M5.Lcd.setCursor(0, y);
                    M5.Lcd.print(temp);
                    y += 12;
                    if(y > 120) break;
                }
                temp = "";
                lineCount++;
            }
        }
    }
    
    M5.Lcd.setCursor(0, 125);
    M5.Lcd.println("A=Obnovit'  B=Nazad");
}

// ---------- АКСЕЛЕРОМЕТР ----------
void drawAccel() {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println("Akselerometr:");
    
    float ax, ay, az;
    M5.Imu.getAccel(&ax, &ay, &az);
    
    M5.Lcd.setCursor(0, 20);
    M5.Lcd.printf("X: %.2f", ax);
    M5.Lcd.setCursor(0, 35);
    M5.Lcd.printf("Y: %.2f", ay);
    M5.Lcd.setCursor(0, 50);
    M5.Lcd.printf("Z: %.2f", az);
    
    int x = 120 + ax * 50;
    int y = 90 + ay * 50;
    x = constrain(x, 10, 230);
    y = constrain(y, 20, 125);
    
    M5.Lcd.fillCircle(x, y, 5, TFT_GREEN);
    M5.Lcd.drawCircle(120, 90, 50, TFT_WHITE);
    
    M5.Lcd.setCursor(0, 125);
    M5.Lcd.println("<< B = Nazad >>");
}

// ---------- ЗМЕЙКА ----------
void resetGame() {
    snakeLen = 3;
    dir = 0;
    gameOver = false;
    snakeX[0] = 120; snakeY[0] = 60;
    snakeX[1] = 110; snakeY[1] = 60;
    snakeX[2] = 100; snakeY[2] = 60;
    foodX = random(10, 230);
    foodY = random(20, 110);
}

void drawGame() {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.drawRect(0, 0, 240, 135, TFT_WHITE);
    for(int i=0; i<snakeLen; i++) M5.Lcd.fillRect(snakeX[i], snakeY[i], 8, 8, TFT_GREEN);
    M5.Lcd.fillCircle(foodX, foodY, 4, TFT_RED);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setCursor(5, 5);
    M5.Lcd.printf("Score: %d", snakeLen-3);
    if(gameOver) {
        M5.Lcd.setTextSize(2);
        M5.Lcd.setCursor(50, 50);
        M5.Lcd.print("GAME OVER");
    }
}

void moveSnake() {
    if(gameOver) return;
    for(int i=snakeLen-1; i>0; i--) {
        snakeX[i] = snakeX[i-1];
        snakeY[i] = snakeY[i-1];
    }
    if(dir == 0) snakeX[0] += 10;
    else if(dir == 1) snakeY[0] += 10;
    else if(dir == 2) snakeX[0] -= 10;
    else snakeY[0] -= 10;
    
    if(snakeX[0] < 5 || snakeX[0] > 230 || snakeY[0] < 15 || snakeY[0] > 125) gameOver = true;
    if(abs(snakeX[0]-foodX) < 10 && abs(snakeY[0]-foodY) < 10) {
        snakeLen++;
        foodX = random(10, 230);
        foodY = random(20, 110);
    }
    for(int i=1; i<snakeLen; i++) {
        if(snakeX[0]==snakeX[i] && snakeY[0]==snakeY[i]) gameOver = true;
    }
}

// ---------- ТАЙМЕР ВЫКЛЮЧЕНИЯ ----------
void checkPowerOff() {
    if(pwrForKeyboard) return;
    
    if(M5.BtnPWR.isPressed()) {
        if(!pwrHeld) {
            pwrHeld = true;
            pwrStart = millis();
            shuttingDown = false;
            lastRemaining = -1;
        } else {
            unsigned long elapsed = millis() - pwrStart;
            
            if(elapsed >= 2500 && !shuttingDown) {
                shuttingDown = true;
                M5.Lcd.fillScreen(TFT_BLACK);
                M5.Lcd.setTextColor(TFT_RED);
                M5.Lcd.setTextSize(2);
                M5.Lcd.setCursor(50, 50);
                M5.Lcd.print("POKA!");
                delay(1000);
                M5.Power.deepSleep();
            } else if(elapsed < 2500) {
                float remaining = 2.5 - (elapsed / 1000.0);
                if(abs(remaining - lastRemaining) > 0.05) {
                    M5.Lcd.fillRect(0, 0, 240, 20, TFT_RED);
                    M5.Lcd.setTextColor(TFT_WHITE, TFT_RED);
                    M5.Lcd.setTextSize(1);
                    M5.Lcd.setCursor(5, 4);
                    M5.Lcd.printf("Vyklyucheniye cherez: %.1f sec", remaining);
                    lastRemaining = remaining;
                }
            }
        }
    } else {
        if(pwrHeld && !shuttingDown) {
            needFullRedraw = true;
        }
        pwrHeld = false;
        shuttingDown = false;
        lastRemaining = -1;
    }
}

// ---------- КНОПКА B ----------
bool isBShortPressed() {
    return M5.BtnB.wasPressed();
}

// ---------- SETUP ----------
void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Lcd.setRotation(3);
    
    EEPROM.begin(EEPROM_SIZE);
    currentTheme = loadTheme();
    loadWiFiCredentials();
    loadNewsURL();
    eggScore = loadEggScore();  // Загружаем сохранённый счёт
    
    initRTC();
    
    bootSplash();
    resetGame();
    eggCracked = false;  // Яйцо всегда целое при старте
    locked = true;
    drawLockScreen();
    currentScreen = SCREEN_LOCK;
    lastActivity = millis();
}

// ---------- LOOP ----------
void loop() {
    M5.update();
    checkPowerOff();
    
    if(pwrHeld) return;
    
    bool bShort = isBShortPressed();
    
    if(millis() - lastTimeSync > 3600000) {
        if(wifiConnected) syncTime();
        lastTimeSync = millis();
    }
    
    if(needFullRedraw) {
        needFullRedraw = false;
        if(currentScreen == 0) drawAppRibbon();
        else if(currentScreen == 1) drawInfo();
        else if(currentScreen == 3) drawBat();
        else if(currentScreen == 4) drawGame();
        else if(currentScreen == 5) {
            if(wifiState == 0) drawWiFiScanner();
            else if(wifiState == 1) drawWiFiSelect();
            else if(wifiState == 2) drawPasswordInput();
            else drawWiFiConnecting();
        }
        else if(currentScreen == 6) drawAccel();
        else if(currentScreen == 7) drawNews();
        else if(currentScreen == 8) drawEgg();
        else if(currentScreen == 9) drawSettings();
        else if(currentScreen == 10) drawThemes();
        else if(currentScreen == 11) drawAbout();
        else if(currentScreen == 12) drawNewsURLInput();
    }
    
    if(locked) {
        if(M5.BtnA.wasPressed() || M5.BtnB.wasPressed()) {
            locked = false;
            currentScreen = 0;
            screenEnterTime = millis();
            drawAppRibbon();
            lastActivity = millis();
            autoConnectWiFi();
        }
        static int lastLockMin = -1;
        auto dt = M5.Rtc.getDateTime();
        if(dt.time.minutes != lastLockMin) {
            drawLockScreen();
            lastLockMin = dt.time.minutes;
        }
        return;
    }
    
    if(wifiAutoConnectStarted && !wifiConnected) {
        if(WiFi.status() == WL_CONNECTED) {
            wifiConnected = true;
            wifiAutoConnectStarted = false;
            syncTime();
            if(currentScreen == 0) drawAppRibbon();
        } else {
            static unsigned long autoConnectTimer = 0;
            if(autoConnectTimer == 0) autoConnectTimer = millis();
            if(millis() - autoConnectTimer > 10000) {
                wifiAutoConnectStarted = false;
                WiFi.disconnect();
                autoConnectTimer = 0;
            }
        }
    }
    
    // ЛЕНТА (ВХОД БЕЗ ЗАДЕРЖКИ)
    if(currentScreen == 0) {
        if(bShort) {
            currentApp = (currentApp + 1) % APP_COUNT;
            drawAppRibbon();
            lastActivity = millis();
        }
        if(M5.BtnA.wasPressed()) {
            if(currentApp == 0) { currentScreen = 1; screenEnterTime = millis(); drawInfo(); }
            else if(currentApp == 1) { currentScreen = 3; screenEnterTime = millis(); drawBat(); }
            else if(currentApp == 2) { currentScreen = 4; resetGame(); drawGame(); screenEnterTime = millis(); }
            else if(currentApp == 3) { 
                currentScreen = 5; 
                screenEnterTime = millis();
                wifiState = 0;
                scanWiFiNetworks();
                drawWiFiScanner();
            }
            else if(currentApp == 4) { currentScreen = 6; drawAccel(); screenEnterTime = millis(); }
            else if(currentApp == 5) { 
                currentScreen = 7; 
                screenEnterTime = millis();
                newsContent = "";
                fetchNews();
                drawNews();
            }
            else if(currentApp == 6) { currentScreen = 8; eggCracked = false; drawEgg(); screenEnterTime = millis(); }
            else if(currentApp == 7) { 
                currentScreen = 9; 
                screenEnterTime = millis();
                settingsPos = 0; 
                drawSettings(); 
            }
            else if(currentApp == 8) { 
                currentScreen = 10; 
                screenEnterTime = millis();
                drawThemes(); 
            }
            lastActivity = millis();
        }
    }
    
    // WiFi
    if(currentScreen == 5) {
        pwrForKeyboard = true;
        
        if(wifiState == 0) {
            if(M5.BtnPWR.wasPressed()) { currentScreen = 0; screenEnterTime = millis(); drawAppRibbon(); lastActivity = millis(); }
            if(M5.BtnA.wasPressed()) { wifiState = 1; selectedNetwork = 0; drawWiFiSelect(); lastActivity = millis(); }
        }
        else if(wifiState == 1) {
            if(M5.BtnPWR.wasPressed()) { wifiState = 0; drawWiFiScanner(); lastActivity = millis(); }
            if(M5.BtnA.wasPressed() && millis() - screenEnterTime > 1000) { wifiState = 2; password = ""; keyIndex = 0; drawPasswordInput(); lastActivity = millis(); }
            if(bShort) { selectedNetwork = (selectedNetwork + 1) % min(numNetworks, 8); drawWiFiSelect(); lastActivity = millis(); }
            if(M5.BtnA.isPressed()) {
                if(aPressStart == 0) aPressStart = millis();
                if(!aLongPressActive && millis() - aPressStart > 500) {
                    aLongPressActive = true; aRepeatTimer = millis();
                    selectedNetwork = (selectedNetwork - 1 + min(numNetworks, 8)) % min(numNetworks, 8);
                    drawWiFiSelect(); lastActivity = millis();
                }
                if(aLongPressActive && millis() - aRepeatTimer > 150) {
                    aRepeatTimer = millis();
                    selectedNetwork = (selectedNetwork - 1 + min(numNetworks, 8)) % min(numNetworks, 8);
                    drawWiFiSelect(); lastActivity = millis();
                }
            } else { aPressStart = 0; aLongPressActive = false; }
        }
        else if(wifiState == 2) {
            if(M5.BtnPWR.wasPressed()) { if(password.length() > 0) password = password.substring(0, password.length() - 1); drawPasswordInput(); lastActivity = millis(); }
            if(M5.BtnA.wasPressed()) { keyIndex = (keyIndex + 1) % keyboardChars.length(); drawPasswordInput(); lastActivity = millis(); }
            if(M5.BtnA.isPressed()) {
                if(aPressStart == 0) aPressStart = millis();
                if(!aLongPressActive && millis() - aPressStart > 500) {
                    aLongPressActive = true; aRepeatTimer = millis();
                    keyIndex = (keyIndex - 1 + keyboardChars.length()) % keyboardChars.length();
                    drawPasswordInput(); lastActivity = millis();
                }
                if(aLongPressActive && millis() - aRepeatTimer > 150) {
                    aRepeatTimer = millis();
                    keyIndex = (keyIndex - 1 + keyboardChars.length()) % keyboardChars.length();
                    drawPasswordInput(); lastActivity = millis();
                }
            } else { aPressStart = 0; aLongPressActive = false; }
            
            if(M5.BtnB.isPressed()) {
                if(bPressStart == 0) { bPressStart = millis(); bLongPressActive = false; }
                else if(!bLongPressActive && millis() - bPressStart > 1000) {
                    bLongPressActive = true; wifiState = 3; drawWiFiConnecting(); wifiStatusTime = millis();
                    WiFi.begin(networkSSIDs[selectedNetwork].c_str(), password.c_str());
                    saveWiFiCredentials(networkSSIDs[selectedNetwork], password);
                    lastActivity = millis();
                }
            } else {
                if(bPressStart > 0 && !bLongPressActive) { password += keyboardChars[keyIndex]; drawPasswordInput(); lastActivity = millis(); }
                bPressStart = 0; bLongPressActive = false;
            }
        }
        else {
            if(M5.BtnPWR.wasPressed()) { wifiState = 0; drawWiFiScanner(); lastActivity = millis(); }
            if(WiFi.status() == WL_CONNECTED && !wifiConnected) { wifiConnected = true; drawWiFiConnecting(); syncTime(); }
            if(millis() - wifiStatusTime > 15000 && !wifiConnected) { wifiState = 0; drawWiFiScanner(); }
        }
    }
    
    // EGG CLICKER (С СОХРАНЕНИЕМ)
    if(currentScreen == 8) {
        if(bShort) { 
            saveEggScore(eggScore);  // Сохраняем при выходе
            currentScreen = 0; 
            screenEnterTime = millis(); 
            drawAppRibbon(); 
            lastActivity = millis(); 
        }
        
        if(!eggCracked) {
            if(M5.BtnA.wasPressed() && millis() - screenEnterTime > 1000) {
                eggScore++;
                saveEggScore(eggScore);  // Сохраняем после каждого разбития
                eggCracked = true;
                drawEgg();
                lastActivity = millis();
            }
        } else {
            if(M5.BtnA.wasPressed() && millis() - screenEnterTime > 1000) {
                eggCracked = false;
                drawEgg();
                lastActivity = millis();
            }
        }
    }
    
    // NEWS URL INPUT
    if(currentScreen == 12) {
        pwrForKeyboard = true;
        if(M5.BtnPWR.wasPressed()) { if(newsURL.length() > 0) newsURL = newsURL.substring(0, newsURL.length() - 1); drawNewsURLInput(); lastActivity = millis(); }
        if(M5.BtnA.wasPressed()) { keyIndex = (keyIndex + 1) % keyboardChars.length(); drawNewsURLInput(); lastActivity = millis(); }
        if(M5.BtnA.isPressed()) {
            if(aPressStart == 0) aPressStart = millis();
            if(!aLongPressActive && millis() - aPressStart > 500) {
                aLongPressActive = true; aRepeatTimer = millis();
                keyIndex = (keyIndex - 1 + keyboardChars.length()) % keyboardChars.length();
                drawNewsURLInput(); lastActivity = millis();
            }
            if(aLongPressActive && millis() - aRepeatTimer > 150) {
                aRepeatTimer = millis();
                keyIndex = (keyIndex - 1 + keyboardChars.length()) % keyboardChars.length();
                drawNewsURLInput(); lastActivity = millis();
            }
        } else { aPressStart = 0; aLongPressActive = false; }
        
        if(M5.BtnB.isPressed()) {
            if(bPressStart == 0) { bPressStart = millis(); bLongPressActive = false; }
            else if(!bLongPressActive && millis() - bPressStart > 1000) {
                bLongPressActive = true; saveNewsURL(newsURL); currentScreen = 9; drawSettings(); lastActivity = millis();
            }
        } else {
            if(bPressStart > 0 && !bLongPressActive) { newsURL += keyboardChars[keyIndex]; drawNewsURLInput(); lastActivity = millis(); }
            bPressStart = 0; bLongPressActive = false;
        }
    }
    
    // НОВОСТИ
    if(currentScreen == 7) {
        if(bShort) { currentScreen = 0; screenEnterTime = millis(); drawAppRibbon(); lastActivity = millis(); }
        if(M5.BtnA.wasPressed() && millis() - screenEnterTime > 1000) { newsContent = ""; fetchNews(); drawNews(); lastActivity = millis(); }
        if(M5.BtnB.isPressed()) {
            static unsigned long lastScroll = 0;
            if(millis() - lastScroll > 200) { newsScroll++; drawNews(); lastScroll = millis(); lastActivity = millis(); }
        }
        if(millis() - lastNewsScroll > 3000) { newsScroll++; drawNews(); lastNewsScroll = millis(); }
    }
    
    // ТЕМЫ
    if(currentScreen == 10) {
        if(bShort) {
            currentTheme = (currentTheme + 1) % THEME_COUNT;
            saveTheme(currentTheme);
            drawThemes();
            lastActivity = millis();
        }
        if(M5.BtnA.wasPressed() && millis() - screenEnterTime > 1000) {
            currentScreen = 0;
            screenEnterTime = millis();
            drawAppRibbon();
            lastActivity = millis();
        }
        if(M5.BtnPWR.wasPressed()) {
            currentScreen = 0;
            screenEnterTime = millis();
            drawAppRibbon();
            lastActivity = millis();
        }
    }
    
    // НАСТРОЙКИ
    if(currentScreen == 9) {
        if(M5.BtnPWR.wasPressed()) { currentScreen = 0; screenEnterTime = millis(); drawAppRibbon(); lastActivity = millis(); return; }
        if(bShort) { settingsPos = (settingsPos + 1) % SETTINGS_COUNT; drawSettings(); lastActivity = millis(); }
        if(M5.BtnA.wasPressed() && millis() - screenEnterTime > 1000) {
            if(settingsPos == 0) {
                brightness = (brightness + 10) % 110;
                M5.Lcd.setBrightness(brightness);
                drawSettings();
            } else if(settingsPos == 1) {
                gmtOffset = (gmtOffset + 1) % 14;
                timeSynced = false;
                syncTime();
                drawSettings();
            } else if(settingsPos == 2) {
                currentScreen = 12;
                keyIndex = 0;
                drawNewsURLInput();
            } else if(settingsPos == 3) {
                currentScreen = 0;
                screenEnterTime = millis();
                drawAppRibbon();
            }
            lastActivity = millis();
        }
    }
    
    // ИНФО / БАТАРЕЯ / ACCEL
    if(currentScreen == 1 || currentScreen == 3 || currentScreen == 6) {
        if(bShort) { currentScreen = 0; screenEnterTime = millis(); drawAppRibbon(); lastActivity = millis(); }
    }
    
    // SNAKE
    if(currentScreen == 4) {
        if(bShort) { currentScreen = 0; screenEnterTime = millis(); drawAppRibbon(); lastActivity = millis(); }
        if(!gameOver && millis() - lastMove > gameSpeed) { moveSnake(); drawGame(); lastMove = millis(); }
        if(M5.BtnA.wasPressed()) { dir = (dir + 1) % 4; lastActivity = millis(); }
    }
    
    // АКСЕЛЕРОМЕТР
    if(currentScreen == 6) { drawAccel(); delay(50); }
    
    // ОБНОВЛЕНИЕ НА РАБОЧЕМ СТОЛЕ
    if(currentScreen == 0) {
        static int lastBat = -1;
        static int lastMin = -1;
        int bat = M5.Power.getBatteryLevel();
        auto dt = M5.Rtc.getDateTime();
        if(bat != lastBat || dt.time.minutes != lastMin) { drawAppRibbon(); lastBat = bat; lastMin = dt.time.minutes; }
    }
    
    // АВТОБЛОКИРОВКА
    if(millis() - lastActivity > 30000 && !locked) { locked = true; currentScreen = SCREEN_LOCK; drawLockScreen(); }
    
    if(M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || M5.BtnPWR.wasPressed()) lastActivity = millis();
    
    if(currentScreen != 5 && currentScreen != 12) pwrForKeyboard = false;
    
    delay(15);
}
