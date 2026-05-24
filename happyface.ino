#include "esp_camera.h"
#include <WiFi.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "esp_http_server.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// --- CONFIGURATION ---
const char* ssid = "";
const char* password = "";

// --- CAMERA PINS ---
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

// --- DISPLAY (SH1106 on Pins 15/14) ---
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 14, 15);

volatile int gestureX = 0; 
int lastGestureX = -100;

// SMILE LOGIC
void drawSmile(int x, int y, int w, int h) {
  for (int i = -w; i <= w; i++) {
    int j = (i * i * h) / (w * w);
    u8g2.drawPixel(x + i, y - j);
    u8g2.drawPixel(x + i, y - j + 1);
  }
}

// FACE LOGIC
void drawFace(int x_offset) {
  u8g2.clearBuffer();
  unsigned long now = millis();
  
  // BLINK LOGIC: Blink for 150ms every 2000ms (2 seconds)
  bool blink = (now % 2000 < 150); 

  int leftEyeX = 35 + x_offset;   
  int rightEyeX = 73 + x_offset;  

  if (blink) {
    u8g2.drawHLine(leftEyeX, 36, 20); 
    u8g2.drawHLine(rightEyeX, 36, 20);
  } else {
    // BIG ROUNDED EYES
    u8g2.drawRBox(leftEyeX, 22, 20, 28, 8); 
    u8g2.drawRBox(rightEyeX, 22, 20, 28, 8);
  }

  drawSmile(64, 58, 14, 5); 
  u8g2.sendBuffer();
}

// COMMAND HANDLER
static esp_err_t move_handler(httpd_req_t *req) {
    char buf[32];
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        char param[10];
        if (httpd_query_key_value(buf, "x", param, sizeof(param)) == ESP_OK) {
            int val = atoi(param);
            gestureX = map(val, 0, 100, -18, 18);
            Serial.printf("Command: X=%d -> Pupil=%d\n", val, gestureX);
        }
    }
    httpd_resp_send(req, "OK", 2);
    return ESP_OK;
}

// STREAM HANDLER
static esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=frame");
    while(true){
        fb = esp_camera_fb_get();
        if (!fb) { res = ESP_FAIL; break; }
        res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
        esp_camera_fb_return(fb);
        if(res != ESP_OK) break;
        vTaskDelay(5 / portTICK_PERIOD_MS); // Yield to process commands
    }
    return res;
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);
  u8g2.begin();

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0; config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM; config.pin_d1 = Y3_GPIO_NUM; config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM; config.pin_d4 = Y6_GPIO_NUM; config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM; config.pin_d7 = Y9_GPIO_NUM; config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM; config.pin_vsync = VSYNC_GPIO_NUM; config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM; config.pin_sccb_scl = SIOC_GPIO_NUM; config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM; config.xclk_freq_hz = 10000000; config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA; config.jpeg_quality = 16; config.fb_count = 1;
  esp_camera_init(&config);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }

  httpd_handle_t server = NULL;
  httpd_config_t config_s = HTTPD_DEFAULT_CONFIG();
  if (httpd_start(&server, &config_s) == ESP_OK) {
    httpd_uri_t move_uri = { .uri="/move", .method=HTTP_GET, .handler=move_handler };
    httpd_uri_t stream_uri = { .uri="/stream", .method=HTTP_GET, .handler=stream_handler };
    httpd_register_uri_handler(server, &move_uri);
    httpd_register_uri_handler(server, &stream_uri);
  }

  Serial.println("\n\n====================================");
  Serial.print("ESP32-CAM IP: "); Serial.println(WiFi.localIP());
  Serial.println("====================================\n");
}

void loop() {
  drawFace(gestureX);
  delay(10);
}