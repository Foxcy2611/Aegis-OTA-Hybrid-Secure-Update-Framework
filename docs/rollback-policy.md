# Rollback Policy

> **Phạm vi (Scope):** Chốt correction rollback: success phải về App cũ + IDLE.

> **Tác dụng:** Quy định điều kiện, thứ tự và kết quả bắt buộc của một lần rollback.


```mermaid
flowchart TB
    F["Trial fail / limit"]
    B{"BACKUP valid?"}
    S["Commit ROLLBACK_IN_PROGRESS"]
    R["Restore BACKUP"]
    V{"Restored App valid?"}
    M["Commit old active version + IDLE"]
    J["Boot old App"]
    REC["Recovery"]

    F --> B
    B -->|"YES"| S --> R --> V
    V -->|"YES"| M --> J
    B -->|"NO"| REC
    R -. "Flash fail" .-> REC
    V -->|"NO"| REC
    M -. "metadata fail" .-> REC
```

Reset giữa rollback: nếu state vẫn `ROLLBACK_IN_PROGRESS` và backup valid thì tiếp tục rollback.
