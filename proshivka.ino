#include <M5StickCPlus2.h>
#include <WiFi.h>

// ---------- НАСТРОЙКИ ОС ----------
#define APP_COUNT 8
String appNames[APP_COUNT] = {"Info", "Bat", "Snake", "WiFi", "Accel", "Pong", "Settings", "Themes"};
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

// Для Пинг-Понга
int paddleY = 50;
int ballX = 120, ballY = 60;
int ballDX = 3, ballDY = 2;
int pongScore = 0;
bool pongGameOver = false;
unsigned long lastPongMove = 0;

// Состояния
int currentScreen = 0;
int settingsPos = 0;
#define SETTINGS_COUNT 4
String settingsMenu[SETTINGS_COUNT] = {"Yarkost", "GMT", "O Sisteme", "Nazad"};
int brightness = 100;
int gmtOffset = 3;

// Для выключения
bool pwrHeld = false;
unsigned long pwrStart = 0;
bool shuttingDown = false;
float lastRemaining = -1;

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
    
    if(theme == 2) { // Полоски
        for(int x = 0; x < 240; x += 8) {
            M5.Lcd.fillRect(x, 0, 4, 135, TFT_NAVY);
        }
    }
    else if(theme == 3) { // Волны
        for(int x = 0; x < 240; x += 2) {
            int y = 67 + sin(x * 0.05) * 30;
            M5.Lcd.drawPixel(x, y, TFT_CYAN);
            M5.Lcd.drawPixel(x, y + 20, TFT_CYAN);
            M5.Lcd.drawPixel(x, y - 20, TFT_CYAN);
        }
    }
    else if(theme == 4) { // Кубы (сине-фиолетовые)
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
    int hour = (dt.time.hours + gmtOffset) % 24;
    
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
    M5.Lcd.print("A/B = Vhod  |  PWR = Vkl");
    
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
    int hour = (dt.time.hours + gmtOffset) % 24;
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
        else if(i == 6) col = TFT_DARKGRAY;
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

// ---------- ИНФО (НИКОГДА НЕ УБИРАТЬ) ----------
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
    M5.Lcd.println("A=Podkl  B=Nazad");
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
    M5.Lcd.println("A=Podkl  B=Nazad");
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
    } else {
        M5.Lcd.println("Podklyucheniye...");
    }
    
    M5.Lcd.setCursor(0, 125);
    M5.Lcd.println("B=Nazad");
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

// ---------- ПИНГ-ПОНГ ----------
void resetPong() {
    paddleY = 50;
    ballX = 120; ballY = 60;
    ballDX = 3; ballDY = 2;
    pongScore = 0;
    pongGameOver = false;
}

void drawPong() {
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.drawRect(0, 0, 240, 135, TFT_WHITE);
    M5.Lcd.fillRect(230, paddleY, 5, 30, TFT_WHITE);
    M5.Lcd.fillCircle(ballX, ballY, 3, TFT_GREEN);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setCursor(5, 5);
    M5.Lcd.printf("Score: %d", pongScore);
    if(pongGameOver) {
        M5.Lcd.setTextSize(2);
        M5.Lcd.setCursor(50, 50);
        M5.Lcd.print("GAME OVER");
    }
}

void movePong() {
    if(pongGameOver) return;
    
    ballX += ballDX;
    ballY += ballDY;
    
    if(ballY <= 10 || ballY >= 125) ballDY = -ballDY;
    if(ballX <= 10) ballDX = -ballDX;
    
    if(ballX >= 225 && ballY >= paddleY && ballY <= paddleY + 30) {
        ballDX = -ballDX;
        pongScore++;
        ballDX += (ballDX > 0 ? 1 : -1);
    }
    
    if(ballX > 240) pongGameOver = true;
}

