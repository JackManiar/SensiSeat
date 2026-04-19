#include <ArduinoBLE.h>
#include "esp_system.h"

#define LED_BUILTIN 2

typedef struct posture_packet_t {
  float weightFrontLeft;
  float weightFrontRight;
  float weightBackLeft;
  float weightBackRight; 
} posture_packet_t;

const int DOUT_FRONT_LEFT  = 25;
const int DOUT_FRONT_RIGHT = 32;
const int DOUT_BACK_LEFT   = 22;
const int DOUT_BACK_RIGHT  = 5;
const int HX_SCK_PIN       = 14;

long offsetFrontLeft  = 0;
long offsetFrontRight = 0;
long offsetBackLeft   = 0;
long offsetBackRight  = 0;

// Replace these after calibration
float calibrationFrontLeft  = 1000.0;
float calibrationFrontRight = 1000.0;
float calibrationBackLeft   = 1000.0;
float calibrationBackRight  = 1000.0;

const float BOARD_WIDTH_MM = 400.0;
const float BOARD_DEPTH_MM = 400.0;

const float NOISE_THRESHOLD_KG = 0.10;
const int TARE_SAMPLES = 15;

// Groups the info into a BLE service
BLEService postureService("FEED");
// The BLE characteristic holding the struct
BLECharacteristic postureInfo("F00D", BLERead | BLENotify, sizeof(posture_packet_t), true);
// reference to the connected BLE central
BLEDevice central;

void BLE_setup()
{
  BLE.setLocalName("SensiSeat");
  BLE.setAdvertisedService(postureService); 
  postureService.addCharacteristic(postureInfo);
  BLE.addService(postureService); 

  BLE.advertise();
}

// blocking call to connect to a BLE central. Uses global var "central"
void BLE_connect()
{
  do
  {
    central = BLE.central();       
  } while (!central);

  Serial.print("Connected to central: ");
  
  Serial.println(central.address());
}

// Updates the BLE posture info characteristic with a new struct
void BLE_updatePosture(posture_packet_t info)
{
  // Serial.print("Updating posture on BLE");
  postureInfo.writeValue(&info, sizeof(info));
}

bool allSensorsReady() {
  return digitalRead(DOUT_FRONT_LEFT)  == LOW &&
         digitalRead(DOUT_FRONT_RIGHT) == LOW &&
         digitalRead(DOUT_BACK_LEFT)   == LOW &&
         digitalRead(DOUT_BACK_RIGHT)  == LOW;
}

void readAllRaw(long &rawFrontLeft, long &rawFrontRight, long &rawBackLeft, long &rawBackRight) {
  rawFrontLeft = 0;
  rawFrontRight = 0;
  rawBackLeft = 0;
  rawBackRight = 0;

  while (!allSensorsReady()) {
    delay(1);
  }

  for (int i = 0; i < 24; i++) {
    digitalWrite(HX_SCK_PIN, HIGH);
    delayMicroseconds(2);

    rawFrontLeft  = (rawFrontLeft << 1)  | digitalRead(DOUT_FRONT_LEFT);
    rawFrontRight = (rawFrontRight << 1) | digitalRead(DOUT_FRONT_RIGHT);
    rawBackLeft   = (rawBackLeft << 1)   | digitalRead(DOUT_BACK_LEFT);
    rawBackRight  = (rawBackRight << 1)  | digitalRead(DOUT_BACK_RIGHT);

    digitalWrite(HX_SCK_PIN, LOW);
    delayMicroseconds(2);
  }

  // One extra pulse selects Channel A, Gain 128 for next conversion
  digitalWrite(HX_SCK_PIN, HIGH);
  delayMicroseconds(2);
  digitalWrite(HX_SCK_PIN, LOW);
  delayMicroseconds(2);

  // Sign-extend 24-bit signed values to 32-bit signed long
  if (rawFrontLeft  & 0x800000) rawFrontLeft  |= 0xFF000000;
  if (rawFrontRight & 0x800000) rawFrontRight |= 0xFF000000;
  if (rawBackLeft   & 0x800000) rawBackLeft   |= 0xFF000000;
  if (rawBackRight  & 0x800000) rawBackRight  |= 0xFF000000;
}

