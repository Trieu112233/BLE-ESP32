/* * ESP32 BLE Client - STRESS TEST (OPTIMIZED)
 * Chức năng: Gửi 1 triệu gói tin tốc độ cao, không crash.
 */

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <esp_task_wdt.h>
#include "esp_gattc_api.h" 

// --- CẤU HÌNH ---
const uint32_t TOTAL_PACKETS = 1000000; // 1 Triệu gói
const int PAYLOAD_SIZE = 20;            // 20 Byte (Tăng lên 240 nếu muốn ép tràn buffer nhanh hơn)

// UUIDs
static BLEUUID serviceUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
static BLEUUID charUUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");

// Biến hệ thống
static BLEAddress *pServerAddress = NULL;
static BLERemoteCharacteristic* pRemoteCharacteristic = NULL;
static BLEClient* pClient = NULL;
bool connected = false;
bool doConnect = false;

// Cấu trúc gói tin
struct DataPacket {
  uint32_t seqNumber;  
  uint8_t  payload[PAYLOAD_SIZE]; 
};

// Callback kết nối
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    Serial.println(">> Connected to Server");
  }
  void onDisconnect(BLEClient* pclient) {
    connected = false;
    Serial.println(">> Disconnected!");
  }
};

bool connectToServer() {
    pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallback());

    if (!pClient->connect(*pServerAddress)) return false;
    
    // 1. TĂNG MTU
    pClient->setMTU(247);

    // 2. ÉP TỐC ĐỘ KẾT NỐI CAO NHẤT (QUAN TRỌNG)
    // Min 6 * 1.25 = 7.5ms (Nhanh nhất chuẩn BLE cho phép)
    pClient->setConnectionParams(pServerAddress->getAddress(), 6, 12, 0, 100);

    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) return false;

    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) return false;

    connected = true;
    return true;
}

// Scan Callback
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.getName() == "ESP32_WWR_Server") { 
      BLEDevice::getScan()->stop();
      pServerAddress = new BLEAddress(advertisedDevice.getAddress());
      doConnect = true;
    }
  }
};

void setup() {
  Serial.begin(115200);
  
  // Cấu hình Watchdog 30s để tránh reset oan
  esp_task_wdt_deinit();
  esp_task_wdt_config_t wdt_config = { .timeout_ms = 30000, .idle_core_mask = (1 << 0) | (1 << 1), .trigger_panic = true };
  esp_task_wdt_init(&wdt_config); 
  esp_task_wdt_add(NULL); 

  BLEDevice::init("ESP32_Gen_Client");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); 
  pBLEScan->start(5, false); 
  Serial.println(">> Client Ready. Waiting for Scan...");
}

void loop() {
  esp_task_wdt_reset(); // Reset WDT ở loop chính
if (doConnect) {
    if (connectToServer()) {
       Serial.println(">> Sẵn sàng! Gõ 's' để gửi 1 TRIỆU gói.");
    }
    doConnect = false;
  }

  if (Serial.available()) {
    char c = Serial.read();
    if (c == 's' && connected) {
       Serial.println("--- BẮT ĐẦU GỬI 1.000.000 GÓI ---");
       unsigned long startTime = millis();
       uint32_t dropped = 0;

       uint16_t conn_id = pClient->getConnId();
       uint16_t handle = pRemoteCharacteristic->getHandle();
       esp_gatt_if_t gatt_if = pClient->getGattcIf();

       // --- VÒNG LẶP HỦY DIỆT ---
       for (uint32_t i = 0; i < TOTAL_PACKETS; i++) {
           DataPacket pkt;
           pkt.seqNumber = i;
           
           // Gửi Non-Blocking (Nhanh nhất có thể)
           esp_err_t err = esp_ble_gattc_write_char(
               gatt_if, conn_id, handle, sizeof(DataPacket), (uint8_t*)&pkt,
               ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE
           );

           if (err != ESP_OK) dropped++; // Nếu Buffer đầy thì đếm lỗi

           // --- CHÌA KHÓA CHỐNG RESET ---
           // Cứ mỗi 500 gói, nghỉ 1 xíu để CPU xử lý Wifi/BT background task
           // Nếu không có dòng này: Watchdog sẽ cắn -> Reset.
           if (i % 500 == 0) { 
               delay(1); 
               esp_task_wdt_reset(); 
           }

           // In tiến độ mỗi 50.000 gói (đừng in nhiều quá làm chậm)
           if (i > 0 && i % 50000 == 0) {
               Serial.printf("Sent: %d | Dropped: %d\n", i, dropped);
           }
       }
       
       unsigned long time = millis() - startTime;
       Serial.println("--- HOÀN TẤT ---");
       Serial.printf("Total Time: %lu ms\n", time);
       Serial.printf("Speed: %.2f pkt/s\n", (float)TOTAL_PACKETS * 1000 / time);
       Serial.printf("Dropped at Source: %d\n", dropped);
    }
  }
}
