# Kết Nối P2P qua UDP Hole Punching

## Summary

**UDP Hole Punching** là một kỹ thuật được sử dụng để thiết lập kết nối trực tiếp peer-to-peer (P2P) giữa các thiết bị phía sau NAT (Network Address Translation). Phương pháp này cho phép các peer giao tiếp trực tiếp với nhau mà không cần phải qua server trung gian nào khác.

## Server Usage

- **Máy chủ Signaling**: `broker.emqx.io:1883`
- **Máy chủ STUN**: `stun.l.google.com:19302`

## Description

### 1. **Signaling qua MQTT**
   - Sử dụng MQTT cho cơ chế signaling để trao đổi **Candidate** giữa các thiết bị.

### 2. **STUN Protocol**   
   - Lấy **IP_PUBLIC** và mở **PORT_PUBLIC** thông qua việc gửi query tới **STUN SEVER**. Router sẽ tạm thời giữ **PORT_PUBLIC** này trong một khoảng thời gian ngắn.
   - Việc sử dụng **STUN** rất quan trọng cho UDP Hole Punchinig, thiết bị Behind NAT sẽ mapped **PORT_LOCAL** của nó vào **PORT_PUBLIC** này để gửi message tới thiết bị khác Behind NAT.

### 3. **Gathering Candidate**
   - Quá trình thiết lập kết nối phụ thuộc vào:
     - **Case 1**: Cả hai thiết bị đều ở sau cùng một NAT và trên cùng một máy chủ.
     - **Case 2**: Cả hai thiết bị đều ở sau cùng một NAT nhưng trên các máy khác nhau trong cùng một mạng nội bộ.
     - **Case 3**: Cả hai thiết bị đều ở sau hai NAT khác nhau, yêu cầu mỗi thiết bị phải **mapped** **PORT_LOCAL** của bản thân vào **PORT_PUBLIC**.
     
### 4. **UDP Hole Punching**
   - Các thiết bị sẽ bắt đầu gửi/nhận message dựa trên các **Candidate** đã nhận được từ phía còn lại.

## How to use ?

Để chạy ứng dụng, chỉ cần thực hiện lệnh sau:
- Install dependency
  ```
  sudo apt update
  sudo apt install libmosquitto-dev
  ```
 - Build
   ```bash
   make
   ```
Chương trình sau khi build bao gồm 2 application **Peer1** và **Peer2** có thể chạy với 3 test case:
- Chạy trên cùng một máy Linux trên hai terminal.
- Chạy trên hai máy Linux kết nối với cùng một mạng.
- Chạy trên hai máy Linux nằm trên các nhà cung cấp dịch vụ Internet khác nhau.

## Reference
 - [P2P-NAT](https://bford.info/pub/net/p2pnat)
