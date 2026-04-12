#include <M5StickCPlus2.h>
#include <WiFi.h>
#include <EEPROM.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "time.h"

// ---------- ЗВУКИ (НЕБЛОКИРУЮЩИЕ) ----------
#define SPEAKER_PIN 2
#define MAX_SOUND_QUEUE 20

// Ноты
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_D5  587
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_G5  784
#define NOTE_A5  880
#define NOTE_B5  988
#define NOTE_C6  1047
#define NOTE_D6  1175
#define NOTE_E6  1319
#define NOTE_G6  1568
#define NOTE_REST 0

// Очередь звуков
int soundQueue[MAX_SOUND_QUEUE][2];
int queueCount = 0;
int currentSound = 0;
unsigned long toneEndTime = 0;
bool tonePlaying = false;

// ---------- НАСТРОЙКИ EEPROM ----------
#define EEPROM_SIZE 512
#define THEME_ADDR 0
#define WIFI_SSID_ADDR 10
#define WIFI_PASS_ADDR 70
#define NEWS_URL_ADDR 130
#define EGG_SCORE_ADDR 210
#define WIFI_SAVED_FLAG_ADDR 250
#define GMT_OFFSET_ADDR 251
#define SNAKE_HIGHSCORE_ADDR 260
#define FLAPPY_HIGHSCORE_ADDR 264

#define APP_COUNT 11
String appNames[APP_COUNT] = {"Info", "Bat", "Snake", "Flappy", "Egg", "WiFi", "Accel", "News", "Power", "Settings", "Themes"};
int currentApp = 0;

// Темы (5 тем)
int currentTheme = 0;
#define THEME_COUNT 5
String themeNames[THEME_COUNT] = {"Standart", "Temnaya", "Poloski", "Volny", "Kuby"};

// ---------- ЗМЕЙКА ----------
int snakeX[200], snakeY[200];
int snakeLen = 3;
int foodX, foodY;
int bonusX = -1, bonusY = -1;
int dir = 0;
bool gameOver = false;
unsigned long lastMove = 0;
int gameSpeed = 200;
int snakeScore = 0;
int snakeHighScore = 0;
int snakeLevel = 1;
bool hasShield = false;
int shieldTimer = 0;

#define MAX_OBSTACLES 10
int obstacleX[MAX_OBSTACLES];
int obstacleY[MAX_OBSTACLES];
int obstacleCount = 0;

// ---------- FLAPPY BIRD ----------
float birdY = 60;
float birdVelocity = 0;
const float GRAVITY = 0.4;
const float JUMP_FORCE = -5.0;
const int BIRD_X = 40;
const int BIRD_SIZE = 8;

struct Pipe {
    int x;
    int gapY;
    bool passed;
};
Pipe pipes[3];
int flappyScore = 0;
int flappyHighScore = 0;
bool flappyGameOver = false;
bool flappyStarted = false;
unsigned long lastPipeMove = 0;
int pipeSpeed = 3;
int pipeGap = 45;

// ---------- EGG CLICKER ----------
int eggScore = 0;
bool eggCracked = false;
unsigned long lastEggAnim = 0;

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
String networkSSIDs[20];
bool wifiConnected = false;
unsigned long wifiStatusTime = 0;
bool timeSynced = false;
String savedSSID = "";
String savedPass = "";
bool wifiAutoConnectStarted = false;
unsigned long wifiConnectStartTime = 0;
String wifiErrorMessage = "";

// Клавиатура
String password = "";
String keyboardChars = "1234567890qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM.:/";
int keyIndex = 0;
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
bool newsFirstLoad = true;

// NTP серверы
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const char* ntpServer3 = "time.google.com";

// Время
unsigned long lastTimeSync = 0;
bool rtcInitialized = false;

// Power меню
int powerMenuPos = 0;
#define POWER_MENU_COUNT 4
String powerMenuItems[POWER_MENU_COUNT] = {"Reboot", "Power Off", "Deep Sleep", "Back"};

// Флаг для принудительной перерисовки
bool needFullRedraw = false;

// ---------- НЕБЛОКИРУЮЩИЕ ФУНКЦИИ ЗВУКА ----------
void queueSound(int freq, int duration) {
    if(queueCount < MAX_SOUND_QUEUE) {
        soundQueue[queueCount][0] = freq;
        soundQueue[queueCount][1] = duration;
        queueCount++;
    }
}

void playToneAsync(int freq, int duration) {
    if(freq > 0) {
        tone(SPEAKER_PIN, freq, duration);
    }
    toneEndTime = millis() + duration;
    tonePlaying = true;
}

void updateSound() {
    if(tonePlaying && millis() > toneEndTime) {
        noTone(SPEAKER_PIN);
        tonePlaying = false;
    }
}

void playNextInQueue() {
    if(!tonePlaying && currentSound < queueCount) {
        playToneAsync(soundQueue[currentSound][0], soundQueue[currentSound][1]);
        currentSound++;
    }
    if(currentSound >= queueCount && !tonePlaying) {
        queueCount = 0;
        currentSound = 0;
    }
}

void playEatSound() {
    queueSound(NOTE_C6, 30);
    queueSound(NOTE_E6, 30);
    queueSound(NOTE_G6, 40);
}

void playBonusSound() {
    queueSound(NOTE_G5, 50);
    queueSound(NOTE_C6, 50);
    queueSound(NOTE_E6, 80);
    queueSound(NOTE_G6, 100);
}

void playJumpSound() {
    queueSound(NOTE_C5, 50);
    queueSound(NOTE_E5, 40);
    queueSound(NOTE_G5, 50);
}

void playGameOverSound() {
    queueSound(NOTE_G4, 150);
    queueSound(NOTE_E4, 150);
    queueSound(NOTE_C4, 200);
    queueSound(NOTE_REST, 100);
    queueSound(NOTE_C4, 150);
    queueSound(NOTE_D4, 150);
    queueSound(NOTE_E4, 200);
}

void playEggCrackSound() {
    queueSound(NOTE_C5, 30);
    queueSound(NOTE_REST, 20);
    queueSound(NOTE_C5, 30);
    queueSound(NOTE_REST, 20);
    queueSound(NOTE_G4, 50);
    queueSound(NOTE_E4, 60);
}

void playClickSound() {
    queueSound(NOTE_C6, 15);
}

void playUnlockSound() {
    queueSound(NOTE_E5, 50);
    queueSound(NOTE_G5, 60);
    queueSound(NOTE_C6, 80);
}

void playStartupSound() {
    queueSound(NOTE_C5, 100);
    queueSound(NOTE_E5, 100);
    queueSound(NOTE_G5, 100);
    queueSound(NOTE_C6, 150);
    queueSound(NOTE_REST, 50);
    queueSound(NOTE_C6, 100);
    queueSound(NOTE_G5, 100);
    queueSound(NOTE_E5, 100);
    queueSound(NOTE_C5, 150);
    queueSound(NOTE_REST, 80);
    queueSound(NOTE_G5, 80);
    queueSound(NOTE_A5, 80);
    queueSound(NOTE_B5, 80);
    queueSound(NOTE_C6, 200);
}

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

void saveSnakeHighScore(int score) {
    EEPROM.write(SNAKE_HIGHSCORE_ADDR, score & 0xFF);
    EEPROM.write(SNAKE_HIGHSCORE_ADDR + 1, (score >> 8) & 0xFF);
    EEPROM.commit();
}

