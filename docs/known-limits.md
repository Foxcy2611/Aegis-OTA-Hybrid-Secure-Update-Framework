# Known Limits

> **Phạm vi (Scope):** Giới hạn và non-claims của scope P2P.

> **Tác dụng:** Làm rõ những khả năng dự án không cam kết để tránh hiểu sai phạm vi.


Bản P2P không claim:

- LoRaWAN;
- Network Server;
- FUOTA/multicast;
- fleet-scale scheduling;
- physical tamper resistance;
- secure-element key storage;
- bootloader self-update;
- secure remote recovery khi Application không còn chạy được;
- guaranteed RF range/capacity khi chưa benchmark.

Session Journal chứa tối đa 4096 session ID trên mỗi chu kỳ provisioning.
Khi đầy/corrupt, secure transport từ chối session mới; thiết bị cần local
reprovision transport key rồi erase journal, không có remote erase command.

ST-Link/SWD là Recovery baseline. Mọi claim P2P/FULL/Delta chỉ hợp lệ khi có đúng
production-path evidence.
