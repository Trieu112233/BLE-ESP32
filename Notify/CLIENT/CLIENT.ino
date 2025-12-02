/* * ESP32 BLE Notify Test - CLIENT (100k Packets)
 */

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// UUIDs
static BLEUUID serviceUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
static BLEUUID charUUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");

static boolean doConnect = false;
static boolean connected = false;
static boolean startScanning = false; 
static BLEAdvertisedDevice* myDevice;

// Thống kê
uint32_t expectedSeq = 0;    
uint32_t totalReceived = 0;  
uint32_t totalLost = 0;      
bool firstPacket = true; 

static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
    
    if (length >= 4) {
        uint32_t receivedSeq = *(uint32_t*)pData; 

        if (firstPacket) {
            expectedSeq = receivedSeq;
            firstPacket = false;
            totalReceived = 0;
            totalLost = 0;
            Serial.println("\n--- START STREAM (100k Test) ---");
        }

        if (receivedSeq != expectedSeq) {
            if (receivedSeq < expectedSeq) {
                 expectedSeq = receivedSeq; 
            } else {
                 uint32_t lostCount = receivedSeq - expectedSeq;
                 totalLost += lostCount;
            }
        }

        totalReceived++;
        expectedSeq = receivedSeq + 1;

        // CHỈ IN MỖI 5000 GÓI (Để đỡ lag)
        if (totalReceived % 5000 == 0) {
            float lossRate = (totalReceived + totalLost) > 0 ? (float)totalLost / (totalReceived + totalLost) * 100.0 : 0.0;
            Serial.printf("Rx: %d | Lost: %d | Loss: %.2f%%\n", totalReceived, totalLost, lossRate);
        }
        
        // KHI NHẬN ĐỦ 100k GÓI (Check khoảng cuối cùng)
        if (receivedSeq >= 99990 && receivedSeq < 100010) { 
             float lossRate = (float)totalLost / (totalReceived + totalLost) * 100.0;
             Serial.println("--- 100k PACKETS DONE ---");
             Serial.printf("KET QUA: Rx: %d | Lost: %d | Loss Rate: %.4f%%\n", totalReceived, totalLost, lossRate);
        }
    }
}

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    Serial.println(">> Da ket noi Server!");
    connected = true;
  }
  void onDisconnect(BLEClient* pclient) {
    connected = false;
    Serial.println(">> Mat ket noi!");
    firstPacket = true; 
    startScanning = true;
  }
};

bool connectToServer() {
    Serial.print("Dang ket noi... ");
    
    BLEClient* pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallback());

    if (!pClient->connect(myDevice)) return false;

    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
      pClient->disconnect();
      return false;
    }

    BLERemoteCharacteristic* pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
      pClient->disconnect();
      return false;
    }

    if(pRemoteCharacteristic->canNotify()) {
      pRemoteCharacteristic->registerForNotify(notifyCallback);
      Serial.println("OK! Cho du lieu...");
    }

    return true;
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("Thay: ");
    Serial.println(advertisedDevice.getName().c_str());
    
    if (advertisedDevice.getName() == "ESP32_Server_Fixed") { 
      Serial.println(" -> DUNG LA SERVER! KET NOI...");
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      startScanning = false; 
    }
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Client 100k Test Started...");
  BLEDevice::init("");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(45); 
  pBLEScan->setWindow(45);   
  pBLEScan->setActiveScan(true); 
  pBLEScan->start(0, false); 
}

void loop() {
  if (doConnect == true) {
    if (!connectToServer()) {
      Serial.println("Ket noi loi. Quet lai...");
      startScanning = true;
    }
    doConnect = false;
  }
  
  if (startScanning) {
       startScanning = false; 
       BLEDevice::getScan()->start(0, false); 
  }
  delay(100); 
}