int loadSnakeHighScore() {
    int score = EEPROM.read(SNAKE_HIGHSCORE_ADDR);
    score |= EEPROM.read(SNAKE_HIGHSCORE_ADDR + 1) << 8;
    if(score < 0 || score > 10000) score = 0;
    return score;
}

void saveFlappyHighScore(int score) {
    EEPROM.write(FLAPPY_HIGHSCORE_ADDR, score & 0xFF);
    EEPROM.write(FLAPPY_HIGHSCORE_ADDR + 1, (score >> 8) & 0xFF);
    EEPROM.commit();
}

int loadFlappyHighScore() {
    int score = EEPROM.read(FLAPPY_HIGHSCORE_ADDR);
    score |= EEPROM.read(FLAPPY_HIGHSCORE_ADDR + 1) << 8;
    if(score < 0 || score > 10000) score = 0;
    return score;
}

void saveWiFiCredentials(String ssid, String pass) {
    for(int i = 0; i < 60; i++) {
        EEPROM.write(WIFI_SSID_ADDR + i, i < ssid.length() ? ssid[i] : 0);
    }
    for(int i = 0; i < 60; i++) {
        EEPROM.write(WIFI_PASS_ADDR + i, i < pass.length() ? pass[i] : 0);
    }
    EEPROM.write(WIFI_SAVED_FLAG_ADDR, 0xAA);
    EEPROM.commit();
    
    savedSSID = ssid;
    savedPass = pass;
}

void loadWiFiCredentials() {
    char ssid[60] = {0};
    char pass[60] = {0};
    
    uint8_t flag = EEPROM.read(WIFI_SAVED_FLAG_ADDR);
    
    if(flag == 0xAA) {
        for(int i = 0; i < 60; i++) {
            ssid[i] = EEPROM.read(WIFI_SSID_ADDR + i);
            pass[i] = EEPROM.read(WIFI_PASS_ADDR + i);
        }
        savedSSID = String(ssid);
        savedPass = String(pass);
    } else {
        savedSSID = "";
        savedPass = "";
    }
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
    if(strlen(url) > 0 && url[0] == 'h') {
        newsURL = String(url);
    }
}

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
    if(score < 0 || score > 1000000) score = 0;
    return score;
}

void saveGMTOffset(int offset) {
    EEPROM.write(GMT_OFFSET_ADDR, offset);
    EEPROM.commit();
}

void loadGMTOffset() {
    int saved = EEPROM.read(GMT_OFFSET_ADDR);
    if(saved >= 0 && saved <= 13) {
        gmtOffset = saved;
    }
}

// ---------- ВРЕМЯ ----------
bool isLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int daysInMonth(int month, int year) {
    if(month == 2) {
        return isLeapYear(year) ? 29 : 28;
    }
    if(month == 4 || month == 6 || month == 9 || month == 11) {
        return 30;
    }
    return 31;
}

m5::rtc_datetime_t getLocalDateTime() {
    auto utc = M5.Rtc.getDateTime();
    
    int localHour = utc.time.hours + gmtOffset;
    int dayOffset = 0;
    
    while(localHour >= 24) {
        localHour -= 24;
        dayOffset++;
    }
    
    m5::rtc_datetime_t local = utc;
    local.time.hours = localHour;
    
    if(dayOffset > 0) {
        for(int i = 0; i < dayOffset; i++) {
            local.date.date++;
            if(local.date.date > daysInMonth(local.date.month, local.date.year)) {
                local.date.date = 1;
                local.date.month++;
                if(local.date.month > 12) {
                    local.date.month = 1;
                    local.date.year++;
                }
            }
        }
    }
    
    return local;
}

bool connectToSavedWiFi() {
    if(savedSSID.length() == 0 || savedPass.length() == 0) {
        wifiErrorMessage = "Net sokhranennykh setei";
        return false;
    }
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(savedSSID.c_str(), savedPass.c_str());
    
    int attempts = 0;
    while(WiFi.status() != WL_CONNECTED && attempts < 25) {
        delay(500);
        attempts++;
    }
    
    if(WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        wifiErrorMessage = "";
        syncTime();
        return true;
    } else {
        int status = WiFi.status();
        if(status == WL_CONNECT_FAILED) {
            wifiErrorMessage = "Nevernyi parol' ili slabyi signal";
        } else if(status == WL_NO_SSID_AVAIL) {
            wifiErrorMessage = "Set' ne naidena";
        } else {
            wifiErrorMessage = "Oshibka podklyucheniya";
        }
        WiFi.disconnect();
        return false;
    }
}

void syncTime() {
    if(!wifiConnected) return;
    
    configTzTime("UTC", ntpServer1, ntpServer2, ntpServer3);
    
    struct tm timeinfo;
    int attempts = 0;
    bool synced = false;
    
    while(!synced && attempts < 30) {
        if(getLocalTime(&timeinfo, 500)) {
            if(timeinfo.tm_sec > 0 || attempts > 5) {
                synced = true;
                break;
            }
        }
        attempts++;
        delay(500);
    }
    
    if(synced) {
        M5.Rtc.setDateTime(&timeinfo);
        timeSynced = true;
        rtcInitialized = true;
    }
}

void initRTC() {
    if(!rtcInitialized) {
        const char* compile_date = __DATE__;
        const char* compile_time = __TIME__;
        
        char month_str[4];
        int day, year;
        sscanf(compile_date, "%s %d %d", month_str, &day, &year);
        
        const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", 
                                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        int month = 1;
        for(int i = 0; i < 12; i++) {
            if(strcmp(month_str, months[i]) == 0) {
                month = i + 1;
                break;
            }
        }
        
        int hour, minute, second;
        sscanf(compile_time, "%d:%d:%d", &hour, &minute, &second);
        
        int utcHour = hour - gmtOffset;
        int dayOffset = 0;
        
        while(utcHour < 0) {
            utcHour += 24;
            dayOffset--;
        }
        
        m5::rtc_date_t date;
        date.year = year;
        date.month = month;
        date.date = day;
        
        if(dayOffset < 0) {
            date.date--;
            if(date.date < 1) {
                date.month--;
                if(date.month < 1) {
                    date.month = 12;
                    date.year--;
                }
                date.date = daysInMonth(date.month, date.year);
            }
        }
        
        m5::rtc_time_t time;
        time.hours = utcHour;
        time.minutes = minute;
        time.seconds = second;
        
        M5.Rtc.setDateTime(&date, &time);
        rtcInitialized = true;
    }
}

