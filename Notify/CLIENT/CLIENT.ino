/* * ESP32 BLE Client - AGGRESSIVE DROP TEST (STRESS TEST)
 * Chức năng: Gửi WriteWithoutResponse tốc độ tối đa.
 * Logic: Nếu Buffer đầy -> Drop ngay lập tức -> Gửi gói tiếp theo.
 * Mục tiêu: Kiểm tra giới hạn chịu đựng của Buffer và tốc độ xử lý của CPU.
 */


#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <esp_task_wdt.h>
#include "esp_gattc_api.h" // Thư viện cấp thấp cho hàm gửi non-blocking


// --- CẤU HÌNH ÉP XUNG ---
// Gửi một mạch lớn để ép buffer tràn nhanh chóng
const int BURST_SIZE = 20000;      
// Không nghỉ ngơi giữa các gói tin
const int DELAY_MICROS = 500;        
const uint32_t TOTAL_PACKETS = 100000;
// ------------------------


// UUIDs (Phải khớp với Server)
static BLEUUID serviceUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
static BLEUUID charUUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");


// Biến trạng thái kết nối
static boolean doConnect = false;
static boolean connected = false;
static boolean startScanning = false;
static BLEAddress *pServerAddress = NULL;
static BLERemoteCharacteristic* pRemoteCharacteristic = NULL;
static BLEClient* pClient = NULL;


// Biến thống kê
uint32_t packetCounter = 0;
uint32_t droppedAtSource = 0; // Đếm số gói bị Client vứt bỏ do đầy Buffer
bool testRunning = false;
unsigned long startTime = 0;


// Cấu trúc gói tin
struct DataPacket {
  uint32_t seqNumber;  
  uint8_t  payload[16];
};


// Callback kết nối
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    // Chưa làm gì ở đây
  }
  void onDisconnect(BLEClient* pclient) {
    connected = false;
    testRunning = false;
    Serial.println(">> Disconnected!");
    startScanning = true;
  }
};


// Hàm kết nối Server
bool connectToServer() {
    Serial.printf("Connecting to: %s\n", pServerAddress->toString().c_str());
   
    // Tạo Client mới
    pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallback());


    // Kết nối
    if (!pClient->connect(*pServerAddress)) {
        Serial.println("Connect Failed!");
        return false;
    }
   
    // Tăng MTU để tối ưu băng thông
    pClient->setMTU(247);


    // Tìm Service
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
        Serial.println("Failed to find Service UUID");
        return false;
    }


    // Tìm Characteristic
    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
        Serial.println("Failed to find Characteristic UUID");
        return false;
    }


    connected = true;
    Serial.println(">> CONNECTED! READY TO STRESS TEST.");
    return true;
}


// Callback khi tìm thấy thiết bị
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    // Tìm đúng tên Server
    if (advertisedDevice.getName() == "ESP32_WWR_Server") {
      BLEDevice::getScan()->stop();
      pServerAddress = new BLEAddress(advertisedDevice.getAddress());
      doConnect = true;
      startScanning = false;
    }
  }
};


void setup() {
  Serial.begin(115200);
  delay(2000); // Chờ ổn định nguồn


  // --- CẤU HÌNH WATCHDOG (FIX LỖI RESET) ---
  // Hủy cấu hình mặc định (5s)
  esp_task_wdt_deinit();


  // Cài đặt lại cấu hình mới (30s)
  esp_task_wdt_config_t wdt_config = {
      .timeout_ms = 30000,
      .idle_core_mask = (1 << 0) | (1 << 1),
      .trigger_panic = false
  };
 
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);
  // ------------------------------------------


  Serial.println("Client Aggressive Drop Test Started...");
  BLEDevice::init("ESP32_Stress_Client");
 
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(45);
  pBLEScan->setWindow(45);  
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);
}


