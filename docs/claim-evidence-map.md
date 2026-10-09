# Claim / Evidence Map

> **Phạm vi (Scope):** Map các claim README/CV sang evidence.

> **Tác dụng:** Giúp mỗi tuyên bố của dự án truy ngược được tới test và bằng chứng tương ứng.


| Claim | Evidence cần có |
|---|---|
| Secure transport | crypto vectors + tamper/replay negative |
| Signed firmware | signer fixture + STM32 verify |
| FULL OTA | E2E log + target hash/version |
| Delta OTA | host self-apply + STM32 reconstruct + target verify |
| Resume | disconnect/reset + resumed offset |
| Rollback | trial fail + restored App hash + IDLE metadata |
| Power-loss safe | fault matrix |
| AS32 P2P | config/profile + bidirectional link logs |

Không claim feature chưa có evidence.