void fetchNews() {
    if(!wifiConnected) {
        newsContent = "> WiFi ne podklyuchen!\n  Zaydi v WiFi prilozhenie\n\n";
        return;
    }
    
    if(newsLoading) return;
    
    newsLoading = true;
    
    HTTPClient http;
    http.begin(newsURL);
    http.setTimeout(8000);
    
    int httpCode = http.GET();
    
    if(httpCode > 0) {
        if(httpCode == 200) {
            String payload = http.getString();
            
            DynamicJsonDocument doc(8192);
            DeserializationError error = deserializeJson(doc, payload);
            
            if(!error) {
                JsonArray items = doc["news"].as<JsonArray>();
                int newsCount = items.size();
                
                if(newsCount == 0) {
                    newsContent = "> Net novostei\n  Dobav'te ih na servere\n\n";
                } else {
                    newsContent = "";
                    for(JsonVariant item : items) {
                        const char* title = item["title"];
                        const char* date = item["date"];
                        
                        if(title && date) {
                            newsContent += "> ";
                            newsContent += String(title);
                            newsContent += "\n  ";
                            newsContent += String(date);
                            newsContent += "\n\n";
                        }
                    }
                }
            } else {
                newsContent = "> Oshibka JSON\n  ";
                newsContent += error.c_str();
                newsContent += "\n\n";
            }
        } else {
            newsContent = "> Server nedostupen\n  Kod: " + String(httpCode) + "\n\n";
        }
    } else {
        newsContent = "> Oshibka podklyucheniya\n  " + http.errorToString(httpCode) + "\n\n";
    }
    
    http.end();
    newsLoading = false;
    newsScroll = 0;
    newsFirstLoad = false;
}

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
    
    playStartupSound();
    
    for(int i = 0; i <= 180; i += 5) {
        M5.Lcd.fillRect(32, 92, i, 11, TFT_GREEN);
        updateSound();
        playNextInQueue();
        delay(40);
    }
    
    M5.Lcd.setCursor(50, 115);
    M5.Lcd.print("DONE!");
    delay(500);
}

void drawLockScreen() {
    drawThemeBackground(currentTheme, true);
    
    auto local = getLocalDateTime();
    
    uint16_t textColor = getTextColor(currentTheme);
    M5.Lcd.setTextColor(textColor);
    M5.Lcd.setTextSize(4);
    M5.Lcd.setCursor(30, 30);
    M5.Lcd.printf("%02d:%02d", local.time.hours, local.time.minutes);
    
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(40, 70);
    M5.Lcd.printf("%02d/%02d/%04d", local.date.date, local.date.month, local.date.year);
    
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(10, 110);
    M5.Lcd.print("A/B = Vhod");
    
    int bat = M5.Power.getBatteryLevel();
    M5.Lcd.setCursor(180, 5);
    M5.Lcd.printf("%d%%", bat);
    M5.Lcd.drawRoundRect(177, 3, 58, 12, 3, textColor);
    
    if(wifiConnected) {
        M5.Lcd.setTextColor(TFT_GREEN);
        M5.Lcd.setCursor(5, 5);
        M5.Lcd.print("WiFi");
    }
}

void drawAppRibbon() {
    drawThemeBackground(currentTheme, false);
    
    uint16_t textColor = getTextColor(currentTheme);
    M5.Lcd.setTextColor(textColor);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(10, 5);
    M5.Lcd.print("WInESP");
    
    auto local = getLocalDateTime();
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(120, 5);
    M5.Lcd.printf("%02d:%02d GMT+%d", local.time.hours, local.time.minutes, gmtOffset);
    
    int bat = M5.Power.getBatteryLevel();
    M5.Lcd.setCursor(195, 5);
    M5.Lcd.printf("%d%%", bat);
    M5.Lcd.drawRoundRect(192, 3, 45, 12, 3, textColor);
    
    M5.Lcd.setCursor(120, 18);
    M5.Lcd.printf("%02d/%02d/%04d", local.date.date, local.date.month, local.date.year);
    
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
        else if(i == 2) col = TFT_GREEN;
        else if(i == 3) col = TFT_PINK;
        else if(i == 4) col = TFT_YELLOW;
        else if(i == 5) col = TFT_BLUE;
        else if(i == 6) col = TFT_PURPLE;
        else if(i == 7) col = TFT_DARKGRAY;
        else if(i == 8) col = TFT_RED;
        else if(i == 9) col = TFT_MAGENTA;
        else col = TFT_CYAN;
        
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

void drawNewsURLInput() {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println("News URL:");
    M5.Lcd.println(newsURL);
    M5.Lcd.println("");
    
    int startX = 5;
    int startY = 45;
    int btnW = 22;
    int btnH = 14;
    int cols = 10;
    
    for(int i = 0; i < keyboardChars.length(); i++) {
        int row = i / cols;
        int col = i % cols;
        int x = startX + col * (btnW + 1);
        int y = startY + row * (btnH + 1);
        
        if(y > 105) continue;
        
        if(i == keyIndex) {
            M5.Lcd.fillRect(x-1, y-1, btnW+2, btnH+2, TFT_YELLOW);
        }
        M5.Lcd.fillRect(x, y, btnW, btnH, TFT_DARKGRAY);
        M5.Lcd.setTextColor(TFT_WHITE);
        M5.Lcd.setCursor(x + 5, y + 3);
        M5.Lcd.print(keyboardChars[i]);
    }
    
    M5.Lcd.setCursor(0, 115);
    M5.Lcd.println("A:Next B:Add PWR:Del HoldB:Save");
}

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
    
    auto utc = M5.Rtc.getDateTime();
    auto local = getLocalDateTime();
    M5.Lcd.print("UTC: ");
    M5.Lcd.print(utc.time.hours);
    M5.Lcd.print(":");
    if(utc.time.minutes < 10) M5.Lcd.print("0");
    M5.Lcd.println(utc.time.minutes);
    
    M5.Lcd.print("Local: ");
    M5.Lcd.print(local.time.hours);
    M5.Lcd.print(":");
    if(local.time.minutes < 10) M5.Lcd.print("0");
    M5.Lcd.println(local.time.minutes);
    
    if(wifiConnected) {
        M5.Lcd.println("WiFi: " + WiFi.SSID());
        M5.Lcd.println("IP: " + WiFi.localIP().toString());
    } else {
        M5.Lcd.println("WiFi: otklyuchen");
    }
    M5.Lcd.println("");
    M5.Lcd.println("Telegram:");
    M5.Lcd.println("t.me/WinEspAssembler");
    M5.Lcd.println("");
    M5.Lcd.println("<< B = Nazad >>");
}

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

void scanWiFiNetworks() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    numNetworks = WiFi.scanNetworks();
    for(int i = 0; i < min(numNetworks, 20); i++) {
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
    for(int i = 0; i < password.length(); i++) M5.Lcd.print("*");
    M5.Lcd.println("");
    
    int startX = 5;
    int startY = 45;
    int btnW = 22;
    int btnH = 14;
    int cols = 10;
    
    for(int i = 0; i < keyboardChars.length(); i++) {
        int row = i / cols;
        int col = i % cols;
        int x = startX + col * (btnW + 1);
        int y = startY + row * (btnH + 1);
        
        if(y > 105) continue;
        
        if(i == keyIndex) {
            M5.Lcd.fillRect(x-1, y-1, btnW+2, btnH+2, TFT_YELLOW);
        }
        M5.Lcd.fillRect(x, y, btnW, btnH, TFT_DARKGRAY);
        M5.Lcd.setTextColor(TFT_WHITE);
        M5.Lcd.setCursor(x + 5, y + 3);
        M5.Lcd.print(keyboardChars[i]);
    }
    
    M5.Lcd.setCursor(0, 115);
    M5.Lcd.println("A:Next B:Add PWR:Del HoldB:Connect");
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
        M5.Lcd.println("");
        M5.Lcd.setTextColor(TFT_WHITE);
        M5.Lcd.println("Dannye sokhraneny v EEPROM!");
        syncTime();
    } else if(wifiErrorMessage.length() > 0) {
        M5.Lcd.setTextColor(TFT_RED);
        M5.Lcd.println("Oshibka!");
        M5.Lcd.println(wifiErrorMessage);
        M5.Lcd.setTextColor(TFT_WHITE);
        M5.Lcd.println("");
        M5.Lcd.println("Prover'te parol'");
    } else {
        M5.Lcd.println("Podklyucheniye...");
        static int dots = 0;
        for(int i = 0; i < dots; i++) M5.Lcd.print(".");
        dots = (dots + 1) % 4;
    }
    
    M5.Lcd.setCursor(0, 125);
    M5.Lcd.println("B=Nazad");
}

