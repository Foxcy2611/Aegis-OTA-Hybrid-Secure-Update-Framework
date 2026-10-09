# Fault Injection

> **Phạm vi (Scope):** Fault matrix và correction: FAULT không phải production state.

> **Tác dụng:** Hướng dẫn chèn lỗi có kiểm soát để chứng minh hệ thống phục hồi đúng.


Không thiết kế:

```text
FAULT -> RECOVERY
```

Đúng:

```text
fault event
 -> reset/error
 -> Bootloader reads persistent state
 -> resume / reconstruct / install / rollback / recovery
```

Inject reset/power-loss tại:

- FULL staging;
- DELTA receive;
- Delta manifest body/commit;
- commit Delta apply-started marker trong W25Q32;
- reconstruct;
- metadata;
- backup;
- commit `INSTALL_IN_PROGRESS`;
- Internal Flash program;
- Trial;
- commit `ROLLBACK_IN_PROGRESS`;
- rollback restore;
- final metadata commit.

Terminal outcomes hợp lệ:

```text
A. Old App valid + IDLE
B. New App confirmed + IDLE
C. Recovery vì không còn image an toàn
```
