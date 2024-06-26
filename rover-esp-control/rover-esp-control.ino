/* Bluetooth */
#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

/* CAMERA SETUP STUFF */
#include "esp_camera.h"
#include "FS.h"
#include "SD_MMC.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "driver/rtc_io.h"
#include "EEPROM.h"

#define EEPROM_SIZE 1

// Pin definitions for CAMERA_MODEL_AI_THINKER
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

/* RoboClaw */
#include "RoboClaw.h"

// Pin and address definitions for the RoboClaw
RoboClaw rc = RoboClaw(&Serial, 10000);
#define addresssRc 0x80 // 128

/* Bluetooth device information */
#define DEVICE_NAME "UNL-Rover"

/* Picture number */
#define PIC_NUM_ADDR 0

int pictureNumber = 0;
camera_fb_t *fb = NULL;

enum Message {
    RESPONSE = 0,
    INVALID = -1,
    EXPECTED_ARG = -2,
    FUNCTION_FAILURE = -3,
    INVALID_ARG = -4,
};

enum Direction {
    STOP = 1,
    FORWARD = 2,
    BACKWARD = 3,
};

void setup() {
    // Initalize RoboClaw and stop motor movement
    rc.begin(38400);
    rc.
    rc.ForwardBackwardM1(addresssRc, 64);
    rc.ForwardBackwardM2(addresssRc, 64);

    // Initalize bluetooth serial
    SerialBT.begin(DEVICE_NAME);

    // Initalize camera
    if (!initCamera(framesize_t::FRAMESIZE_UXGA)) {
        ESP.restart();
        return;
    }

    // Initalize SD card
    if(!SD_MMC.begin()){
        ESP.restart();
        return;
    }

    // Initalize EEPROM
    EEPROM.begin(EEPROM_SIZE);
    pictureNumber = EEPROM.read(PIC_NUM_ADDR);
}

void loop() {
    // Wait around if not connected to a bluetooth device
    if (!SerialBT.connected()) {
        delay(20);
        return;
    }

    // Check for new messages and restart the loop if there are none
    if (!SerialBT.available()) {
        delay(20);
        return;
    }

    // Get the string from the terminal
    String result = SerialBT.readStringUntil('\n');
    if (result.length() == 0) {
        SerialBT.write(Message::INVALID);
        return;
    }

    // Parse the arguments and perform some actions and return a status
    int status = parseArgs(result);
    SerialBT.write(status);
}

int parseArgs(String input) {
    // Get the first argument
    char *result_char = const_cast<char*> (input.c_str());
    char *arg = strtok(result_char, " ");

    // Parse the arguments
    switch (*arg) {
        // Move the rover
        case('m'): {
            arg = strtok(NULL, " ");
            if (arg == NULL) {
                return Message::EXPECTED_ARG;
            }

            int direction = atoi(arg);
            if (direction == 0) {
                return Message::INVALID_ARG;
            }

            if (!move(direction)) {
                return Message::FUNCTION_FAILURE;
            }
        }
        break;

        // Save picture
        case('p'): {
            if (!savePicture()) {
                return Message::FUNCTION_FAILURE;
            }
        }
        break;

        // Set the next image number
        case('n'): {
            arg = strtok(NULL, " ");
            if (arg == NULL) {
                sendString(String(pictureNumber));
                break;
            }

            int newNumber = atoi(arg);
            if (newNumber == 0) {
                return Message::INVALID_ARG;
            }

            pictureNumber = newNumber;
            EEPROM.write(PIC_NUM_ADDR, newNumber);
            EEPROM.commit();
        }
        break;

        // Get the battery voltage
        case('v'): {
            uint16_t voltage = rc.ReadMainBatteryVoltage(addresssRc);
            sendString(String(voltage));
        }
        break;

        // If nothing else matched, it must be wrong!
        default: {
            return Message::INVALID;
        }
    }

    return input.length() + 1;
}

void sendString(String message) {
    SerialBT.write(Message::RESPONSE);
    SerialBT.write(message.length());
    SerialBT.print(message);
}

/// Initalizes the camera
bool initCamera(framesize_t frameSize) {
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

    config.frame_size = frameSize; // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
    config.jpeg_quality = 10;
    config.fb_count = 2;

    // Initalize camera and framebuffer
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        return false;
    }

    // Get 5 frames from the camera to warm up
    for (int i = 0; i < 5; i++) {
        fb = esp_camera_fb_get();
        esp_camera_fb_return(fb);
        fb = NULL;
    }

    return true;
}

/// Saves a picture to the next position
bool savePicture() {
    fb = esp_camera_fb_get();

    String path = "/pic" + leadingZeroes(pictureNumber) +".jpeg";
    fs::FS &fs = SD_MMC;

    File file = fs.open(path.c_str(), FILE_WRITE);

    if (!file) {
        // Failed to open file
        return false;
    }

    file.write(fb->buf, fb->len); // payload (image), payload length
    file.flush();
    file.close();

    pictureNumber++;
    EEPROM.write(PIC_NUM_ADDR, pictureNumber);
    EEPROM.commit();

    esp_camera_fb_return(fb);

    return true;
}

/// Adds leading zeroes to an integer, and returns it as a string
String leadingZeroes(int input) {
    String number = String(input);
    String newNumber = "";

    if (input < 10) {
        newNumber = "000";
    } else if (input < 100) {
        newNumber = "00";
    } else if (input < 1000) {
        newNumber = "0";
    }

    return newNumber + number;
}

/// Make the RoboClaw controller move forwards/backwards
bool move(int dir) {
    // TODO: Figure out what the actual forward and backward values are
    switch(dir) {
        case(Direction::FORWARD): {
            rc.ForwardBackwardM1(addresssRc, 127);
            rc.ForwardBackwardM2(addresssRc, 127);
        }
        break;
        case(Direction::BACKWARD): {
            rc.ForwardBackwardM1(addresssRc, 0);
            rc.ForwardBackwardM2(addresssRc, 0);
        }
        break;
        case(Direction::STOP): {
            rc.ForwardBackwardM1(addresssRc, 64);
            rc.ForwardBackwardM2(addresssRc, 64);
        }
        break;
        default: {
            return false;
        }
    }
    return true;
}