void drawEgg() {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.printf("Clicks: %d", eggScore);
    
    if(!eggCracked) {
        int pulse = 25 + sin(millis() * 0.005) * 3;
        M5.Lcd.fillCircle(120, 70, pulse, TFT_WHITE);
        M5.Lcd.fillCircle(115, 60, 8, TFT_YELLOW);
        M5.Lcd.setCursor(0, 125);
        M5.Lcd.println("A=Klik  B=Vyhod");
    } else {
        int offset = sin(millis() * 0.01) * 5;
        M5.Lcd.fillCircle(110 + offset, 70 - offset/2, 15, TFT_WHITE);
        M5.Lcd.fillCircle(130 - offset, 70 + offset/2, 15, TFT_WHITE);
        M5.Lcd.fillCircle(120, 80, 10, TFT_YELLOW);
        
        for(int i = 0; i < 5; i++) {
            int px = 120 + sin(millis() * 0.01 + i) * 20;
            int py = 70 + cos(millis() * 0.01 + i) * 15;
            M5.Lcd.drawPixel(px, py, TFT_YELLOW);
        }
        
        M5.Lcd.setCursor(0, 125);
        M5.Lcd.println("A=Novoe  B=Vyhod");
    }
}

void drawNews() {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.println("=== NOVOSTI ===");
    
    if(!wifiConnected) {
        M5.Lcd.setTextColor(TFT_RED);
        M5.Lcd.setCursor(0, 12);
        M5.Lcd.println("WiFi otklyuchen!");
        M5.Lcd.setTextColor(TFT_WHITE);
    }
    
    if(newsContent.length() == 0) {
        M5.Lcd.setCursor(0, 25);
        M5.Lcd.println("Zagruzka novostei...");
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

// ---------- ПРИЛОЖЕНИЕ POWER ----------
void drawPowerMenu() {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_RED);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(50, 20);
    M5.Lcd.println("⚡ POWER ⚡");
    
    M5.Lcd.drawLine(10, 45, 230, 45, TFT_DARKGRAY);
    
    for(int i = 0; i < POWER_MENU_COUNT; i++) {
        M5.Lcd.setTextSize(1);
        M5.Lcd.setCursor(40, 60 + i * 22);
        
        if(i == powerMenuPos) {
            M5.Lcd.setTextColor(TFT_YELLOW);
            M5.Lcd.print("> ");
        } else {
            M5.Lcd.setTextColor(TFT_WHITE);
            M5.Lcd.print("  ");
        }
        
        if(i == 0) M5.Lcd.print("🔄 Reboot");
        else if(i == 1) M5.Lcd.print("⏻ Power Off");
        else if(i == 2) M5.Lcd.print("😴 Deep Sleep");
        else M5.Lcd.print("↩ Back");
    }
    
    // Информация о батарее
    M5.Lcd.setTextColor(TFT_GREEN);
    M5.Lcd.setCursor(10, 120);
    M5.Lcd.printf("Battery: %d%%", M5.Power.getBatteryLevel());
    
    M5.Lcd.setTextColor(TFT_DARKGRAY);
    M5.Lcd.setCursor(0, 130);
    M5.Lcd.println("B=Select  A=Back");
}

void rebootDevice() {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_GREEN);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(30, 50);
    M5.Lcd.println("REBOOTING...");
    delay(1000);
    ESP.restart();
}

void powerOffDevice() {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_RED);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(30, 50);
    M5.Lcd.println("POWER OFF");
    delay(1000);
    M5.Power.deepSleep();
}

void deepSleepDevice() {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_YELLOW);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(20, 40);
    M5.Lcd.println("DEEP SLEEP");
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(30, 70);
    M5.Lcd.println("Press PWR to wake");
    delay(1500);
    M5.Power.deepSleep();
}

// ---------- ЗМЕЙКА ФУНКЦИИ ----------
void generateObstacles(int level) {
    obstacleCount = level * 2;
    if(obstacleCount > MAX_OBSTACLES) obstacleCount = MAX_OBSTACLES;
    
    for(int i = 0; i < obstacleCount; i++) {
        bool valid;
        do {
            valid = true;
            obstacleX[i] = random(4, 20) * 10 + 5;
            obstacleY[i] = random(3, 11) * 10 + 5;
            
            for(int j = 0; j < snakeLen; j++) {
                if(abs(obstacleX[i] - snakeX[j]) < 10 && abs(obstacleY[i] - snakeY[j]) < 10) {
                    valid = false;
                    break;
                }
            }
        } while(!valid);
    }
}

void generateFood() {
    bool valid;
    do {
        valid = true;
        foodX = random(2, 22) * 10 + 5;
        foodY = random(2, 11) * 10 + 5;
        
        for(int i = 0; i < snakeLen; i++) {
            if(abs(foodX - snakeX[i]) < 10 && abs(foodY - snakeY[i]) < 10) {
                valid = false;
                break;
            }
        }
        
        for(int i = 0; i < obstacleCount; i++) {
            if(abs(foodX - obstacleX[i]) < 10 && abs(foodY - obstacleY[i]) < 10) {
                valid = false;
                break;
            }
        }
    } while(!valid);
}

void generateBonus() {
    if(random(100) < 20) {
        bool valid;
        do {
            valid = true;
            bonusX = random(2, 22) * 10 + 5;
            bonusY = random(2, 11) * 10 + 5;
            
            for(int i = 0; i < snakeLen; i++) {
                if(abs(bonusX - snakeX[i]) < 10 && abs(bonusY - snakeY[i]) < 10) {
                    valid = false;
                    break;
                }
            }
            
            for(int i = 0; i < obstacleCount; i++) {
                if(abs(bonusX - obstacleX[i]) < 10 && abs(bonusY - obstacleY[i]) < 10) {
                    valid = false;
                    break;
                }
            }
            
            if(abs(bonusX - foodX) < 10 && abs(bonusY - foodY) < 10) {
                valid = false;
            }
        } while(!valid);
    }
}

void resetSnake() {
    snakeLen = 3;
    dir = 0;
    gameOver = false;
    snakeScore = 0;
    snakeLevel = 1;
    gameSpeed = 200;
    hasShield = false;
    shieldTimer = 0;
    
    snakeX[0] = 120; snakeY[0] = 60;
    snakeX[1] = 110; snakeY[1] = 60;
    snakeX[2] = 100; snakeY[2] = 60;
    
    generateObstacles(snakeLevel);
    generateFood();
    bonusX = -1; bonusY = -1;
}

