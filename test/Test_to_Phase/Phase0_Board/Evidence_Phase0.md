# Phase 0 Contract/Feasibility Evidence

> Tác dụng: lưu gate tái lập cho contract, linker và representative firmware.

Phase 0 không xác nhận hành vi W25Q32/AS32/Internal Flash trên mạch. Các hành vi
đó thuộc Phase 1, 2 và 4 trở đi.

## Lệnh tái lập

```bash
cmake --preset debug
cmake --build --preset debug

cmake --preset release
cmake --build --preset release
```

## Contract đã khóa

- STM32F103C8T6: 64 KB Flash, 20 KB SRAM.
- Bootloader: 16 KB tại `0x08000000`.
- Application: 46 KB tại `0x08004000`.
- Metadata A/B: mỗi page 1 KB tại cuối Internal Flash.
- W25Q32: Staging 256 KB, Backup 256 KB, Delta 256 KB, còn lại Reserved.
- Linker dành 4 KB stack và chặn section vượt partition.
- C header dùng `_Static_assert` để khóa persistent/wire byte layout.

## Kết quả source mẫu hiện tại

| Build | Bootloader Flash | Application Flash | Application static RAM |
|---|---:|---:|---:|
| Debug (`-Os -g3`, LTO) | 15,376 B / 16 KB | 25,056 B / 46 KB | 1,924 B / 20 KB |
| Release (`-Os`, LTO) | 15,376 B / 16 KB | 25,056 B / 46 KB | 1,924 B / 20 KB |

Các số Flash trong bảng gồm `text + data`; static RAM gồm `data + bss`. Stack
runtime/fault behavior vẫn phải đo trên board ở phase sở hữu đường chạy đó.
