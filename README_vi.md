# Kết Nối P2P qua UDP Hole Punching

## Tổng Quan

**UDP Hole Punching** là một kỹ thuật được sử dụng để thiết lập kết nối trực tiếp peer-to-peer (P2P) giữa các thiết bị phía sau NAT (Network Address Translation). Phương pháp này cho phép các peer giao tiếp mà không cần phải có địa chỉ IP công khai trực tiếp.

## Sử Dụng Server

- **Máy chủ Signaling**: `broker.emqx.io:1883`
- **Máy chủ STUN**: `stun.l.google.com:19302`

## Mô Tả Quy Trình

### 1. **Signaling qua MQTT**
   - Các peer sử dụng MQTT như một cơ chế signaling để trao đổi thông tin kết nối.
   
### 2. **Lấy Địa Chỉ IP Công Qua STUN**
   - Mỗi peer truy vấn máy chủ STUN để xác định địa chỉ IP công và cổng của nó.
   - Bộ định tuyến tạm thời giữ cổng công mở trong một khoảng thời gian ngắn sau yêu cầu này, cho phép các gói tin đến từ cùng một điểm bên ngoài.

### 3. **Tạo Socket UDP**
   - Mỗi peer khởi tạo một socket UDP trên cổng cục bộ của nó.

### 4. **Công Bố Thông Tin Candidate**
   - Các peer kết nối tới máy chủ MQTT và công bố thông tin kết nối của chúng (IP, cổng) lên một chủ đề được chỉ định.

### 5. **Thu Thập Candidate**
   - Mỗi peer nhận thông tin kết nối của peer còn lại.
   - Quá trình thiết lập kết nối phụ thuộc vào cấu hình mạng:
     - **Trường hợp 1**: Cả hai peer đều ở sau cùng một NAT và trên cùng một máy.
     - **Trường hợp 2**: Cả hai peer đều ở sau cùng một NAT nhưng trên các máy khác nhau trong cùng một mạng nội bộ.
     - **Trường hợp 3**: Các peer ở sau các NAT khác nhau, yêu cầu mỗi peer phải ánh xạ cổng cục bộ của mình vào cổng công đã nhận từ máy chủ STUN.
     
### 6. **UDP Hole Punching**
   - Sau khi các peer trao đổi thông tin kết nối và nhận được các lệnh, chúng sẽ khởi động giao tiếp qua các cổng đã ánh xạ.
   - Quá trình hole punching thiết lập một kết nối UDP trực tiếp giữa các peer.

## Cách Chạy

Để chạy ứng dụng, chỉ cần thực hiện lệnh sau:

```bash
make
```
Chương trình sau khi xây dựng bao gồm Peer1 và Peer2 có thể chạy với 3 trường hợp thử nghiệm
    - Chạy trên cùng một máy Linux trên hai terminal.
    - Chạy trên hai máy Linux kết nối với cùng một mạng.
    - Chạy trên hai máy Linux nằm trên các nhà cung cấp dịch vụ Internet khác nhau.

## Reference
[P2P-NAT](https://bford.info/pub/net/p2pnat)