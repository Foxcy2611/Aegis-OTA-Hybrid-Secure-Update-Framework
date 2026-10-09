#ifndef AEGIS_CRYPTO_CONFIG_H
#define AEGIS_CRYPTO_CONFIG_H

#include "firmware_format.h"

#include <stddef.h>
#include <stdint.h>

/*
 * Quy ước mật mã lúc chạy dùng chung giữa Python/ESP32 và firmware STM32.
 * File này chỉ khóa kích thước, offset và cách biểu diễn byte; không chứa khóa,
 * không thực hiện crypto và không định nghĩa lại Firmware_SignedHeader_t.
 */

/* ==================== Kích thước thuật toán ==================== */

#define CRYPTO_TRANSPORT_KEY_SIZE              32U
/* Khóa đối xứng dùng bởi ChaCha20-Poly1305 giữa ESP32 và Application. */

#define CRYPTO_AEAD_NONCE_SIZE                 12U
/* IETF nonce dùng với crypto_aead_init_ietf(). */

#define CRYPTO_AEAD_TAG_SIZE                   16U
/* Poly1305 authentication tag của mỗi secure frame. */

#define CRYPTO_ED25519_SEED_SIZE               32U
/* Seed ngẫu nhiên chỉ tồn tại ở Release/CI. */

#define CRYPTO_ED25519_SECRET_KEY_SIZE         64U
/* Secret key Monocypher đã mở rộng từ seed; không đưa xuống STM32. */

#define CRYPTO_ED25519_PUBLIC_KEY_SIZE         32U
/* Khóa công khai xác thực được cấp cho Application và Bootloader. */

#define CRYPTO_SHA512_HASH_SIZE                64U
/* SHA-512 digest/prehash dùng làm input cho Ed25519ph. */

#define CRYPTO_ED25519_SIGNATURE_SIZE          FIRMWARE_SIGNATURE_SIZE
/* Chữ ký firmware dùng lại đúng quy ước từ firmware_format.h. */

/* ==================== Quy ước byte của secure frame ==================== */

/*
 * ChaCha20-Poly1305 chỉ thuộc ESP32 và STM32 Application.
 * Bootloader không parse hoặc decrypt secure frame.
 */

#define CRYPTO_SESSION_ID_SIZE                 8U
#define CRYPTO_FRAME_SYNC_SIZE                 2U
#define CRYPTO_FRAME_COUNTER_SIZE              4U
#define CRYPTO_FRAME_TYPE_SIZE                 1U
#define CRYPTO_FRAME_SEQUENCE_SIZE             2U
#define CRYPTO_FRAME_LENGTH_SIZE               2U
#define CRYPTO_FRAME_AAD_SIZE                  5U
#define CRYPTO_FRAME_CRC32_SIZE                4U
#define CRYPTO_FRAME_MAX_PAYLOAD_SIZE          256U

#define CRYPTO_FRAME_SYNC_BYTE_0               UINT8_C(0xA5)
#define CRYPTO_FRAME_SYNC_BYTE_1               UINT8_C(0x5A)

#define CRYPTO_FRAME_SYNC_OFFSET               0U
#define CRYPTO_FRAME_SESSION_ID_OFFSET         2U
#define CRYPTO_FRAME_COUNTER_OFFSET            10U
#define CRYPTO_FRAME_TYPE_OFFSET               14U
#define CRYPTO_FRAME_SEQUENCE_OFFSET           15U
#define CRYPTO_FRAME_LENGTH_OFFSET             17U
#define CRYPTO_FRAME_CIPHERTEXT_OFFSET         19U

/*
 * TAG, CRC32 và raw-size phụ thuộc LEN. Mỗi macro chỉ đánh giá payload_length
 * đúng một lần; caller phải kiểm payload_length <= 256 trước khi dùng.
 */
#define CRYPTO_FRAME_TAG_OFFSET(payload_length)                         \
    (CRYPTO_FRAME_CIPHERTEXT_OFFSET + (uint32_t)(payload_length))

#define CRYPTO_FRAME_CRC32_OFFSET(payload_length)                       \
    (CRYPTO_FRAME_TAG_OFFSET(payload_length) + CRYPTO_AEAD_TAG_SIZE)

#define CRYPTO_FRAME_RAW_SIZE(payload_length)                           \
    (CRYPTO_FRAME_CRC32_OFFSET(payload_length) + CRYPTO_FRAME_CRC32_SIZE)

#define CRYPTO_FRAME_CRC_COVERAGE_OFFSET       CRYPTO_FRAME_SESSION_ID_OFFSET

