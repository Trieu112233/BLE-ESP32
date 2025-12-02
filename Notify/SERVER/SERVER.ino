/* * ESP32 BLE Server - FIX COMPILATION ERROR V3.X
 * Chức năng: Gửi tốc độ cao (Best Effort).
 * Lưu ý: Trên Core 3.0+, hàm notify() trả về void nên ta không đếm được chính xác
 * số gói bị drop tại nguồn, nhưng cơ chế vẫn là gửi liên tục không chờ (Non-blocking).
 */

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <esp_task_wdt.h>

// --- CẤU HÌNH TEST ---
const int BURST_SIZE = 1;        // Gửi 1 lèo 500 gói
const int DELAY_MICROS = 200;     // Nghỉ giữa các burst
const uint32_t TOTAL_PACKETS = 100000; 
// ---------------------

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
bool testRunning = false;

uint32_t packetCounter = 0;
unsigned long startTime = 0;

struct DataPacket {
  uint32_t seqNumber;  
  uint8_t  payload[16]; 
};

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      // Đã bỏ dòng updateConnParams gây lỗi. 
      // Client sẽ tự yêu cầu tăng tốc độ khi kết nối.
      BLEDevice::startAdvertising();
      Serial.println(">> Client Connected! Type 's' to start.");
    };
    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println(">> Client Disconnected");
    }
};

void setup() {
  Serial.begin(115200);

  // 1. Cấu hình Watchdog (30s)
  esp_task_wdt_config_t wdt_config = {
      .timeout_ms = 30000,
      .idle_core_mask = (1 << 0) | (1 << 1),
      .trigger_panic = false
  };
  esp_task_wdt_init(&wdt_config); 
  esp_task_wdt_add(NULL); 

  // 2. Init BLE
  BLEDevice::init("ESP32_Server_Fixed");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  pCharacteristic->addDescriptor(new BLE2902());
  pService->start();
  
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);
  BLEDevice::startAdvertising();
  
  Serial.println("SERVER READY (V3.X Fixed). Send 's' to start.");
}

void loop() {
  esp_task_wdt_reset(); // Cho chó ăn

  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == 's' && deviceConnected && !testRunning) {
      testRunning = true;
      packetCounter = 0;
      startTime = millis();
      Serial.println("--- STARTING HIGH SPEED TEST ---");
    }
  }

  if (deviceConnected && testRunning) {
    if (packetCounter < TOTAL_PACKETS) {
      
      // --- VÒNG LẶP BURST ---
      for (int i = 0; i < BURST_SIZE; i++) {
          if (packetCounter >= TOTAL_PACKETS) break;

          DataPacket pkt;
          pkt.seqNumber = packetCounter; 
          
          pCharacteristic->setValue((uint8_t*)&pkt, sizeof(DataPacket));
          
          // Dùng hàm chuẩn của thư viện. 
          // Nó sẽ tự động đẩy xuống buffer controller. 
          // Nếu buffer đầy, packet này sẽ bị drop ngầm (hoặc chờ rất ngắn).
          pCharacteristic->notify(); 

          // Tăng luôn counter để không bao giờ bị kẹt lại (Skip logic)
          packetCounter++;

          // Cứ 50 gói thì cho chó ăn 1 miếng để tránh Reset
          if (i % 50 == 0) {
             esp_task_wdt_reset();
          }
      }
      
      // --- NGHỈ GIỮA CÁC BURST ---
      if (DELAY_MICROS > 0) {
        delayMicroseconds(DELAY_MICROS);
      }
      
      // Delay thật 1ms mỗi 2000 gói để hệ thống xử lý Background
      // Đây là chìa khóa để không bị Crash 5k
      if (packetCounter % 2000 == 0) {
         delay(1); 
         Serial.printf("Sent (Best Effort): %d\n", packetCounter);
      }

      // --- KẾT THÚC TEST ---
      if (packetCounter >= TOTAL_PACKETS) {
        testRunning = false;
        unsigned long duration = millis() - startTime;
        float speed = (float)TOTAL_PACKETS * 1000.0 / duration;
        
        Serial.println("\n--- TEST FINISHED ---");
        Serial.printf("Time: %lu ms\n", duration);
        Serial.printf("Throughput speed: %.2f pkt/s\n", speed);
      }
    }
  }

  // Tự động quảng cáo lại
  if (!deviceConnected && oldDeviceConnected) {
      delay(500); 
      pServer->startAdvertising(); 
      oldDeviceConnected = deviceConnected;
  }
  if (deviceConnected && !oldDeviceConnected) {
      oldDeviceConnected = deviceConnected;
  }
}