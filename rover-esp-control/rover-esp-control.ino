// Bluetooth
#include "BluetoothSerial.h"

// Camera
#include "esp_camera.h"
#include "FS.h"
#include "SD_MMC.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "driver/rtc_io.h"
#include <EEPROM.h>

BluetoothSerial SerialBT;

#define EEPROM_SIZE 1

// Pin definition for CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

const String DEVICE_NAME = "UNL-Rover";
const String MAC = "08:F9:E0:ED:20:4A";

int pictureNumber = 0;
camera_fb_t *fb = NULL;

enum Error {
    INVALID = -1,
    EXPECTED_ARG = -2,
    FUNCTION_FAILURE = -3,
}; 

void setup() {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector
    Serial.begin(115200);

    // Set up bluetooth
    SerialBT.begin(DEVICE_NAME);
    Serial.printf("Bluetooth Device \"%s\" started; MAC Address: %s\n", DEVICE_NAME.c_str(), MAC.c_str());

    // Set up camera
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;

    // If there is PSRAM, the camera can take higher quality photos
    if(psramFound()){
        config.frame_size = FRAMESIZE_UXGA; // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
        config.jpeg_quality = 10;
        config.fb_count = 2;
    } else {
        config.frame_size = FRAMESIZE_SVGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
    }

    // Initalize camera and framebuffer
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x", err);

        ESP.restart();
        return;
    }

    // Initalize SD card
    if(!SD_MMC.begin()){
        Serial.println("SD Card Mount Failed");
        
        ESP.restart();
        return;
    }

    // Initalize EEPROM
    EEPROM.begin(EEPROM_SIZE);
    pictureNumber = EEPROM.read(0);
}

void loop() {
    if (!SerialBT.connected()) {
        delay(100);
        return;
    }

    String result = "";
    int resLen = 0;
    if (SerialBT.available()) {
        result = SerialBT.readStringUntil('\n');
        Serial.println(result);
        if (result.length() == 0) {
            SerialBT.write(Error::INVALID);
            return;
        }

        resLen = result.length() + 1;
    }

    if (resLen != 0) {
        // Get the first argument
        char *result_char = const_cast<char*> (result.c_str()); 
        char *arg = strtok(result_char, " "); 

        if (*arg == 'm') {
            Serial.println("Moving");
        } else if (*arg == 'p') {
            Serial.println("Taking picture");

            if (!savePicture()) {
                SerialBT.write(Error::FUNCTION_FAILURE);
                return;
            }
        } else if (*arg == 'n') {
            // Get the next argument
            arg = strtok(NULL, " ");
            if (arg == NULL) {
                SerialBT.write(Error::EXPECTED_ARG);
                return;
            }

            int newVal = atoi(arg);

            if (newVal == 0) {
                SerialBT.write(Error::INVALID);
                return;
            }

            Serial.print("Updating EEPROM picture value to ");
            Serial.println(arg);

            EEPROM.write(0, newVal);
            EEPROM.commit();
        } else {
            SerialBT.write(Error::INVALID);
            return;
        }

        // Return the length of the sent bytes, success
        SerialBT.write(result.length() + 1);
    }
}

bool savePicture() {
    for (int i = 0; i < 3; i++) {
        fb = esp_camera_fb_get();
        esp_camera_fb_return(fb);
        fb = NULL;
    }
    fb = esp_camera_fb_get();
    pictureNumber++;

    String path = "/pic" + String(pictureNumber) +".jpeg";
    fs::FS &fs = SD_MMC;

    File file = fs.open(path.c_str(), FILE_WRITE);

    if (!file) {
        // Failed to open file
        return false;
    }

    file.write(fb->buf, fb->len); // payload (image), payload length
    Serial.printf("Saved file to path: %s\n", path.c_str());
    file.close();

    EEPROM.write(0, pictureNumber);
    EEPROM.commit();

    esp_camera_fb_return(fb);

    return true;
}