#define CRYPTO_FRAME_CRC_COVERAGE_SIZE(payload_length)                  \
    (CRYPTO_FRAME_CRC32_OFFSET(payload_length) -                        \
     CRYPTO_FRAME_CRC_COVERAGE_OFFSET)

#define CRYPTO_FRAME_FIXED_OVERHEAD_SIZE                                \
    CRYPTO_FRAME_RAW_SIZE(0U)

#define CRYPTO_FRAME_MAX_RAW_SIZE                                       \
    CRYPTO_FRAME_RAW_SIZE(CRYPTO_FRAME_MAX_PAYLOAD_SIZE)

/* Nonce bytes = SESSION_ID[8] || FRAME_COUNTER_LE[4]. */
#define CRYPTO_NONCE_SESSION_ID_OFFSET         0U
#define CRYPTO_NONCE_FRAME_COUNTER_OFFSET      8U

/* AAD bytes = TYPE[1] || SEQ_LE[2] || LEN_LE[2]. */
#define CRYPTO_AAD_TYPE_OFFSET                 0U
#define CRYPTO_AAD_SEQUENCE_OFFSET             1U
#define CRYPTO_AAD_LENGTH_OFFSET               3U

/* Hai miền counter ngăn hai chiều tạo cùng nonce trong một session. */
#define CRYPTO_HOST_TO_DEVICE_COUNTER_MIN      UINT32_C(0x00000000)
#define CRYPTO_HOST_TO_DEVICE_COUNTER_MAX      UINT32_C(0x7FFFFFFF)
#define CRYPTO_DEVICE_TO_HOST_COUNTER_MIN      UINT32_C(0x80000000)
#define CRYPTO_DEVICE_TO_HOST_COUNTER_MAX      UINT32_C(0xFFFFFFFF)

/* Counter không được wrap; resend cùng SEQ phải dùng frame counter mới. */

/*
 * Hai struct dưới đây chứa byte đã serialize, không chứa integer native-endian.
 * Vì mọi member là byte nên có thể truyền trực tiếp cho Monocypher.
 */
typedef struct{
    uint8_t session_id[CRYPTO_SESSION_ID_SIZE];
    /* ID phiên OTA; phải mới trước khi counter của cùng transport key lặp lại. */

    uint8_t frame_counter_le[CRYPTO_FRAME_COUNTER_SIZE];
    /* Frame counter đúng 4 byte little-endian. */
} Crypto_Nonce_t;

typedef struct{
    uint8_t type;
    /* Logical command/frame type; được xác thực nhưng không mã hóa. */

    uint8_t sequence_le[CRYPTO_FRAME_SEQUENCE_SIZE];
    /* Ghép request với ACK/NACK; resend có thể giữ SEQ. */

    uint8_t length_le[CRYPTO_FRAME_LENGTH_SIZE];
    /* Số byte ciphertext đúng 2 byte little-endian. */
} Crypto_FrameAad_t;

/* ==================== Kiểm tra quy ước lúc biên dịch ==================== */

_Static_assert(sizeof(uint8_t) == 1U, "uint8_t phải có đúng 1 byte");
_Static_assert(sizeof(uint32_t) == 4U, "uint32_t phải có đúng 4 byte");

_Static_assert(CRYPTO_TRANSPORT_KEY_SIZE == 32U,
               "ChaCha20-Poly1305 key phải giữ đúng 32 byte");
_Static_assert(CRYPTO_AEAD_NONCE_SIZE == 12U,
               "IETF nonce phải giữ đúng 12 byte");
_Static_assert(CRYPTO_AEAD_TAG_SIZE == 16U,
               "Poly1305 tag phải giữ đúng 16 byte");
_Static_assert(CRYPTO_ED25519_SEED_SIZE == 32U,
               "Ed25519 seed phải giữ đúng 32 byte");
_Static_assert(CRYPTO_ED25519_SECRET_KEY_SIZE == 64U,
               "Ed25519 secret key mở rộng phải giữ đúng 64 byte");
_Static_assert(CRYPTO_ED25519_PUBLIC_KEY_SIZE == 32U,
               "Ed25519 public key phải giữ đúng 32 byte");
_Static_assert(CRYPTO_SHA512_HASH_SIZE == 64U,
               "SHA-512 digest phải giữ đúng 64 byte");
_Static_assert(CRYPTO_ED25519_SIGNATURE_SIZE == 64U,
               "Ed25519ph signature phải giữ đúng 64 byte");
_Static_assert(FIRMWARE_SIGNED_HEADER_SIZE == 32U,
               "SignedHeader dùng cho crypto phải giữ đúng 32 byte");

