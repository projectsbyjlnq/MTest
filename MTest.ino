// GUMAGANANG CODE:


#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h> 

// === WiFi Config ===
const char* ssid = "Tash";               // ⚠️ Palitan ng WiFi SSID mo
const char* password = "27BernieTheRabbit2321@"; // ⚠️ Palitan ng WiFi password mo

WebSocketsClient webSocket;

// === Unit 1 Pin Mapping ===
#define U1_LED1 19
#define U1_BTN1 25 // Light 1
#define U1_LED2 18 
#define U1_BTN2 26 // Light 2
#define U1_LED3 5
#define U1_BTN3 27 // Light 3

#define U1_BTN4 14 // TV
#define U1_BTN5 12 // Aircon
#define U1_BTN6 13 // PC

#define U1_WATER 34 // Sensor

// === Unit 2 Pin Mapping ===
#define U2_LED1 15
#define U2_BTN1 32 // Light 1
#define U2_LED2 4
#define U2_BTN2 33 // Light 2
#define U2_LED3 2
#define U2_BTN3 23 // Light 3

#define U2_BTN4 22 // TV
#define U2_BTN5 21 // Aircon
#define U2_BTN6 36 // PC (GPIO36/VP - Input Only)

#define U2_WATER 35 // Sensor (GPIO39/VN - Input Only)

// === Wattage Table ===
const int W_LIGHT = 10;
const int W_TV = 100;
const int W_AIRCON = 750;
const int W_PC = 300;

// === States and Helper Arrays ===
// Index Mapping: [0:Light1, 1:Light2, 2:Light3, 3:TV, 4:Aircon, 5:PC]
bool U1_states[6] = {false};
bool U2_states[6] = {false};

// Gumamit ng -1 para sa mga appliances na walang dedicated LED indicator (TV, Aircon, PC)
const int U1_LED_PINS[] = {U1_LED1, U1_LED2, U1_LED3, -1, -1, -1}; 
const int U2_LED_PINS[] = {U2_LED1, U2_LED2, U2_LED3, -1, -1, -1};

const int U1_BTN_PINS[] = {U1_BTN1, U1_BTN2, U1_BTN3, U1_BTN4, U1_BTN5, U1_BTN6};
const int U2_BTN_PINS[] = {U2_BTN1, U2_BTN2, U2_BTN3, U2_BTN4, U2_BTN5, U2_BTN6};
const int NUM_APPLIANCES = 6;

// === Timer for Non-Blocking Data Sending and Button Checking ===
unsigned long lastPayloadSend = 0;
unsigned long lastButtonCheck = 0;
const long interval = 1000; // Send data every 1 second
const long buttonInterval = 50; // Check buttons every 50ms (Debounce Time)

// Debounce state tracking
int U1_lastButtonState[NUM_APPLIANCES] = {0};
int U2_lastButtonState[NUM_APPLIANCES] = {0}; 

// Function Declarations
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length);
void applyState(int unit, int index, bool state);
String buildStatusPayload(int unit_num, int liters, int power, const bool states[]);


void setup() {
  Serial.begin(115200);

  // --- Pin Setup ---
  // Unit 1 setup
  pinMode(U1_BTN1, INPUT_PULLUP);
  pinMode(U1_BTN2, INPUT_PULLUP);
  pinMode(U1_BTN3, INPUT_PULLUP);
  pinMode(U1_BTN4, INPUT_PULLUP);
  pinMode(U1_BTN5, INPUT_PULLUP);
  pinMode(U1_BTN6, INPUT_PULLUP);
  pinMode(U1_LED1, OUTPUT);
  pinMode(U1_LED2, OUTPUT);
  pinMode(U1_LED3, OUTPUT);

  // Unit 2 setup
  pinMode(U2_BTN1, INPUT_PULLUP);
  pinMode(U2_BTN2, INPUT_PULLUP);
  pinMode(U2_BTN3, INPUT_PULLUP);
  pinMode(U2_BTN4, INPUT_PULLUP);
  pinMode(U2_BTN5, INPUT_PULLUP);
  pinMode(U2_BTN6, INPUT); // GPIO36 has no internal pull-up
  
  pinMode(U2_LED1, OUTPUT);
  pinMode(U2_LED2, OUTPUT);
  pinMode(U2_LED3, OUTPUT);
  
  // Initialize all LEDs OFF
  for(int i = 0; i < NUM_APPLIANCES; i++) {
    applyState(1, i, false);
    applyState(2, i, false);
  }
  // --- End Pin Setup ---

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Connected to WiFi");
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());

  // Connect ESP32 as client to WebSocket server (PC IP + port)
  webSocket.begin("192.168.8.102", 8080, "/"); // ⚠️ Palitan kung iba IP ng PC
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
}

