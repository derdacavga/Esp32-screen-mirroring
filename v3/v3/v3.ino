#include <TJpg_Decoder.h>
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <TFT_eSPI.h>
#include <SPI.h>

const char* ssid = "Your SSID";
const char* password = "Your SSID Password";
const int websocket_port = 81;

WebSocketsServer webSocket = WebSocketsServer(websocket_port);
TFT_eSPI tft = TFT_eSPI();

SemaphoreHandle_t spiMutex;

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= tft.height()) return false;
  tft.pushImage(x, y, w, h, bitmap);
  return true;
}

#define JPEG_BUFFER_SIZE 60 * 1024
uint8_t* jpeg_buffer1 = NULL;
uint8_t* jpeg_buffer2 = NULL;

uint32_t jpeg_buffer_pos = 0;
uint32_t expected_jpeg_size = 0;

void perform_calibration() {
  uint16_t calData[5];

  tft.fillScreen(TFT_GREEN);
  tft.setCursor(20, 0);
  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.setTextColor(TFT_BLACK, TFT_GREEN);
  tft.println("Calibration");
  tft.setTextFont(1);
  tft.println("\ntouch that\nblue corner");
  delay(1000);

  tft.calibrateTouch(calData, TFT_MAGENTA, TFT_GREEN, 15); 
  tft.setTouch(calData);

  Serial.println("Your Screen Calibration");
  Serial.printf("uint16_t calData[5] = { %d, %d, %d, %d, %d };\n", calData[0], calData[1], calData[2], calData[3], calData[4]);
  Serial.println();
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  static uint8_t* current_buffer = jpeg_buffer1;
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Connection is lost!\n", num);
      expected_jpeg_size = 0;
      jpeg_buffer_pos = 0;
      break;

    case WStype_TEXT: {
      String text = String((char*)payload);
      if (text.startsWith("JPEG_FRAME_SIZE:")) {
        expected_jpeg_size = text.substring(16).toInt();
        jpeg_buffer_pos = 0;
        if (expected_jpeg_size > JPEG_BUFFER_SIZE) {
          Serial.println("Error: JPEG size too large, resetting!");
          expected_jpeg_size = 0;
          jpeg_buffer_pos = 0;
          return;
        }
      }
      break;
    }

    case WStype_BIN:
      if (expected_jpeg_size == 0) {
        Serial.println("Error: No JPEG size received, ignoring data!");
        return;
      }
      if (jpeg_buffer_pos + length <= expected_jpeg_size && jpeg_buffer_pos + length <= JPEG_BUFFER_SIZE) {
        memcpy(current_buffer + jpeg_buffer_pos, payload, length);
        jpeg_buffer_pos += length;

        if (jpeg_buffer_pos >= expected_jpeg_size) {
          uint32_t start_time = micros();
          xSemaphoreTake(spiMutex, portMAX_DELAY);
          TJpgDec.drawJpg(0, 0, current_buffer, expected_jpeg_size);
          xSemaphoreGive(spiMutex);  
          webSocket.broadcastTXT("FRAME_DONE");
          expected_jpeg_size = 0;
          jpeg_buffer_pos = 0;
          current_buffer = (current_buffer == jpeg_buffer1) ? jpeg_buffer2 : jpeg_buffer1;
        }
      } else {
        Serial.println("Error: Buffer overflow or invalid data, resetting!");
        expected_jpeg_size = 0;
        jpeg_buffer_pos = 0;
      }
      break;

    case WStype_CONNECTED:
      Serial.printf("[%u] Connection established!\n", num);
      break;
  }
}

void websocketTask(void *pvParameters) {
  while (1) {
    webSocket.loop();
    vTaskDelay(1);
  }
}

void setup() {
  Serial.begin(115200);

  spiMutex = xSemaphoreCreateMutex();
  if (spiMutex == NULL) {
    Serial.println("Error: Failed to create SPI mutex!");
    while (1);
  }

  jpeg_buffer1 = (uint8_t*)ps_malloc(JPEG_BUFFER_SIZE);
  jpeg_buffer2 = (uint8_t*)ps_malloc(JPEG_BUFFER_SIZE);
  if (jpeg_buffer1 == NULL || jpeg_buffer2 == NULL) {
    Serial.println("Error: Failed to allocate JPEG buffers in PSRAM!");
    while (1);
  }

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  perform_calibration();
  //  tft.setTouch(300, 3000, 350, 3500, 6);
  
  tft.fillScreen(TFT_BLACK);
  
  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);

  tft.setTextColor(TFT_WHITE);
  tft.setCursor(10, 10);
  tft.println("Waiting WiFi...");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnect to Wifi!");

  tft.fillScreen(TFT_BLACK);
  tft.println("Connection Succes!");
  tft.print("IP: ");
  tft.println(WiFi.localIP());

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  xTaskCreatePinnedToCore(websocketTask, "WebSocketTask", 16384, NULL, 2, NULL, 0);
}

void loop() {
  uint16_t tx, ty;
  xSemaphoreTake(spiMutex, portMAX_DELAY);
  if (tft.getTouch(&tx, &ty, 200)) {
    xSemaphoreGive(spiMutex);  
    String touchData = "TOUCH:" + String(tx) + "," + String(ty);
    webSocket.broadcastTXT(touchData);
  } else {
    xSemaphoreGive(spiMutex); 
  }
}