_Static_assert(offsetof(Crypto_Nonce_t, session_id) ==
                   CRYPTO_NONCE_SESSION_ID_OFFSET,
               "Sai vị trí session ID trong nonce");
_Static_assert(offsetof(Crypto_Nonce_t, frame_counter_le) ==
                   CRYPTO_NONCE_FRAME_COUNTER_OFFSET,
               "Sai vị trí frame counter trong nonce");
_Static_assert(sizeof(Crypto_Nonce_t) == CRYPTO_AEAD_NONCE_SIZE,
               "Cách biểu diễn nonce phải giữ đúng 12 byte");

_Static_assert(offsetof(Crypto_FrameAad_t, type) == CRYPTO_AAD_TYPE_OFFSET,
               "Sai vị trí type trong AAD");
_Static_assert(offsetof(Crypto_FrameAad_t, sequence_le) ==
                   CRYPTO_AAD_SEQUENCE_OFFSET,
               "Sai vị trí sequence trong AAD");
_Static_assert(offsetof(Crypto_FrameAad_t, length_le) ==
                   CRYPTO_AAD_LENGTH_OFFSET,
               "Sai vị trí length trong AAD");
_Static_assert(sizeof(Crypto_FrameAad_t) == CRYPTO_FRAME_AAD_SIZE,
               "Cách biểu diễn AAD phải giữ đúng 5 byte");

_Static_assert((CRYPTO_FRAME_SYNC_OFFSET + CRYPTO_FRAME_SYNC_SIZE) ==
                   CRYPTO_FRAME_SESSION_ID_OFFSET,
               "Sai layout SYNC trong secure frame");
_Static_assert((CRYPTO_FRAME_SESSION_ID_OFFSET + CRYPTO_SESSION_ID_SIZE) ==
                   CRYPTO_FRAME_COUNTER_OFFSET,
               "Sai layout session ID trong secure frame");
_Static_assert((CRYPTO_FRAME_COUNTER_OFFSET + CRYPTO_FRAME_COUNTER_SIZE) ==
                   CRYPTO_FRAME_TYPE_OFFSET,
               "Sai layout frame counter trong secure frame");
_Static_assert((CRYPTO_FRAME_TYPE_OFFSET + CRYPTO_FRAME_TYPE_SIZE) ==
                   CRYPTO_FRAME_SEQUENCE_OFFSET,
               "Sai layout type trong secure frame");
_Static_assert((CRYPTO_FRAME_SEQUENCE_OFFSET + CRYPTO_FRAME_SEQUENCE_SIZE) ==
                   CRYPTO_FRAME_LENGTH_OFFSET,
               "Sai layout sequence trong secure frame");
_Static_assert((CRYPTO_FRAME_LENGTH_OFFSET + CRYPTO_FRAME_LENGTH_SIZE) ==
                   CRYPTO_FRAME_CIPHERTEXT_OFFSET,
               "Sai layout length trong secure frame");

_Static_assert(CRYPTO_FRAME_TAG_OFFSET(CRYPTO_FRAME_MAX_PAYLOAD_SIZE) == 275U,
               "Sai offset TAG của raw frame lớn nhất");
_Static_assert(CRYPTO_FRAME_CRC32_OFFSET(CRYPTO_FRAME_MAX_PAYLOAD_SIZE) == 291U,
               "Sai offset CRC32 của raw frame lớn nhất");
_Static_assert(CRYPTO_FRAME_FIXED_OVERHEAD_SIZE == 39U,
               "Phần phụ cố định của secure frame phải giữ đúng 39 byte");
_Static_assert(CRYPTO_FRAME_MAX_RAW_SIZE == 295U,
               "Raw secure frame lớn nhất phải giữ đúng 295 byte");
_Static_assert(CRYPTO_FRAME_CRC_COVERAGE_SIZE(
                   CRYPTO_FRAME_MAX_PAYLOAD_SIZE
               ) == 289U,
               "CRC coverage của raw frame lớn nhất phải giữ đúng 289 byte");

_Static_assert(CRYPTO_HOST_TO_DEVICE_COUNTER_MAX <
                   CRYPTO_DEVICE_TO_HOST_COUNTER_MIN,
               "Hai miền frame counter không được chồng lấn");
_Static_assert(CRYPTO_FRAME_MAX_PAYLOAD_SIZE <= UINT16_MAX,
               "Payload phải biểu diễn được bằng LEN 2 byte");

#endif /* AEGIS_CRYPTO_CONFIG_H */
