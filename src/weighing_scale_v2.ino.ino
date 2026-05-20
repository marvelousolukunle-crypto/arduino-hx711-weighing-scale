/*
   WIRING:
     HX711 VCC  --> Arduino 5V
     HX711 GND  --> Arduino GND
     HX711 DT   --> Arduino D3
     HX711 SCK  --> Arduino D2

     Load Cell RED   --> HX711 E+
     Load Cell BLACK --> HX711 E-
     Load Cell WHITE --> HX711 A-
     Load Cell GREEN --> HX711 A+

   HOW TO CALIBRATE:
     Step 1: Set SCALE_FACTOR to 1.0 below and upload
     Step 2: Open Serial Monitor at 9600 baud
     Step 3: Keep scale empty, note the number (should be near 0)
     Step 4: Place a known weight (e.g. 500g)
     Step 5: Write down the number shown
     Step 6: SCALE_FACTOR = that number / known weight in grams
     Step 7: Update SCALE_FACTOR below and re-upload
   =====================================================
*/

#include <HX711.h>
#include <LiquidCrystal_I2C.h>

// scale settings
#define SCALE_FACTOR     104.031794871792  // ← YOUR calibration number goes here
#define ZERO_THRESHOLD   5.0    // ← weights below this (grams) show as 0
#define LCD_ADDRESS      0x27   // ← try 0x3F if screen stays blank

#define DOUT_PIN   3
#define SCK_PIN    2

// LCD dimensions
#define LCD_COLS   16
#define LCD_ROWS   2

// Averaging & stability settings
#define FAST_READS      3    // readings when weight is changing (responsive)
#define STABLE_READS    10   // readings when weight is stable (accurate)
#define STABLE_MARGIN   1.5  // grams — if reading changes less than this, treat as stable
#define HISTORY_SIZE    6    // number of past readings to track stability

HX711 scale;
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLS, LCD_ROWS);

// Internal state tracking
float  lastDisplayed   = -999;   // last weight shown on screen
float  readingHistory[HISTORY_SIZE];
int    historyIndex    = 0;
bool   historyFull     = false;
bool   scaleIsStable   = false;
float finalWeight = 0;
float fastReading = 0;

//timers
unsigned long lastSensorRead    = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastSerialLog     = 0;

// Timing intervals
#define SENSOR_INTERVAL   80
#define DISPLAY_INTERVAL  200
#define SERIAL_INTERVAL   500

// Scale states
enum ScaleState {
  STATE_IDLE,       // scale is empty
  STATE_MEASURING,  // weight is changing
  STATE_STABLE,     // weight has settled
  STATE_ERROR       // sensor lost
};
ScaleState currentState = STATE_IDLE;

void updateState(float weight, bool stable) {
  if (!scale.is_ready()) {
    currentState = STATE_ERROR;
    return;
  }
  if (weight < ZERO_THRESHOLD) {
    currentState = STATE_IDLE;
  }
  else if (!stable) {
    currentState = STATE_MEASURING;
  }
  else {
    currentState = STATE_STABLE;
  }
}

//for returning the current state for the serial monitor
const char* stateName() {
  switch (currentState) {
    case STATE_IDLE:  return "IDLE";
    case  STATE_MEASURING:  return "MEASURING";
    case STATE_STABLE:  return "STABLE";
    case STATE_ERROR:  return "ERROR";
    default:           return "UNKNOWN"; 
  }
}

//   HELPER: PRINT TEXT CENTERED ON ANY ROW
void printCentered(const char* text, int row) {
  int len = strlen(text);
  int startCol = (LCD_COLS - len) / 2;
  if (startCol < 0) startCol = 0;
  lcd.setCursor(startCol, row);
  lcd.print(text);
}

//   UPDATE THE LCD DISPLAY
void updateDisplay(float weight, bool stable) {
  char weightText[16];
  char statusText[16];

  // Build weight text
  if (weight == 0) {
    strcpy(weightText, "0.0 g");
  } else if (weight < 1000.0) {
    dtostrf(weight, 5, 1, weightText);
    strcat(weightText, " g");
  } else {
    float kg = weight / 1000.0;
    dtostrf(kg, 5, 3, weightText);
    strcat(weightText, " kg");
  }

  // Build status text
  if (weight == 0) {
    strcpy(statusText, "Ready");
  } else if (!stable) {
    strcpy(statusText, "Measuring...");
  } else {
    strcpy(statusText, "Stable");
  }

  // Print row 0: weight centered
  lcd.setCursor(0, 0);
  lcd.print("                ");   // clear row
  printCentered(weightText, 0);

  // Print row 1: status centered
  lcd.setCursor(0, 1);
  lcd.print("                ");   // clear row
  printCentered(statusText, 1);
}

