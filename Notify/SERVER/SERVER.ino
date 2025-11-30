/* * ESP32 BLE Server - ANALYZER
 * Chức năng: Đếm số gói nhận được và số gói bị mất dựa trên Sequence Number.
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
      // 1. Lấy con trỏ dữ liệu thô (Tránh copy string gây chậm)
      uint8_t* pData = pCharacteristic->getData();
      
      // Kiểm tra độ dài an toàn (4 byte seq + payload)
      // Lưu ý: getValue() trả về std::string, ta lấy length từ đó hoặc pCharacteristic->getLength() (tuỳ version)
      // Ở đây giả định payload luôn đúng format để tối ưu tốc độ
      if (pData != NULL) {
        uint32_t currentSeq;
          // Copy 4 byte đầu tiên vào biến seq (Cực nhanh)
          memcpy(&currentSeq, pData, 4);

          // 2. Logic kiểm tra Sequence
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
                 // Phát hiện lỗ hổng
                 totalLost += (currentSeq - expectedSeq);
             } else {
                 // Gói đến trễ hoặc Client reset
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
      BLEDevice::startAdvertising(); // Quảng cáo lại ngay
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
                                         BLECharacteristic::PROPERTY_WRITE_NR // Write Without Response
                                       );

  pCharacteristic->setCallbacks(new MyCallbacks());
  pService->start();
BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setMinPreferred(0x06); // Gợi ý kết nối nhanh
  BLEDevice::startAdvertising();
  Serial.println("Server Ready. Waiting...");
}

void loop() {
  // Chỉ in thống kê mỗi 1 giây (để không làm chậm quá trình nhận)
  static unsigned long lastPrint = 0;
  
  if (testRunning && millis() - lastPrint > 1000) {
      // Copy biến volatile ra để in cho an toàn
      uint32_t rx = totalReceived;
      uint32_t lost = totalLost;
      
      float lossRate = 0;
      if (rx + lost > 0) lossRate = (float)lost / (rx + lost) * 100.0;
      
      Serial.printf("[LIVE] Rx: %d | Lost: %d | Loss Rate: %.2f%%\n", rx, lost, lossRate);
      lastPrint = millis();
  }
}