void drawSnake() {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.drawRect(0, 0, 240, 135, TFT_WHITE);
    
    for(int i = 0; i < obstacleCount; i++) {
        M5.Lcd.fillRect(obstacleX[i]-5, obstacleY[i]-5, 10, 10, TFT_DARKGRAY);
    }
    
    M5.Lcd.fillCircle(foodX, foodY, 4, TFT_RED);
    
    if(bonusX != -1) {
        M5.Lcd.fillCircle(bonusX, bonusY, 5, TFT_GOLD);
    }
    
    for(int i = 0; i < snakeLen; i++) {
        if(hasShield && i == 0) {
            M5.Lcd.fillRect(snakeX[i]-3, snakeY[i]-3, 8, 8, TFT_CYAN);
            M5.Lcd.drawRect(snakeX[i]-5, snakeY[i]-5, 12, 12, TFT_YELLOW);
        } else {
            M5.Lcd.fillRect(snakeX[i]-3, snakeY[i]-3, 8, 8, TFT_GREEN);
        }
    }
    
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setCursor(5, 5);
    M5.Lcd.printf("Lvl:%d Score:%d", snakeLevel, snakeScore);
    M5.Lcd.setCursor(160, 5);
    M5.Lcd.printf("HS:%d", snakeHighScore);
    
    if(gameOver) {
        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(TFT_RED);
        M5.Lcd.setCursor(40, 50);
        M5.Lcd.print("GAME OVER");
        M5.Lcd.setTextSize(1);
        M5.Lcd.setCursor(50, 80);
        M5.Lcd.print("A - restart");
        M5.Lcd.setCursor(50, 95);
        M5.Lcd.print("B - menu");
    }
    
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(0, 125);
    M5.Lcd.println("A=Povorot  B=Vyhod");
}

void moveSnake() {
    if(gameOver) return;
    
    for(int i = snakeLen - 1; i > 0; i--) {
        snakeX[i] = snakeX[i - 1];
        snakeY[i] = snakeY[i - 1];
    }
    
    if(dir == 0) snakeX[0] += 10;
    else if(dir == 1) snakeY[0] += 10;
    else if(dir == 2) snakeX[0] -= 10;
    else snakeY[0] -= 10;
    
    if(!hasShield) {
        if(snakeX[0] < 5 || snakeX[0] > 230 || snakeY[0] < 15 || snakeY[0] > 125) {
            gameOver = true;
            playGameOverSound();
        }
        
        for(int i = 0; i < obstacleCount; i++) {
            if(abs(snakeX[0] - obstacleX[i]) < 10 && abs(snakeY[0] - obstacleY[i]) < 10) {
                gameOver = true;
                playGameOverSound();
            }
        }
        
        for(int i = 1; i < snakeLen; i++) {
            if(snakeX[0] == snakeX[i] && snakeY[0] == snakeY[i]) {
                gameOver = true;
                playGameOverSound();
            }
        }
    }
    
    if(abs(snakeX[0] - foodX) < 10 && abs(snakeY[0] - foodY) < 10) {
        snakeLen++;
        snakeScore += 10;
        playEatSound();
        
        if(snakeScore > snakeHighScore) {
            snakeHighScore = snakeScore;
            saveSnakeHighScore(snakeHighScore);
        }
        
        if(snakeLen % 5 == 0) {
            snakeLevel++;
            gameSpeed = max(80, 200 - snakeLevel * 20);
            generateObstacles(snakeLevel);
        }
        
        generateFood();
        generateBonus();
    }
    
    if(bonusX != -1 && abs(snakeX[0] - bonusX) < 10 && abs(snakeY[0] - bonusY) < 10) {
        snakeScore += 30;
        hasShield = true;
        shieldTimer = 50;
        bonusX = -1;
        bonusY = -1;
        playBonusSound();
        
        if(snakeScore > snakeHighScore) {
            snakeHighScore = snakeScore;
            saveSnakeHighScore(snakeHighScore);
        }
    }
    
    if(hasShield) {
        shieldTimer--;
        if(shieldTimer <= 0) {
            hasShield = false;
        }
    }
}

// ---------- FLAPPY BIRD ФУНКЦИИ ----------
void resetFlappy() {
    birdY = 60;
    birdVelocity = 0;
    flappyScore = 0;
    flappyGameOver = false;
    flappyStarted = false;
    
    for(int i = 0; i < 3; i++) {
        pipes[i].x = 280 + i * 100;
        pipes[i].gapY = random(35, 100);
        pipes[i].passed = false;
    }
}

void drawFlappy() {
    M5.Lcd.fillScreen(TFT_SKYBLUE);
    
    for(int i = 0; i < 3; i++) {
        if(pipes[i].x > -20) {
            M5.Lcd.fillRect(pipes[i].x, 0, 20, pipes[i].gapY - pipeGap/2, TFT_GREEN);
            M5.Lcd.fillRect(pipes[i].x - 2, pipes[i].gapY - pipeGap/2 - 10, 24, 10, TFT_DARKGREEN);
            
            M5.Lcd.fillRect(pipes[i].x, pipes[i].gapY + pipeGap/2, 20, 135 - (pipes[i].gapY + pipeGap/2), TFT_GREEN);
            M5.Lcd.fillRect(pipes[i].x - 2, pipes[i].gapY + pipeGap/2, 24, 10, TFT_DARKGREEN);
        }
    }
    
    M5.Lcd.fillCircle(BIRD_X, (int)birdY, BIRD_SIZE, TFT_YELLOW);
    M5.Lcd.fillCircle(BIRD_X + 3, (int)birdY - 2, 3, TFT_WHITE);
    M5.Lcd.fillCircle(BIRD_X + 4, (int)birdY - 3, 1, TFT_BLACK);
    M5.Lcd.fillTriangle(BIRD_X + 6, (int)birdY - 1, BIRD_X + 10, (int)birdY, BIRD_X + 6, (int)birdY + 1, TFT_ORANGE);
    
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(10, 10);
    M5.Lcd.print(flappyScore);
    M5.Lcd.setCursor(160, 10);
    M5.Lcd.print("HS:");
    M5.Lcd.print(flappyHighScore);
    
    if(!flappyStarted && !flappyGameOver) {
        M5.Lcd.setTextSize(1);
        M5.Lcd.setCursor(60, 50);
        M5.Lcd.print("A - Jump");
        M5.Lcd.setCursor(60, 65);
        M5.Lcd.print("B - Exit");
    }
    
    if(flappyGameOver) {
        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(TFT_RED);
        M5.Lcd.setCursor(40, 50);
        M5.Lcd.print("GAME OVER");
        M5.Lcd.setTextSize(1);
        M5.Lcd.setTextColor(TFT_WHITE);
        M5.Lcd.setCursor(50, 80);
        M5.Lcd.print("A - restart");
        M5.Lcd.setCursor(50, 95);
        M5.Lcd.print("B - menu");
    }
    
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(0, 125);
    M5.Lcd.println("A=Jump  B=Exit");
}