long averageRawForCorner(int cornerIndex, int samples) {
  long rawFrontLeft, rawFrontRight, rawBackLeft, rawBackRight;
  long sum = 0;

  for (int i = 0; i < samples; i++) {
    readAllRaw(rawFrontLeft, rawFrontRight, rawBackLeft, rawBackRight);

    if (cornerIndex == 0) sum += rawFrontLeft;
    if (cornerIndex == 1) sum += rawFrontRight;
    if (cornerIndex == 2) sum += rawBackLeft;
    if (cornerIndex == 3) sum += rawBackRight;
  }

  return sum / samples;
}

void tareAllSensors(int samples) {
  offsetFrontLeft  = averageRawForCorner(0, samples);
  offsetFrontRight = averageRawForCorner(1, samples);
  offsetBackLeft   = averageRawForCorner(2, samples);
  offsetBackRight  = averageRawForCorner(3, samples);
}

float rawToKg(long rawValue, long offsetValue, float calibrationValue) {
  float weightKg = (rawValue - offsetValue) / calibrationValue;

  if (abs(weightKg) < NOISE_THRESHOLD_KG) {
    weightKg = 0.0;
  }

  return weightKg;
}

void setup() {
  Serial.begin(115200);

  pinMode(DOUT_FRONT_LEFT, INPUT);
  pinMode(DOUT_FRONT_RIGHT, INPUT);
  pinMode(DOUT_BACK_LEFT, INPUT);
  pinMode(DOUT_BACK_RIGHT, INPUT);
  pinMode(HX_SCK_PIN, OUTPUT);

  digitalWrite(HX_SCK_PIN, LOW);

  pinMode(LED_BUILTIN, OUTPUT); 

  // begin initialization
  if (!BLE.begin()) {
    Serial.println("starting Bluetooth® Low Energy module failed!");

    while (1);
  }

  BLE_setup();
  Serial.println("Bluetooth® device active, waiting for connections...");
  BLE_connect();
  digitalWrite(LED_BUILTIN, HIGH);

  delay(1000);
  Serial.println("Keep the seat empty. Taring will start in 3 seconds...");
  delay(3000);

  tareAllSensors(TARE_SAMPLES);

  Serial.println("Tare complete.");
  Serial.println("Starting readings...");


}