// ---------- ТАЙМЕР ВЫКЛЮЧЕНИЯ ----------
void checkPowerOff() {
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
    bootSplash();
    resetGame();
    resetPong();
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
    
    // Принудительная перерисовка при необходимости
    if(needFullRedraw) {
        needFullRedraw = false;
        if(currentScreen == 0) drawAppRibbon();
        else if(currentScreen == 1) drawInfo();
        else if(currentScreen == 3) drawBat();
        else if(currentScreen == 4) drawGame();
        else if(currentScreen == 5) {
            if(wifiState == 0) drawWiFiScanner();
            else if(wifiState == 1) drawWiFiSelect();
            else drawWiFiConnecting();
        }
        else if(currentScreen == 6) drawAccel();
        else if(currentScreen == 8) drawPong();
        else if(currentScreen == 9) drawSettings();
        else if(currentScreen == 10) drawThemes();
        else if(currentScreen == 11) drawAbout();
    }
    
    // ЭКРАН БЛОКИРОВКИ
    if(locked) {
        if(M5.BtnA.wasPressed() || M5.BtnB.wasPressed()) {
            locked = false;
            currentScreen = 0;
            drawAppRibbon();
            lastActivity = millis();
        }
        static int lastLockMin = -1;
        auto dt = M5.Rtc.getDateTime();
        if(dt.time.minutes != lastLockMin) {
            drawLockScreen();
            lastLockMin = dt.time.minutes;
        }
        return;
    }
    
    // ЛЕНТА
    if(currentScreen == 0) {
        if(bShort) {
            currentApp = (currentApp + 1) % APP_COUNT;
            drawAppRibbon();
            lastActivity = millis();
        }
        if(M5.BtnA.wasPressed()) {
            if(currentApp == 0) { currentScreen = 1; drawInfo(); }
            else if(currentApp == 1) { currentScreen = 3; drawBat(); }
            else if(currentApp == 2) { currentScreen = 4; resetGame(); drawGame(); }
            else if(currentApp == 3) { 
                currentScreen = 5; 
                wifiState = 0;
                scanWiFiNetworks();
                drawWiFiScanner();
            }
            else if(currentApp == 4) { currentScreen = 6; drawAccel(); }
            else if(currentApp == 5) { currentScreen = 8; resetPong(); drawPong(); }
            else if(currentApp == 6) { currentScreen = 9; settingsPos = 0; drawSettings(); }
            else if(currentApp == 7) { currentScreen = 10; drawThemes(); }
            lastActivity = millis();
        }
    }
    
    // WiFi
    else if(currentScreen == 5) {
        if(wifiState == 0) {
            if(bShort) {
                currentScreen = 0;
                drawAppRibbon();
                lastActivity = millis();
            }
            if(M5.BtnA.wasPressed()) {
                wifiState = 1;
                selectedNetwork = 0;
                drawWiFiSelect();
                lastActivity = millis();
            }
        }
        else if(wifiState == 1) {
            if(bShort) {
                wifiState = 0;
                drawWiFiScanner();
                lastActivity = millis();
            }
            if(M5.BtnA.wasPressed()) {
                wifiState = 2;
                drawWiFiConnecting();
                wifiStatusTime = millis();
                WiFi.begin(networkSSIDs[selectedNetwork].c_str(), "");
                lastActivity = millis();
            }
            if(M5.BtnB.isPressed()) {
                static unsigned long lastScroll = 0;
                if(millis() - lastScroll > 200) {
                    selectedNetwork = (selectedNetwork + 1) % min(numNetworks, 8);
                    drawWiFiSelect();
                    lastScroll = millis();
                }
            }
        }
        else {
            if(bShort) {
                wifiState = 0;
                drawWiFiScanner();
                lastActivity = millis();
            }
            if(WiFi.status() == WL_CONNECTED && !wifiConnected) {
                wifiConnected = true;
                drawWiFiConnecting();
            }
            if(millis() - wifiStatusTime > 15000 && !wifiConnected) {
                wifiState = 0;
                drawWiFiScanner();
            }
        }
    }
    
    // ТЕМЫ
    else if(currentScreen == 10) {
        if(M5.BtnPWR.wasPressed()) {
            currentScreen = 0;
            drawAppRibbon();
            lastActivity = millis();
            return;
        }
        if(bShort) {
            currentTheme = (currentTheme + 1) % THEME_COUNT;
            drawThemes();
            lastActivity = millis();
        }
        if(M5.BtnA.wasPressed()) {
            currentScreen = 0;
            drawAppRibbon();
            lastActivity = millis();
        }
    }
    
    // НАСТРОЙКИ
    else if(currentScreen == 9) {
        if(M5.BtnPWR.wasPressed()) {
            currentScreen = 0;
            drawAppRibbon();
            lastActivity = millis();
            return;
        }
        if(bShort) {
            settingsPos = (settingsPos + 1) % SETTINGS_COUNT;
            drawSettings();
            lastActivity = millis();
        }
        if(M5.BtnA.wasPressed()) {
            if(settingsPos == 0) {
                brightness = (brightness + 10) % 110;
                M5.Lcd.setBrightness(brightness);
                drawSettings();
            } else if(settingsPos == 1) {
                gmtOffset = (gmtOffset + 1) % 14;
                drawSettings();
            } else if(settingsPos == 2) {
                currentScreen = 11;
                drawAbout();
            } else if(settingsPos == 3) {
                currentScreen = 0;
                drawAppRibbon();
            }
            lastActivity = millis();
        }
    }
    
    // О СИСТЕМЕ
    else if(currentScreen == 11) {
        if(bShort) {
            currentScreen = 9;
            drawSettings();
            lastActivity = millis();
        }
    }
    
    // ИНФО
    else if(currentScreen == 1) {
        if(bShort) {
            currentScreen = 0;
            drawAppRibbon();
            lastActivity = millis();
        }
    }
    
    // БАТАРЕЯ / ACCEL
    else if(currentScreen == 3 || currentScreen == 6) {
        if(bShort) {
            currentScreen = 0;
            drawAppRibbon();
            lastActivity = millis();
        }
    }
    
    // SNAKE
    else if(currentScreen == 4) {
        if(bShort) {
            currentScreen = 0;
            drawAppRibbon();
            lastActivity = millis();
        }
        if(!gameOver && millis() - lastMove > gameSpeed) {
            moveSnake();
            drawGame();
            lastMove = millis();
        }
        if(M5.BtnA.wasPressed()) {
            dir = (dir + 1) % 4;
            lastActivity = millis();
        }
    }
    
    // PONG
    else if(currentScreen == 8) {
        if(bShort) {
            currentScreen = 0;
            drawAppRibbon();
            lastActivity = millis();
        }
        if(!pongGameOver && millis() - lastPongMove > 30) {
            movePong();
            drawPong();
            lastPongMove = millis();
        }
        if(M5.BtnA.isPressed()) {
            paddleY += 5;
            if(paddleY > 105) paddleY = 105;
            drawPong();
            lastActivity = millis();
        }
        if(M5.BtnB.isPressed()) {
            paddleY -= 5;
            if(paddleY < 0) paddleY = 0;
            drawPong();
            lastActivity = millis();
        }
    }
    
    // АКСЕЛЕРОМЕТР
    if(currentScreen == 6) {
        drawAccel();
        delay(50);
    }
    
    // ОБНОВЛЕНИЕ НА РАБОЧЕМ СТОЛЕ
    if(currentScreen == 0) {
        static int lastBat = -1;
        static int lastMin = -1;
        int bat = M5.Power.getBatteryLevel();
        auto dt = M5.Rtc.getDateTime();
        
        if(bat != lastBat || dt.time.minutes != lastMin) {
            drawAppRibbon();
            lastBat = bat;
            lastMin = dt.time.minutes;
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
    
    delay(15);
}