void updateFlappy() {
    if(!flappyStarted || flappyGameOver) return;
    
    birdVelocity += GRAVITY;
    birdY += birdVelocity;
    
    if(birdY < 5 || birdY > 130) {
        flappyGameOver = true;
        playGameOverSound();
        if(flappyScore > flappyHighScore) {
            flappyHighScore = flappyScore;
            saveFlappyHighScore(flappyHighScore);
        }
    }
    
    for(int i = 0; i < 3; i++) {
        pipes[i].x -= pipeSpeed;
        
        if(!pipes[i].passed && pipes[i].x + 20 < BIRD_X - BIRD_SIZE) {
            pipes[i].passed = true;
            flappyScore++;
            playEatSound();
            if(flappyScore > flappyHighScore) {
                flappyHighScore = flappyScore;
                saveFlappyHighScore(flappyHighScore);
            }
        }
        
        if(pipes[i].x > BIRD_X - BIRD_SIZE - 20 && pipes[i].x < BIRD_X + BIRD_SIZE) {
            if(birdY - BIRD_SIZE < pipes[i].gapY - pipeGap/2 || 
               birdY + BIRD_SIZE > pipes[i].gapY + pipeGap/2) {
                flappyGameOver = true;
                playGameOverSound();
                if(flappyScore > flappyHighScore) {
                    flappyHighScore = flappyScore;
                    saveFlappyHighScore(flappyHighScore);
                }
            }
        }
        
        if(pipes[i].x < -30) {
            pipes[i].x = 260;
            pipes[i].gapY = random(35, 100);
            pipes[i].passed = false;
        }
    }
}

