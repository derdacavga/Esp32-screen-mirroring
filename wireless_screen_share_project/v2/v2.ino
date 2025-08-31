#include <TJpg_Decoder.h>
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <TFT_eSPI.h>

const char* ssid = "Your SSID";
const char* password = "Your PAssword";
const int websocket_port = 81;

WebSocketsServer webSocket = WebSocketsServer(websocket_port);
TFT_eSPI tft = TFT_eSPI();

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
  if ( y >= tft.height() ) return false;

  tft.pushImage(x, y, w, h, bitmap);

  return true;
}

#define JPEG_BUFFER_SIZE 50 * 1024
uint8_t* jpeg_buffer;

uint32_t jpeg_buffer_pos = 0;
uint32_t expected_jpeg_size = 0;

void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Connection lose!\n", num);
      break;

    case WStype_TEXT:
      {
        String text = String((char*)payload);
        if (text.startsWith("JPEG_FRAME_SIZE:")) {
          expected_jpeg_size = text.substring(16).toInt();
          jpeg_buffer_pos = 0;
          if (expected_jpeg_size > JPEG_BUFFER_SIZE) {
            Serial.printf("Error: Came JPEG size (%d) buffer (%d) over!\n", expected_jpeg_size, JPEG_BUFFER_SIZE);
            expected_jpeg_size = 0;
          } else {
          }
        }
        break;
      }

    case WStype_BIN:
      if (expected_jpeg_size > 0 && jpeg_buffer_pos + length <= expected_jpeg_size) {
        memcpy(jpeg_buffer + jpeg_buffer_pos, payload, length);
        jpeg_buffer_pos += length;
      }

      if (expected_jpeg_size > 0 && jpeg_buffer_pos >= expected_jpeg_size) {
        TJpgDec.drawJpg(0, 0, jpeg_buffer, expected_jpeg_size);

        expected_jpeg_size = 0;
      }
      break;
  }
}

void setup() {
  Serial.begin(115200);

  jpeg_buffer = (uint8_t*)ps_malloc(JPEG_BUFFER_SIZE);
  if (jpeg_buffer == NULL) {
    Serial.println("Error:Not defined area for JPEG buffer!");
    while (1);
  }

  tft.init();
  tft.setRotation(1); // 1 or 3 For display Rotation
  tft.fillScreen(TFT_BLACK);

  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);

  tft.setCursor(10, 10);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.println("Waiting Wi-Fi...");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  tft.fillScreen(TFT_BLACK);
  tft.setCursor(10, 10);
  tft.println("Connection Succes!");
  tft.print("IP: ");
  tft.println(WiFi.localIP());
  tft.println("\nConnection");
  tft.println("waiting...");

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop() {
  webSocket.loop();
}