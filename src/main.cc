// plug in the ioo to gnd
// hit button
// run build
// upload
// to have the code actually run:
// remove the gnd wire, hit button

/*********
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp32-cam-take-photo-save-microsd-card

  IMPORTANT!!!
   - Select Board "AI Thinker ESP32-CAM"
   - GPIO 0 must be connected to GND to upload a sketch
   - After connecting GPIO 0 to GND, press the ESP32-CAM on-board RESET button to put your board in flashing mode

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*********/
#include <PS4Controller.h>
#include <Arduino.h>

void setup() {
  Serial.begin(9600);
  pinMode(4, OUTPUT);
  // start with led on to demo code is running
  digitalWrite(4, HIGH);
  delay(2000);
  digitalWrite(4, LOW);
  Serial.println("3 new code with new mac");

  PS4.begin("78:3e:2b:05:06:07");
  Serial.println("Controller ready.");
}

void loop() {
 if(PS4.isConnected()) {
    Serial.println("is connected!");
    digitalWrite(4, HIGH);
 } else {
    Serial.println("not connected...");
    digitalWrite(4, LOW);

 }
  delay(1000);
}