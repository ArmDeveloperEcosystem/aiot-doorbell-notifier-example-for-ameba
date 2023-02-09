//
// SPDX-FileCopyrightText: Copyright 2023 Arm Limited and/or its affiliates <open-source-office@arm.com>
// SPDX-License-Identifier: MIT
//

/*
  This Arduino sketch uses the Realtek RTL8721DM based SparkFun AzureWave Thing Plus - AW-CU488 development board
  and a SparkFun Analog MEMS Microphone Breakout - SPH8878LR5H-1 to capture audio and detect a doorbell sound.
  When a doorbell sound is detected, an SMS message is sent to a cellphone using the Twilio Programmable SMS API.

  The sketch uses the Ameba_TensorFlowLite and ArduinoHttpClient libraries.

  Circuit:

  - SparkFun AzureWave Thing Plus - AW-CU488 and SparkFun Analog MEMS Microphone Breakout - SPH8878LR5H-1:
    - 3v3      -> VCC
    - GND      -> GND
    - 22 (PA4) -> AUD

*/

#include <AudioCodec.h>
#include <WiFi.h>

#include "tflite_model.h"
#include "mel_weight_matrix.h"

#include "Model.h"
#include "QuantizedMelPowerSpectrogram.h"
#include "TwilioClient.h"

#include "arduino_secrets.h"

Model mlModel(tflite_model, 32 * 1024);

QuantizedMelPowerSpectrogram melPowerSpectrogram(
  49  /* width */,
  40  /* # of mel bins */,
  480 /* frame length */,
  320 /* frame step */,
  256 /* FFT size */,
  80  /* top dB */,
  mel_weight_matrix
);

WiFiSSLClient wifiClient;
TwilioClient twilioClient(wifiClient, TWILIO_ACCOUNT_SID, TWILIO_AUTH_TOKEN);

short audioBuffer[512];
unsigned long lastMessageMillis = 0;

float smoothedDoorbellPrediction = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println("Ameba Doorbell Notifier");
  Serial.println();

  // turn the built-in LED on
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  if (!mlModel.begin()) {
    Serial.println("Failed to initiliaze the ML model!");
    while (1);
  }

  if (!melPowerSpectrogram.begin()) {
    Serial.println("Failed to initiliaze the Mel Power Spectrogram!");
    while (1);
  }

  // assign the input scale and zero points from the ML Model
  melPowerSpectrogram.setOutputScale(mlModel.inputScale());
  melPowerSpectrogram.setOutputZeroPoint(mlModel.inputZeroPoint());

  // optional, uncomment to enable scaling of input before applying the DSP to
  // create the Mel power spectrogram
  //  melPowerSpectrogram.setInputScale(1.0);

  // connect to the Wi-Fi SSID
  Serial.print("Attempting to connect to SSID: ");
  Serial.println(WIFI_SSID);

  while (WiFi.begin(WIFI_SSID, WIFI_PASS) != WL_CONNECTED) {
    // wait 10 seconds for retry
    delay(10000);
  }

  Serial.println();
  Serial.println("Connected to Wi-Fi");

  // Configure and start the codec
  Serial.println("Starting audio codec and listening for doorbell sounds ...");

  // mono 16-bit @ 16 kHz
  Codec.setChannelCount(1);
  Codec.setBitDepth(16);
  Codec.setSampleRate(16000);

  // select analog mic and set gain
  Codec.setInputMicType(ANALOGMIC);
  Codec.setADCGain(47, 47);

  // to use PDM mic, uncomment the next to lines
  //  Codec.setInputMicType(PDMMIC);
  //  Codec.setDMicBoost(2, 2);

  // start audio codec in input only mode
  Codec.begin(true, false);

  // NOTE: once the audio codec is started the boards built-in LED CANNOT be used anymore
}

void loop() {
  // wait for new audio data from the audio codec
  if (Codec.readAvaliable()) {
    // read the new audio data into the audio buffer
    Codec.readDataPage(audioBuffer, 512);

    // write the new audio data to the Mel power spectrogram
    melPowerSpectrogram.write(audioBuffer, 512);

    // read the (quantized 8-bit) Mel power spectrogram data into the ML input
    melPowerSpectrogram.read(mlModel.input(), mlModel.inputBytes());

    // get a prediction from the ML model
    float predictions[ mlModel.numOutputs() ];

    mlModel.predict(predictions);

    // apply exponential smoothing to the doorbell prediction, which is at the first index
    smoothedDoorbellPrediction = smoothedDoorbellPrediction * 0.8 + predictions[0] * 0.2;

    // check if the smoothed doorbell prediction is over threshold value
    if (smoothedDoorbellPrediction > 0.90) {
      // smoothed doorbell sound prediction is over the threshold value

      Serial.print("[");
      for (int i = 0; i < mlModel.numOutputs(); i++) {
        Serial.print(' '); Serial.print(predictions[i]);
      }
      Serial.println(" ]");

      // check when the last message was sent, to avoid sending too many messages
      if ((millis() - lastMessageMillis) > (30 * 1000) || millis() < (30 * 1000)) {
        // assign the time the last message was sent to the current time
        lastMessageMillis = millis();

        // send message using Twilio
        Serial.println("Sending message via Twilio ...");

        if (!twilioClient.sendMessage(TWILIO_TO, TWILIO_FROM, "\xf0\x9f\x9a\xaa\xf0\x9f\x94\x94")) {
          Serial.println("Failed to send message!");
        }

        // clear the Mel power spectrogram
        melPowerSpectrogram.clear();
      }
    }
  }
}