//   CHECK IF SCALE IS STABLE
bool isStable() {
  if (!historyFull) return false;  // not enough history yet

  float minVal = readingHistory[0];
  float maxVal = readingHistory[0];

  for (int i = 1; i < HISTORY_SIZE; i++) {
    if (readingHistory[i] < minVal) minVal = readingHistory[i];
    if (readingHistory[i] > maxVal) maxVal = readingHistory[i];
  }

  // Stable if the spread of all readings is within STABLE_MARGIN
  return (maxVal - minVal) <= STABLE_MARGIN;
}

// Start LCD
void initDisplay() {
  lcd.begin();
  lcd.backlight();
}

// Splash screen
void  showSplashScreen() {
  printCentered("WEIGHING SCALE", 0);
  printCentered("MCE Group 2", 1);
  delay(1500);
  lcd.clear();
}

// Start HX711
void initScale()
{
  scale.begin(DOUT_PIN, SCK_PIN);

  // Wait until HX711 is ready
  printCentered("Please wait...", 0);
  int attempts = 0;
  while (!scale.is_ready()) {
    attempts++;
    delay(200);
    if (attempts > 5) {
      // HX711 not responding after 5 seconds
      Serial.println("SENSOR ERROR!");
      Serial.println("Pls Check wiring");
      lcd.clear();
      printCentered("SENSOR ERROR!", 0);
      for (byte safemode = 0; safemode < 3; safemode++) {
        scale.begin(DOUT_PIN, SCK_PIN);
        if (scale.is_ready())
          break;
        delay(500);
      }
      if (!scale.is_ready()) {
        printCentered("Check wiring!", 1);
        while (true);
      }
    }
  }
  // Set calibration
  scale.set_scale(SCALE_FACTOR);
  // Tare with display feedback
  lcd.clear();
  printCentered("Keep empty!", 0);
  printCentered("Zeroing...", 1);
  scale.tare(20);   // average 20 readings for solid zero
  // Initialise history array with zeros
  for (int i = 0; i < HISTORY_SIZE; i++) {
    readingHistory[i] = 0;
  }
  // Ready!
  lcd.clear();
  printCentered("Scale Ready!", 0);
  printCentered("Pls Place weight", 1);
  delay(1500);
  lcd.clear();
}

void setup() {
  Serial.begin(9600);

  // Start LCD
  initDisplay();

  // Splash screen
  showSplashScreen();

  // Start HX711
  initScale();
}

void loop() {
  //Task 1
  unsigned long now = millis();
  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = now;

    if (!scale.is_ready()) {
      currentState = STATE_ERROR;
      return;
    }

    //a quick fast reading
    fastReading = scale.get_units(FAST_READS);
    if (fastReading < 0) fastReading = 0;

    //Store in history
    readingHistory[historyIndex] = fastReading;
    historyIndex = (historyIndex + 1) % HISTORY_SIZE;
    if (historyIndex == 0) historyFull = true;
    scaleIsStable = isStable();

    if (scaleIsStable) {
      finalWeight = scale.get_units(STABLE_READS);  // slow but accurate
      if (finalWeight < 0) finalWeight = 0;
    } else {
      finalWeight = fastReading;  // fast and responsive while changing
    }

    if (finalWeight < ZERO_THRESHOLD) finalWeight = 0;
    updateState(finalWeight, scaleIsStable);
  }

  //Task 2
  if (now - lastDisplayUpdate >= DISPLAY_INTERVAL) {
    lastDisplayUpdate = now;
    if (abs(finalWeight - lastDisplayed) >= 0.5 || lastDisplayed == -999) {
      updateDisplay(finalWeight, scaleIsStable);
      lastDisplayed = finalWeight;
    }
  }
  if (now - lastSerialLog >= SERIAL_INTERVAL) {
    lastSerialLog = now;
    Serial.print("Weight: ");
    Serial.print(finalWeight, 1);
    Serial.print("g  |  Stable: ");
    Serial.println(stateName());
  }
}
