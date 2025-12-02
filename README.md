# ESP32 BLE Performance & Stress Test

## 1. Giới thiệu
Repository này chứa mã nguồn cho hệ thống kiểm thử hiệu năng giao thức **Bluetooth Low Energy (BLE)** giữa hai bo mạch ESP32. Dự án được thiết kế để đánh giá khả năng truyền tải dữ liệu tốc độ cao, kiểm tra độ trễ (latency), và đo lường tỷ lệ mất gói tin (packet loss) trong các trường hợp chịu tải lớn.

Mục tiêu chính:
- **Stress Test**: Gửi liên tục lượng lớn gói tin (lên đến 100000 gói) để kiểm tra độ ổn định.
- **Analyzer**: Phía Server phân tích số lượng gói tin nhận được và phát hiện các gói tin bị mất dựa trên Sequence Number.
- So sánh hiệu quả giữa các phương thức truyền tin (Write Without Response vs Notify).

## 2. Cấu trúc thư mục
Dự án được chia thành các thư mục dựa trên phương thức giao tiếp BLE:

```
├── WNR/                  # Chế độ Write Without Response (Tối ưu tốc độ)
│   ├── CLIENT/           # Code cho ESP32 đóng vai trò Client (Gửi dữ liệu)
│   └── SERVER/           # Code cho ESP32 đóng vai trò Server (Nhận & Phân tích)
│
├── Notify/               # Chế độ Notify (Sử dụng cơ chế thông báo)
│   ├── CLIENT/           # Code cho ESP32 Client (Stress Test - Aggressive Drop)
│   └── SERVER/           # Code cho ESP32 Server (Event Driven Log)
```

## 3. Yêu cầu phần cứng
- **Board**: 2 x ESP32 WROOM 32 (Một board nạp code Client, một board nạp code Server).

## 4. Môi trường phát triển
- **IDE**: Arduino IDE.
- **Thư viện**: `BLEDevice`, `BLEUtils`, `BLEServer`, `BLEClient` (có sẵn trong gói cài đặt ESP32 cho Arduino).
- **Cấu hình Board trong IDE**: `DOIT ESP32 DEVKIT V1`.

## 5. Nhóm thực hiện (Contributors)
Dự án được thực hiện bởi nhóm 4 sinh viên:

| STT | Họ và tên | Mã sinh viên |
|:---:|:---|:---:|
| 1 | Nguyễn Văn Diện | 22022158 |
| 2 | Nguyễn Minh Đức | 22022207 |
| 3 | Quách Ngọc Quang | 22022132 |
| 4 | Nguyễn Đức Triệu | 22022110 |

---
*Lưu ý: Dự án phục vụ mục đích học tập và nghiên cứu hiệu năng BLE trên vi điều khiển ESP32.*
