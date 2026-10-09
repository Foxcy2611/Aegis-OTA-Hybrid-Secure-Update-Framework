# ELF, BIN và HEX khác nhau thế nào?

| File | Dùng để làm gì |
|---|---|
| `.elf` | OpenOCD, GDB, debug; chứa symbol và địa chỉ |
| `.hex` | Nạp bằng programmer; vẫn chứa địa chỉ |
| `.bin` | Dữ liệu firmware thô; không chứa địa chỉ |
| `.map` | Kiểm tra symbol và bố trí Flash/RAM |
| `.lst` | Assembly/disassembly để điều tra |