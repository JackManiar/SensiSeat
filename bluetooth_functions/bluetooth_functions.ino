#include <ArduinoBLE.h>
#define LED_BUILTIN 2

// Holds the packet of information to be sent to the companion app
// Condensing into a struct allows for effeciency and modularity 
typedef struct posture_packet_t {
  float weightFrontLeft;
  float weightFrontRight;
  float weightBackLeft;
  float weightBackRight; 
} posture_packet_t;

// Groups the info into a BLE service
BLEService postureService("FEED");
// The BLE characteristic holding the struct
BLECharacteristic postureInfo("F00D", BLERead | BLENotify, sizeof(posture_packet_t), true);
// reference to the connected BLE central
BLEDevice central;

// Call to setup BLE and advertise
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

// demo arduino setup() function -- REMOVE WHEN DONE
void setup() {
  Serial.begin(9600);    
  while (!Serial);

  pinMode(LED_BUILTIN, OUTPUT); 

  // begin initialization
  if (!BLE.begin()) {
    Serial.println("starting Bluetooth® Low Energy module failed!");

    while (1);
  }

  BLE_setup();
  BLE_connect();
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("Bluetooth® device active, waiting for connections...");
}
//demo arduino loop() function -- REMOVE WHEN DONE
void loop() {
  long previousMillis = 0;
  while (central.connected()) {
    long currentMillis = millis();
    if (currentMillis - previousMillis >= 500) {
      previousMillis = currentMillis;
      posture_packet_t packet;
      packet.weightBackLeft = (float)random(20);
      packet.weightBackRight = (float)random(20);
      packet.weightFrontLeft = (float)random(20);
      packet.weightFrontRight = (float)random(20);

      BLE_updatePosture(packet);
    }
  }
  // when the central disconnects, turn off the LED:
  digitalWrite(LED_BUILTIN, LOW);
  Serial.print("Disconnected from central: ");
  Serial.println(central.address());
}