void loop() {
  long rawFrontLeft, rawFrontRight, rawBackLeft, rawBackRight;
  readAllRaw(rawFrontLeft, rawFrontRight, rawBackLeft, rawBackRight);

  float weightFrontLeft  = -rawToKg(rawFrontLeft,  offsetFrontLeft,  calibrationFrontLeft);
  float weightFrontRight = -rawToKg(rawFrontRight, offsetFrontRight, calibrationFrontRight);
  float weightBackLeft   = -rawToKg(rawBackLeft,   offsetBackLeft,   calibrationBackLeft);
  float weightBackRight  = -rawToKg(rawBackRight,  offsetBackRight,  calibrationBackRight);

  float totalWeight = weightFrontLeft + weightFrontRight + weightBackLeft + weightBackRight;

  float xNormalized = 0.0;
  float yNormalized = 0.0;
  float xMillimeters = 0.0;
  float yMillimeters = 0.0;

  if (totalWeight > 0.5) {
    xNormalized = ((weightFrontRight + weightBackRight) - (weightFrontLeft + weightBackLeft)) / totalWeight;
    yNormalized = ((weightFrontLeft + weightFrontRight) - (weightBackLeft + weightBackRight)) / totalWeight;

    xMillimeters = xNormalized * (BOARD_WIDTH_MM / 2.0);
    yMillimeters = yNormalized * (BOARD_DEPTH_MM / 2.0);
  }

  posture_packet_t packet;
  packet.weightBackLeft = weightBackLeft;
  packet.weightBackRight = weightBackRight;
  packet.weightFrontLeft = weightFrontLeft;
  packet.weightFrontRight = weightFrontRight;
  BLE_updatePosture(packet);

  if (!central.connected())
  {
    Serial.println("Central Disconnected. Restarting.");
    esp_restart();
  }

  // Serial.println("--------------------------------------------------");
  // Serial.print("Raw Front Left: ");
  // Serial.print(rawFrontLeft);
  // Serial.print(" | Raw Front Right: ");
  // Serial.print(rawFrontRight);
  // Serial.print(" | Raw Back Left: ");
  // Serial.print(rawBackLeft);
  // Serial.print(" | Raw Back Right: ");
  // Serial.println(rawBackRight);

  // Serial.print("Front Left: ");
  // Serial.print(weightFrontLeft, 2);
  // Serial.print(" kg | Front Right: ");
  // Serial.print(weightFrontRight, 2);
  // Serial.print(" kg | Back Left: ");
  // Serial.print(weightBackLeft, 2);
  // Serial.print(" kg | Back Right: ");
  // Serial.print(weightBackRight, 2);
  // Serial.println(" kg");

  // Serial.print("Total Weight: ");
  // Serial.print(totalWeight, 2);
  // Serial.println(" kg");

  // Serial.print("Center X: ");
  // Serial.print(xMillimeters, 1);
  // Serial.print(" mm | Center Y: ");
  // Serial.print(yMillimeters, 1);
  // Serial.println(" mm");

  // if (totalWeight > 0.5) {
  //   Serial.print("Lean Direction: ");

  //   if (xNormalized < -0.2) {
  //     Serial.print("Left ");
  //   } else if (xNormalized > 0.2) {
  //     Serial.print("Right ");
  //   } else {
  //     Serial.print("Center ");
  //   }

  //   if (yNormalized > 0.2) {
  //     Serial.println("Front");
  //   } else if (yNormalized < -0.2) {
  //     Serial.println("Back");
  //   } else {
  //     Serial.println("Middle");
  //   }
  // } else {
  //   Serial.println("No significant weight detected.");
  // }

  // Serial.println("--------------------------------------------------");
  // Serial.print("Raw Front Left: "); Serial.print(rawFrontLeft);
  // Serial.print(" | Raw Front Right: "); Serial.print(rawFrontRight);
  // Serial.print(" | Raw Back Left: "); Serial.print(rawBackLeft);
  // Serial.print(" | Raw Back Right: "); Serial.println(rawBackRight);

  // Serial.print("Offset Front Left: "); Serial.print(offsetFrontLeft);
  // Serial.print(" | Offset Front Right: "); Serial.print(offsetFrontRight);
  // Serial.print(" | Offset Back Left: "); Serial.print(offsetBackLeft);
  // Serial.print(" | Offset Back Right: "); Serial.println(offsetBackRight);

  // Serial.print("Cal Front Left: "); Serial.print(calibrationFrontLeft, 3);
  // Serial.print(" | Cal Front Right: "); Serial.print(calibrationFrontRight, 3);
  // Serial.print(" | Cal Back Left: "); Serial.print(calibrationBackLeft, 3);
  // Serial.print(" | Cal Back Right: "); Serial.println(calibrationBackRight, 3);

  // Serial.print("Diff Back Left: ");
  // Serial.println(rawBackLeft - offsetBackLeft);

  // Serial.print("Front Left: "); Serial.print(weightFrontLeft, 2);
  // Serial.print(" kg | Front Right: "); Serial.print(weightFrontRight, 2);
  // Serial.print(" kg | Back Left: "); Serial.print(weightBackLeft, 2);
  // Serial.print(" kg | Back Right: "); Serial.print(weightBackRight, 2);
  // Serial.println(" kg");

  // Serial.print("FL raw: "); Serial.print(rawFrontLeft);
  // Serial.print(" | FR raw: "); Serial.print(rawFrontRight);
  // Serial.print(" | BL raw: "); Serial.print(rawBackLeft);
  // Serial.print(" | BR raw: "); Serial.println(rawBackRight);

  delay(500);
}