void flappyJump() {
    if(!flappyStarted && !flappyGameOver) {
        flappyStarted = true;
    }
    if(!flappyGameOver) {
        birdVelocity = JUMP_FORCE;
        playJumpSound();
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
                playGameOverSound();
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

bool isBShortPressed() {
    return M5.BtnB.wasPressed();
}

// ---------- SETUP ----------
void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Lcd.setRotation(3);
    
    Serial.begin(115200);
    
    EEPROM.begin(EEPROM_SIZE);
    
    currentTheme = loadTheme();
    loadWiFiCredentials();
    loadNewsURL();
    eggScore = loadEggScore();
    loadGMTOffset();
    snakeHighScore = loadSnakeHighScore();
    flappyHighScore = loadFlappyHighScore();
    
    initRTC();
    
    bootSplash();
    
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(10, 30);
    M5.Lcd.println("Podklyucheniye k WiFi...");
    
    if(savedSSID.length() > 0) {
        M5.Lcd.setCursor(10, 50);
        M5.Lcd.println("SSID: " + savedSSID);
        
        if(connectToSavedWiFi()) {
            M5.Lcd.setTextColor(TFT_GREEN);
            M5.Lcd.setCursor(10, 70);
            M5.Lcd.println("WiFi podklyuchen!");
            M5.Lcd.setCursor(10, 85);
            M5.Lcd.println("IP: " + WiFi.localIP().toString());
        } else {
            M5.Lcd.setTextColor(TFT_RED);
            M5.Lcd.setCursor(10, 70);
            M5.Lcd.println(wifiErrorMessage);
            wifiConnected = false;
        }
    } else {
        M5.Lcd.setCursor(10, 50);
        M5.Lcd.println("Net sokhranennykh setei");
        wifiConnected = false;
    }
    
    delay(1500);
    
    resetSnake();
    resetFlappy();
    eggCracked = false;
    locked = true;
    drawLockScreen();
    currentScreen = SCREEN_LOCK;
    lastActivity = millis();
}

// ---------- LOOP ----------
void loop() {
    M5.update();
    checkPowerOff();
    
    updateSound();
    playNextInQueue();
    
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
        else if(currentScreen == 2) drawBat();
        else if(currentScreen == 3) drawSnake();
        else if(currentScreen == 4) drawFlappy();
        else if(currentScreen == 5) drawEgg();
        else if(currentScreen == 6) {
            if(wifiState == 0) drawWiFiScanner();
            else if(wifiState == 1) drawWiFiSelect();
            else if(wifiState == 2) drawPasswordInput();
            else drawWiFiConnecting();
        }
        else if(currentScreen == 7) drawAccel();
        else if(currentScreen == 8) drawNews();
        else if(currentScreen == 9) drawPowerMenu();
        else if(currentScreen == 10) drawSettings();
        else if(currentScreen == 11) drawThemes();
        else if(currentScreen == 13) drawNewsURLInput();
    }
    
    // ЭКРАН БЛОКИРОВКИ
    if(locked) {
        if(M5.BtnA.wasPressed() || M5.BtnB.wasPressed()) {
            locked = false;
            currentScreen = 0;
            screenEnterTime = millis();
            drawAppRibbon();
            lastActivity = millis();
            playUnlockSound();
        }
        static int lastLockMin = -1;
        auto local = getLocalDateTime();
        if(local.time.minutes != lastLockMin) {
            drawLockScreen();
            lastLockMin = local.time.minutes;
        }
        return;
    }
    
    // POWER МЕНЮ
    if(currentScreen == 9) {
        if(bShort) {
            powerMenuPos = (powerMenuPos + 1) % POWER_MENU_COUNT;
            drawPowerMenu();
            playClickSound();
            lastActivity = millis();
        }
        
        if(M5.BtnA.wasPressed()) {
            if(powerMenuPos == 3) {
                currentScreen = 0;
                drawAppRibbon();
            } else {
                if(powerMenuPos == 0) rebootDevice();
                else if(powerMenuPos == 1) powerOffDevice();
                else if(powerMenuPos == 2) deepSleepDevice();
            }
            lastActivity = millis();
        }
        
        if(M5.BtnPWR.wasPressed()) {
            currentScreen = 0;
            drawAppRibbon();
            lastActivity = millis();
        }
        return;
    }
    
    // ЛЕНТА ПРИЛОЖЕНИЙ
    if(currentScreen == 0) {
        if(bShort) {
            currentApp = (currentApp + 1) % APP_COUNT;
            drawAppRibbon();
            lastActivity = millis();
            playClickSound();
        }
        if(M5.BtnA.wasPressed()) {
            if(currentApp == 0) { currentScreen = 1; drawInfo(); }
            else if(currentApp == 1) { currentScreen = 2; drawBat(); }
            else if(currentApp == 2) { currentScreen = 3; resetSnake(); drawSnake(); }
            else if(currentApp == 3) { currentScreen = 4; resetFlappy(); drawFlappy(); }
            else if(currentApp == 4) { currentScreen = 5; eggCracked = false; drawEgg(); }
            else if(currentApp == 5) { 
                currentScreen = 6; 
                wifiState = 0;
                wifiErrorMessage = "";
                scanWiFiNetworks();
                drawWiFiScanner();
            }
            else if(currentApp == 6) { currentScreen = 7; drawAccel(); }
            else if(currentApp == 7) { 
                currentScreen = 8; 
                newsContent = "";
                newsFirstLoad = true;
                newsScroll = 0;
                
                if(!wifiConnected && savedSSID.length() > 0) {
                    M5.Lcd.fillScreen(TFT_BLACK);
                    M5.Lcd.setCursor(0, 20);
                    M5.Lcd.println("Podklyucheniye k WiFi...");
                    if(connectToSavedWiFi()) {
                        M5.Lcd.println("Podklyucheno!");
                        delay(500);
                    }
                }
                
                fetchNews();
                drawNews();
            }
            else if(currentApp == 8) { 
                currentScreen = 9; 
                powerMenuPos = 0;
                drawPowerMenu(); 
            }
            else if(currentApp == 9) { currentScreen = 10; settingsPos = 0; drawSettings(); }
            else if(currentApp == 10) { currentScreen = 11; drawThemes(); }
            
            screenEnterTime = millis();
            lastActivity = millis();
        }
    }
    
    // ЗМЕЙКА
    if(currentScreen == 3) {
        if(!gameOver && millis() - lastMove > gameSpeed) {
            moveSnake();
            drawSnake();
            lastMove = millis();
        }
        
        if(M5.BtnA.wasPressed()) {
            if(gameOver) {
                resetSnake();
                drawSnake();
            } else {
                dir = (dir + 1) % 4;
                playClickSound();
            }
            lastActivity = millis();
        }
        
        if(bShort) {
            currentScreen = 0;
            drawAppRibbon();
            lastActivity = millis();
        }
    }
    
    // FLAPPY BIRD
    if(currentScreen == 4) {
        static unsigned long lastFlappyUpdate = 0;
        
        if(flappyStarted && !flappyGameOver) {
            if(millis() - lastFlappyUpdate > 30) {
                updateFlappy();
                drawFlappy();
                lastFlappyUpdate = millis();
            }
        }
        
        if(M5.BtnA.wasPressed()) {
            if(flappyGameOver) {
                resetFlappy();
                drawFlappy();
            } else {
                flappyJump();
                drawFlappy();
            }
            lastActivity = millis();
        }
        
        if(bShort) {
            currentScreen = 0;
            drawAppRibbon();
            lastActivity = millis();
        }
    }
    
    // EGG CLICKER
    if(currentScreen == 5) {
        if(millis() - lastEggAnim > 50) {
            drawEgg();
            lastEggAnim = millis();
        }
        
        if(bShort) { 
            saveEggScore(eggScore);
            currentScreen = 0; 
            drawAppRibbon(); 
            lastActivity = millis();
        }
        
        if(!eggCracked) {
            if(M5.BtnA.wasPressed() && millis() - screenEnterTime > 500) {
                eggScore++;
                saveEggScore(eggScore);
                eggCracked = true;
                playEggCrackSound();
                drawEgg();
                lastActivity = millis();
            }
        } else {
            if(M5.BtnA.wasPressed() && millis() - screenEnterTime > 500) {
                eggCracked = false;
                playClickSound();
                drawEgg();
                lastActivity = millis();
            }
        }
    }
    
    // WIFI МЕНЮ
    if(currentScreen == 6) {
        pwrForKeyboard = true;
        
        if(wifiState == 0) {
            if(M5.BtnPWR.wasPressed()) { currentScreen = 0; drawAppRibbon(); }
            if(M5.BtnA.wasPressed()) { wifiState = 1; selectedNetwork = 0; drawWiFiSelect(); playClickSound(); }
        }
        else if(wifiState == 1) {
            if(M5.BtnPWR.wasPressed()) { wifiState = 0; drawWiFiScanner(); }
            if(M5.BtnA.wasPressed() && millis() - screenEnterTime > 500) { 
                wifiState = 2; 
                password = ""; 
                keyIndex = 0; 
                drawPasswordInput();
                playClickSound();
            }
            if(bShort) { 
                selectedNetwork = (selectedNetwork + 1) % min(numNetworks, 8); 
                drawWiFiSelect();
                playClickSound();
            }
            if(M5.BtnA.isPressed()) {
                if(aPressStart == 0) aPressStart = millis();
                if(!aLongPressActive && millis() - aPressStart > 500) {
                    aLongPressActive = true; 
                    aRepeatTimer = millis();
                    selectedNetwork = (selectedNetwork - 1 + min(numNetworks, 8)) % min(numNetworks, 8);
                    drawWiFiSelect();
                }
                if(aLongPressActive && millis() - aRepeatTimer > 150) {
                    aRepeatTimer = millis();
                    selectedNetwork = (selectedNetwork - 1 + min(numNetworks, 8)) % min(numNetworks, 8);
                    drawWiFiSelect();
                }
            } else { 
                aPressStart = 0; 
                aLongPressActive = false; 
            }
        }
        else if(wifiState == 2) {
            if(M5.BtnPWR.wasPressed()) { 
                if(password.length() > 0) password = password.substring(0, password.length() - 1); 
                drawPasswordInput(); 
                playClickSound();
            }
            if(M5.BtnA.wasPressed()) { 
                keyIndex = (keyIndex + 1) % keyboardChars.length(); 
                drawPasswordInput(); 
                playClickSound();
            }
            if(M5.BtnA.isPressed()) {
                if(aPressStart == 0) aPressStart = millis();
                if(!aLongPressActive && millis() - aPressStart > 500) {
                    aLongPressActive = true; 
                    aRepeatTimer = millis();
                    keyIndex = (keyIndex - 1 + keyboardChars.length()) % keyboardChars.length();
                    drawPasswordInput();
                }
                if(aLongPressActive && millis() - aRepeatTimer > 150) {
                    aRepeatTimer = millis();
                    keyIndex = (keyIndex - 1 + keyboardChars.length()) % keyboardChars.length();
                    drawPasswordInput();
                }
            } else { 
                aPressStart = 0; 
                aLongPressActive = false; 
            }
            
            if(M5.BtnB.isPressed()) {
                if(bPressStart == 0) { 
                    bPressStart = millis(); 
                    bLongPressActive = false; 
                }
                else if(!bLongPressActive && millis() - bPressStart > 800) {
                    bLongPressActive = true;
                    wifiState = 3;
                    wifiErrorMessage = "";
                    
                    saveWiFiCredentials(networkSSIDs[selectedNetwork], password);
                    
                    drawWiFiConnecting();
                    wifiStatusTime = millis();
                    WiFi.begin(networkSSIDs[selectedNetwork].c_str(), password.c_str());
                    playUnlockSound();
                }
            } else {
                if(bPressStart > 0 && !bLongPressActive) { 
                    password += keyboardChars[keyIndex]; 
                    drawPasswordInput();
                    playClickSound();
                }
                bPressStart = 0; 
                bLongPressActive = false;
            }
        }
        else if(wifiState == 3) {
            if(M5.BtnPWR.wasPressed()) { 
                wifiState = 0; 
                wifiErrorMessage = "";
                drawWiFiScanner(); 
            }
            
            if(WiFi.status() == WL_CONNECTED) {
                if(!wifiConnected) {
                    wifiConnected = true;
                    wifiErrorMessage = "";
                    drawWiFiConnecting();
                    syncTime();
                    playUnlockSound();
                }
            } else {
                static unsigned long lastCheck = 0;
                if(millis() - lastCheck > 3000) {
                    lastCheck = millis();
                    int status = WiFi.status();
                    if(status == WL_CONNECT_FAILED) {
                        wifiErrorMessage = "Nevernyi parol'!";
                    } else if(status == WL_NO_SSID_AVAIL) {
                        wifiErrorMessage = "Set' ne naidena!";
                    } else if(status == WL_DISCONNECTED) {
                        wifiErrorMessage = "Ne udalos' podklyuchitsya";
                    }
                    drawWiFiConnecting();
                }
            }
            
            if(millis() - wifiStatusTime > 20000 && !wifiConnected) { 
                wifiState = 0; 
                drawWiFiScanner(); 
            }
            drawWiFiConnecting();
        }
        
        lastActivity = millis();
    }
    
    // NEWS URL INPUT
    if(currentScreen == 13) {
        pwrForKeyboard = true;
        if(M5.BtnPWR.wasPressed()) { 
            if(newsURL.length() > 0) newsURL = newsURL.substring(0, newsURL.length() - 1); 
            drawNewsURLInput(); 
            playClickSound();
        }
        if(M5.BtnA.wasPressed()) { 
            keyIndex = (keyIndex + 1) % keyboardChars.length(); 
            drawNewsURLInput(); 
            playClickSound();
        }
        if(M5.BtnA.isPressed()) {
            if(aPressStart == 0) aPressStart = millis();
            if(!aLongPressActive && millis() - aPressStart > 500) {
                aLongPressActive = true; 
                aRepeatTimer = millis();
                keyIndex = (keyIndex - 1 + keyboardChars.length()) % keyboardChars.length();
                drawNewsURLInput();
            }
            if(aLongPressActive && millis() - aRepeatTimer > 150) {
                aRepeatTimer = millis();
                keyIndex = (keyIndex - 1 + keyboardChars.length()) % keyboardChars.length();
                drawNewsURLInput();
            }
        } else { 
            aPressStart = 0; 
            aLongPressActive = false; 
        }
        
        if(M5.BtnB.isPressed()) {
            if(bPressStart == 0) { 
                bPressStart = millis(); 
                bLongPressActive = false; 
            }
            else if(!bLongPressActive && millis() - bPressStart > 800) {
                bLongPressActive = true; 
                saveNewsURL(newsURL); 
                currentScreen = 10; 
                drawSettings();
                playUnlockSound();
            }
        } else {
            if(bPressStart > 0 && !bLongPressActive) { 
                newsURL += keyboardChars[keyIndex]; 
                drawNewsURLInput(); 
                playClickSound();
            }
            bPressStart = 0; 
            bLongPressActive = false;
        }
        lastActivity = millis();
    }
    
    // НОВОСТИ
    if(currentScreen == 8) {
        if(bShort) { 
            currentScreen = 0; 
            drawAppRibbon(); 
            lastActivity = millis();
        }
        
        if(M5.BtnA.wasPressed() && millis() - screenEnterTime > 500) { 
            newsContent = "";
            newsFirstLoad = true;
            newsScroll = 0;
            fetchNews(); 
            drawNews(); 
            lastActivity = millis();
        }
        
        if(M5.BtnB.isPressed()) {
            static unsigned long lastScrollPress = 0;
            if(millis() - lastScrollPress > 200) { 
                newsScroll++; 
                drawNews(); 
                lastScrollPress = millis(); 
                lastActivity = millis();
            }
        }
    }
    
    // ТЕМЫ
    if(currentScreen == 11) {
        if(bShort) {
            currentTheme = (currentTheme + 1) % THEME_COUNT;
            saveTheme(currentTheme);
            drawThemes();
            playClickSound();
            lastActivity = millis();
        }
        if(M5.BtnA.isPressed()) {
            if(aPressStart == 0) aPressStart = millis();
            if(!aLongPressActive && millis() - aPressStart > 500) {
                aLongPressActive = true; 
                aRepeatTimer = millis();
                currentTheme = (currentTheme - 1 + THEME_COUNT) % THEME_COUNT;
                saveTheme(currentTheme);
                drawThemes();
            }
            if(aLongPressActive && millis() - aRepeatTimer > 150) {
                aRepeatTimer = millis();
                currentTheme = (currentTheme - 1 + THEME_COUNT) % THEME_COUNT;
                saveTheme(currentTheme);
                drawThemes();
            }
        } else { 
            aPressStart = 0; 
            aLongPressActive = false; 
        }
        
        if(M5.BtnA.wasPressed() && millis() - screenEnterTime > 500 && !aLongPressActive) {
            currentScreen = 0;
            drawAppRibbon();
            lastActivity = millis();
        }
    }
    
    // НАСТРОЙКИ
    if(currentScreen == 10) {
        if(M5.BtnPWR.wasPressed()) { 
            currentScreen = 0; 
            drawAppRibbon(); 
            lastActivity = millis();
            return; 
        }
        if(bShort) { 
            settingsPos = (settingsPos + 1) % SETTINGS_COUNT; 
            drawSettings();
            playClickSound();
            lastActivity = millis();
        }
        if(M5.BtnA.isPressed()) {
            if(aPressStart == 0) aPressStart = millis();
            if(!aLongPressActive && millis() - aPressStart > 500) {
                aLongPressActive = true; 
                aRepeatTimer = millis();
                settingsPos = (settingsPos - 1 + SETTINGS_COUNT) % SETTINGS_COUNT;
                drawSettings();
            }
            if(aLongPressActive && millis() - aRepeatTimer > 150) {
                aRepeatTimer = millis();
                settingsPos = (settingsPos - 1 + SETTINGS_COUNT) % SETTINGS_COUNT;
                drawSettings();
            }
        } else { 
            aPressStart = 0; 
            aLongPressActive = false; 
        }
        
        if(M5.BtnA.wasPressed() && millis() - screenEnterTime > 500 && !aLongPressActive) {
            if(settingsPos == 0) {
                brightness = (brightness + 10) % 110;
                if(brightness == 0) brightness = 10;
                M5.Lcd.setBrightness(brightness);
                drawSettings();
                playClickSound();
            } else if(settingsPos == 1) {
                gmtOffset = (gmtOffset + 1) % 14;
                saveGMTOffset(gmtOffset);
                drawSettings();
                playClickSound();
                
                if(currentScreen == 0) drawAppRibbon();
            } else if(settingsPos == 2) {
                currentScreen = 13;
                keyIndex = 0;
                drawNewsURLInput();
            } else if(settingsPos == 3) {
                currentScreen = 0;
                drawAppRibbon();
            }
            lastActivity = millis();
        }
    }
    
    // ИНФО / БАТАРЕЯ / ACCEL
    if(currentScreen == 1 || currentScreen == 2 || currentScreen == 7) {
        if(bShort) { 
            currentScreen = 0; 
            drawAppRibbon(); 
            lastActivity = millis();
        }
    }
    
    // АКСЕЛЕРОМЕТР
    if(currentScreen == 7) { 
        drawAccel(); 
        delay(50); 
    }
    
    // ОБНОВЛЕНИЕ РАБОЧЕГО СТОЛА
    if(currentScreen == 0) {
        static int lastBat = -1;
        static int lastMin = -1;
        int bat = M5.Power.getBatteryLevel();
        auto local = getLocalDateTime();
        if(bat != lastBat || local.time.minutes != lastMin) { 
            drawAppRibbon(); 
            lastBat = bat; 
            lastMin = local.time.minutes; 
        }
    }
    
    // АВТОБЛОКИРОВКА
    if(millis() - lastActivity > 30000 && !locked) { 
        locked = true; 
        currentScreen = SCREEN_LOCK; 
        drawLockScreen(); 
    }
    
    if(M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || M5.BtnPWR.wasPressed()) {
        lastActivity = millis();
    }
    
    if(currentScreen != 6 && currentScreen != 13) pwrForKeyboard = false;
    
    delay(15);
}