void loop() {
  webSocket.loop();  
  unsigned long currentMillis = millis();

  // 1. Non-blocking Timer for Data Sending (1 second interval)
  if (currentMillis - lastPayloadSend >= interval) {
    lastPayloadSend = currentMillis; 
    
    // --- 1a. Compute Readings and Power ---
    int liters1 = map(analogRead(U1_WATER), 0, 4095, 0, 100);
    int liters2 = map(analogRead(U2_WATER), 0, 4095, 0, 100);
    
    int power1 = 0;
    int power2 = 0;
    
    // Power Calculation for Unit 1 
    if (U1_states[0]) power1 += W_LIGHT; 
    if (U1_states[1]) power1 += W_LIGHT; 
    if (U1_states[2]) power1 += W_LIGHT; 
    
    if (U1_states[3]) power1 += W_TV;
    if (U1_states[4]) power1 += W_AIRCON;
    if (U1_states[5]) power1 += W_PC;

    // Power Calculation for Unit 2 
    if (U2_states[0]) power2 += W_LIGHT;
    if (U2_states[1]) power2 += W_LIGHT;
    if (U2_states[2]) power2 += W_LIGHT;
    
    if (U2_states[3]) power2 += W_TV;
    if (U2_states[4]) power2 += W_AIRCON;
    if (U2_states[5]) power2 += W_PC;

    // --- 1b. Send Status Payload ---
    String payload1 = buildStatusPayload(1, liters1, power1, U1_states);
    String payload2 = buildStatusPayload(2, liters2, power2, U2_states);

    webSocket.sendTXT(payload1);
    webSocket.sendTXT(payload2);

    Serial.println("--- Sending Data ---");
    Serial.printf("Unit 1 Pwr: %d W, Unit 2 Pwr: %d W\n", power1, power2);
  }

  // 2. Non-blocking Timer for Button Checking (Debounce)
  if (currentMillis - lastButtonCheck >= buttonInterval) {
    lastButtonCheck = currentMillis;
    
    for(int i = 0; i < NUM_APPLIANCES; i++) {
        // --- Unit 1 Button Check (DIP Switch Logic) ---
        int reading1 = digitalRead(U1_BTN_PINS[i]);
        if (reading1 != U1_lastButtonState[i]) {
            U1_lastButtonState[i] = reading1;
            
            // LOW = ON; HIGH = OFF
            bool newState = (reading1 == LOW); 
            
            applyState(1, i, newState); 
            
            String controlPayload = "{\"cmd\":\"update_state\",\"unit\":\"u1\",\"index\":" + String(i) + ",\"state\":" + (newState ? "true" : "false") + "}";
            webSocket.sendTXT(controlPayload);
            Serial.printf("➡️ Physical DIP Switch U1_%d changed to %s, state sent.\n", i + 1, newState ? "ON" : "OFF");
        }
        
        // --- Unit 2 Button Check (DIP Switch Logic) ---
        int reading2 = digitalRead(U2_BTN_PINS[i]);
        if (reading2 != U2_lastButtonState[i]) {
            U2_lastButtonState[i] = reading2;
            
            bool newState;
            
            if (i == 5) { // Index 5 is U2_BTN6 (PC) - Ito ang kailangan i-invert
                // ✅ FIX: Inverted Logic: Kung LOW ang reading, gawing OFF ang state (false).
                // Kung HIGH ang reading, gawing ON ang state (true).
                newState = (reading2 == HIGH);
            } else {
                // Normal Logic: LOW = ON; HIGH = OFF
                newState = (reading2 == LOW);
            }
            
            applyState(2, i, newState);

            String controlPayload = "{\"cmd\":\"update_state\",\"unit\":\"u2\",\"index\":" + String(i) + ",\"state\":" + (newState ? "true" : "false") + "}";
            webSocket.sendTXT(controlPayload);
            Serial.printf("➡️ Physical DIP Switch U2_%d changed to %s, state sent.\n", i + 1, newState ? "ON" : "OFF");
        }
    }
  }
}

// === Helper Functions ===

// Function to build JSON status payload
String buildStatusPayload(int unit_num, int liters, int power, const bool states[]) {
    StaticJsonDocument<256> doc;
    doc["type"] = "status";
    
    JsonObject data = doc.createNestedObject("data");
    data["unit"] = (unit_num == 1) ? "unit 1" : "unit 2";
    data["water"] = liters;
    data["power"] = power;
    
    JsonArray stateArray = data.createNestedArray("states");
    for(int i = 0; i < NUM_APPLIANCES; i++) {
        stateArray.add(states[i]);
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

// Function to control LED pin and update state array
void applyState(int unit, int index, bool state) {
    if (unit == 1) {
        U1_states[index] = state;
        if (U1_LED_PINS[index] != -1) {
             digitalWrite(U1_LED_PINS[index], state ? HIGH : LOW);
        }
    } else {
        U2_states[index] = state;
        if (U2_LED_PINS[index] != -1) {
            digitalWrite(U2_LED_PINS[index], state ? HIGH : LOW);
        }
    }
}

// WebSocket Event Handler (Walang Pagbabago Dito)
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.println("✅ Connected to WebSocket server");
      break;
    case WStype_DISCONNECTED:
      Serial.println("❌ Disconnected from WebSocket server");
      break;
    case WStype_TEXT:
      Serial.printf("📩 Message from server: %s\n", payload);
      
      {
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (error) {
          Serial.printf("⚠️ JSON Deserialize failed: %s\n", error.c_str());
          return;
        }
        
        if (doc["cmd"] == "control") {
            String unitStr = doc["unit"];
            int unit = (unitStr == "u1") ? 1 : 2;
            int index = doc["index"];
            bool state = doc["state"];
            
            if (index >= 0 && index < NUM_APPLIANCES) {
              Serial.printf("🔌 Command: Set Unit %d, Index %d to %s\n", unit, index, state ? "ON" : "OFF");
              applyState(unit, index, state);
            }
        }
      } 
      break;
    case WStype_ERROR:
      Serial.println("⚠️ WebSocket error occurred!");
      break;
    default:
      // ...
      break;
  }
}