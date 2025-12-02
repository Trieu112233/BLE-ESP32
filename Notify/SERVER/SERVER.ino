/* * ESP32 BLE Server - ANALYZER (Event Driven Log)
 * Chức năng: Đếm số gói nhận được và in Log mỗi 5000 gói.
 */


#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>


#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"


// Biến thống kê
volatile uint32_t totalReceived = 0;
volatile uint32_t totalLost = 0;
uint32_t expectedSeq = 0;
bool firstPacket = true;
bool testRunning = false;


// Callback nhận dữ liệu
class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      uint8_t* pData = pCharacteristic->getData();
     
      if (pData != NULL) {
          uint32_t currentSeq;
          memcpy(&currentSeq, pData, 4);


          if (firstPacket) {
             Serial.println("\n--- BẮT ĐẦU NHẬN DỮ LIỆU ---");
             expectedSeq = currentSeq;
             totalReceived = 0;
             totalLost = 0;
             firstPacket = false;
             testRunning = true;
          }


          if (currentSeq != expectedSeq) {
             if (currentSeq > expectedSeq) {
                 totalLost += (currentSeq - expectedSeq);
             } else {
                 expectedSeq = currentSeq;
             }
          }


          totalReceived++;
          expectedSeq = currentSeq + 1;
      }
    }
};


class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      Serial.println(">> Client Connected");
    };
    void onDisconnect(BLEServer* pServer) {
      Serial.println(">> Client Disconnected");
      firstPacket = true;
      testRunning = false;
      BLEDevice::startAdvertising();
    }
};


void setup() {
  Serial.begin(115200);
  BLEDevice::init("ESP32_WWR_Server");
 
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());


  BLEService *pService = pServer->createService(SERVICE_UUID);


  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID,
                                         BLECharacteristic::PROPERTY_WRITE_NR
                                       );


  pCharacteristic->setCallbacks(new MyCallbacks());
  pService->start();


  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setMinPreferred(0x06);
  BLEDevice::startAdvertising();
  Serial.println("Server Ready. Waiting...");
}


void loop() {
  // Biến lưu trạng thái của lần in trước
  static uint32_t lastPrintedCount = 0;
 
  if (testRunning) {
      // Copy biến volatile ra biến thường để tính toán an toàn
      uint32_t rx = totalReceived;
      uint32_t lost = totalLost;


      // Reset lastPrintedCount nếu Client chạy lại từ đầu (rx nhỏ hơn lần in trước)
      if (rx < lastPrintedCount) {
          lastPrintedCount = 0;
      }


      // LOGIC MỚI: Chỉ in khi đã nhận thêm được >= 5000 gói
      if (rx - lastPrintedCount >= 50) {
          float lossRate = 0;
          if (rx + lost > 0) lossRate = (float)lost / (rx + lost) * 100.0;
         
          Serial.printf("[BATCH] Rx: %d | Lost: %d | Loss Rate: %.2f%%\n", rx, lost, lossRate);
         
          // Cập nhật mốc in
          lastPrintedCount = rx;
      }
  }
}