void loop() {
  // Cho chó ăn ở vòng lặp chính
  esp_task_wdt_reset();


  // Xử lý kết nối
  if (doConnect) {
    if (connectToServer()) {
      Serial.println("Send 's' to start AGGRESSIVE DROP TEST.");
    } else {
      startScanning = true;
    }
    doConnect = false;
  }


  // Nhận lệnh từ Serial
  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == 's' && connected && !testRunning) {
       Serial.println("--- KICH HOAT SERVER & CHO 1 GIAY... ---");
       
       // BƯỚC 1: Gửi thủ công gói số 0 để Server nhận và in dòng "START RECEIVING"
       DataPacket pkt;
       pkt.seqNumber = 0;
       
       // Gửi gói mồi (Seq 0)
       if (pRemoteCharacteristic != NULL) {
           pRemoteCharacteristic->writeValue((uint8_t*)&pkt, sizeof(DataPacket), false);
       }
       
       // BƯỚC 2: Nghỉ 1 giây
       // Mục đích: Cho Server thời gian in Log, dọn buffer, ổn định tâm lý
       delay(1000);


       // BƯỚC 3: Bắt đầu đua tốc độ từ gói số 1
       Serial.println("--- GO !!! (FULL SPEED) ---");
       
       packetCounter = 1; // Quan trọng: Bắt đầu vòng lặp từ số 1 (vì số 0 gửi rồi)
       droppedAtSource = 0;
       testRunning = true; // Bật cờ để vòng lặp phía dưới chạy
       startTime = millis();
    }
  }


  // VÒNG LẶP GỬI DỮ LIỆU CHÍNH
  if (connected && testRunning && pClient != NULL) {
      if (packetCounter < TOTAL_PACKETS) {
         
          // Lấy thông tin cần thiết để gọi hàm cấp thấp (để ngoài vòng lặp cho nhanh)
          uint16_t conn_id = pClient->getConnId();
          uint16_t handle = pRemoteCharacteristic->getHandle();
          esp_gatt_if_t gatt_if = pClient->getGattcIf();


          // Vòng lặp BURST cực lớn
          for (int i = 0; i < BURST_SIZE; i++) {
              if (packetCounter >= TOTAL_PACKETS) break;


              DataPacket pkt;
              pkt.seqNumber = packetCounter;
             
              // 1. GỬI NON-BLOCKING
              esp_err_t err = esp_ble_gattc_write_char(
                  gatt_if,
                  conn_id,
                  handle,
                  sizeof(DataPacket),
                  (uint8_t*)&pkt,
                  ESP_GATT_WRITE_TYPE_NO_RSP,
                  ESP_GATT_AUTH_REQ_NONE
              );


              // 2. LOGIC DROP
              if (err != ESP_OK) {
                  droppedAtSource++;
                  // Vẫn không chờ, đi tiếp ngay
              }
             
              packetCounter++;


              // 3. QUAN TRỌNG: CỨU IDLE TASK
              // Cứ mỗi 1000 gói (tức là sau khoảng vài chục ms),
              // ta phải ngủ 1ms để IDLE Task được chạy và reset watchdog của nó.
              // Nếu không có dòng này, IDLE Task chết -> Reset chip.
              if (i % 1000 == 0) {
                  delay(1);
                  esp_task_wdt_reset(); // Tiện tay reset luôn cho mình
              }
          }


          // Nghỉ giữa các Burst (nếu cấu hình > 0)
          if (DELAY_MICROS > 0) delayMicroseconds(DELAY_MICROS);


          // Log tiến độ mỗi 5000 gói
          if (packetCounter % 5000 == 0) {
              // Delay nhẹ 1ms ở đây để Serial kịp in ra và Wifi/BT Stack không chết hẳn
              delay(1);
              Serial.printf("Gen: %d | DROP (Full Buffer): %d\n", packetCounter, droppedAtSource);
          }


          // Kết thúc
          if (packetCounter >= TOTAL_PACKETS) {
              testRunning = false;
              unsigned long duration = millis() - startTime;
              float speed = (float)TOTAL_PACKETS * 1000.0 / duration;
             
              Serial.println("--- DONE ---");
              Serial.printf("Time: %lu ms | Speed: %.2f pkt/s\n", duration, speed);
              Serial.printf("Total Generated: %d\n", TOTAL_PACKETS);
              Serial.printf("Dropped at Source: %d\n", droppedAtSource);
              Serial.printf("Sent to Air: %d\n", TOTAL_PACKETS - droppedAtSource);
          }
      }
  }


  // Quét lại nếu mất kết nối
  if (startScanning) {
       BLEDevice::getScan()->start(5, false);
       startScanning = false;
  }
}
