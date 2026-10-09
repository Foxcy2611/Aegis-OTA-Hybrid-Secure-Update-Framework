#ifndef AEGIS_OTA_PROTOCOL_H
#define AEGIS_OTA_PROTOCOL_H

/* Quy ước byte của bản tin OTA logic nằm bên trong secure frame. */

#include "crypto_config.h"

#include <stdint.h>

#define OTA_PROTOCOL_FORMAT_VERSION             1U
#define OTA_DATA_CHUNK_MAX_SIZE                 224U
#define OTA_TRANSFER_ID_SIZE                    8U
#define OTA_ARTIFACT_ID_SIZE                    8U
#define OTA_OPERATION_ID_SIZE                   8U

typedef enum{
    OTA_COMMAND_BEGIN = 0x01U,
    OTA_COMMAND_DATA = 0x02U,
    OTA_COMMAND_END = 0x03U,
    OTA_COMMAND_ABORT = 0x04U,
    OTA_COMMAND_STATUS = 0x05U,
    OTA_COMMAND_BOOT = 0x06U,
    OTA_RESPONSE_ACK = 0x80U,
    OTA_RESPONSE_NACK = 0x81U,
    OTA_RESPONSE_STATUS = 0x82U
} OtaProtocol_MessageType_t;

typedef enum{
    OTA_ARTIFACT_FULL = 1U,
    OTA_ARTIFACT_DELTA = 2U
} OtaProtocol_ArtifactType_t;

typedef enum{
    OTA_SECTION_MANIFEST = 0U,
    OTA_SECTION_DATA = 1U
} OtaProtocol_Section_t;

typedef enum{
    OTA_RECEIVE_EMPTY = 0U,
    OTA_RECEIVE_IN_PROGRESS,
    OTA_RECEIVE_COMPLETE_UNVERIFIED,
    OTA_RECEIVE_VALID,
    OTA_RECEIVE_APPLY_IN_PROGRESS,
    OTA_RECEIVE_INVALID
} OtaProtocol_ReceiveState_t;

typedef enum{
    OTA_NACK_NONE = 0U,
    OTA_NACK_BAD_FORMAT,
    OTA_NACK_BAD_SESSION,
    OTA_NACK_REPLAY,
    OTA_NACK_BAD_TRANSFER,
    OTA_NACK_BAD_ARTIFACT,
    OTA_NACK_BAD_STATE,
    OTA_NACK_OUT_OF_RANGE,
    OTA_NACK_UNEXPECTED_OFFSET,
    OTA_NACK_CONFLICTING_DUPLICATE,
    OTA_NACK_STORAGE_ERROR,
    OTA_NACK_INCOMPLETE,
    OTA_NACK_AUTH_FAILED,
    OTA_NACK_POLICY_REJECTED,
    OTA_NACK_TIMEOUT
} OtaProtocol_NackReason_t;

/* BEGIN = transfer + artifact + operation + type + manifest/data/chunk size. */
#define OTA_BEGIN_FIXED_PAYLOAD_SIZE            33U
#define OTA_BEGIN_TRANSFER_ID_OFFSET             0U
#define OTA_BEGIN_ARTIFACT_ID_OFFSET             8U
#define OTA_BEGIN_OPERATION_ID_OFFSET           16U
#define OTA_BEGIN_ARTIFACT_TYPE_OFFSET          24U
#define OTA_BEGIN_MANIFEST_SIZE_OFFSET          25U
#define OTA_BEGIN_DATA_SIZE_OFFSET              27U
#define OTA_BEGIN_CHUNK_SIZE_OFFSET             31U

/* DATA = transfer + section + offset + length + bytes. */
#define OTA_DATA_ENVELOPE_SIZE                  15U
#define OTA_DATA_TRANSFER_ID_OFFSET              0U
#define OTA_DATA_SECTION_OFFSET                  8U
#define OTA_DATA_OFFSET_OFFSET                   9U
#define OTA_DATA_LENGTH_OFFSET                  13U
#define OTA_DATA_BYTES_OFFSET                   15U

#define OTA_END_PAYLOAD_SIZE                     8U
#define OTA_ABORT_PAYLOAD_SIZE                   8U
#define OTA_STATUS_QUERY_PAYLOAD_SIZE            8U
#define OTA_BOOT_PAYLOAD_SIZE                    8U

#define OTA_ACK_FIXED_PAYLOAD_SIZE              14U
#define OTA_ACK_REQUEST_TYPE_OFFSET              0U
#define OTA_ACK_STATUS_REASON_OFFSET             1U
#define OTA_ACK_TRANSFER_ID_OFFSET               2U
#define OTA_ACK_NEXT_OFFSET_OFFSET              10U

#define OTA_STATUS_RESPONSE_PAYLOAD_SIZE        32U
#define OTA_STATUS_STATE_OFFSET                  0U
#define OTA_STATUS_ARTIFACT_TYPE_OFFSET          1U
#define OTA_STATUS_TRANSFER_ID_OFFSET            2U
#define OTA_STATUS_ARTIFACT_ID_OFFSET           10U
#define OTA_STATUS_DATA_SIZE_OFFSET             18U
#define OTA_STATUS_CHUNK_SIZE_OFFSET            22U
#define OTA_STATUS_NEXT_OFFSET_OFFSET           24U
#define OTA_STATUS_FLAGS_OFFSET                 28U

#define OTA_STATUS_FLAG_HEADER_COMMITTED        UINT32_C(0x00000001)
#define OTA_STATUS_FLAG_ARTIFACT_VALID          UINT32_C(0x00000002)
#define OTA_STATUS_FLAG_APPLY_STARTED           UINT32_C(0x00000004)
#define OTA_STATUS_FLAG_BACKUP_VALID            UINT32_C(0x00000008)
#define OTA_STATUS_FLAG_STAGING_READY           UINT32_C(0x00000010)
#define OTA_STATUS_FLAG_INSTALLED               UINT32_C(0x00000020)
#define OTA_STATUS_FLAG_CONFIRMED               UINT32_C(0x00000040)

_Static_assert((OTA_DATA_ENVELOPE_SIZE + OTA_DATA_CHUNK_MAX_SIZE) <=
                   CRYPTO_FRAME_MAX_PAYLOAD_SIZE,
               "OTA DATA phải nằm gọn trong secure frame");
_Static_assert((OTA_BEGIN_CHUNK_SIZE_OFFSET + 2U) ==
                   OTA_BEGIN_FIXED_PAYLOAD_SIZE,
               "Sai layout BEGIN");
_Static_assert((OTA_ACK_NEXT_OFFSET_OFFSET + 4U) ==
                   OTA_ACK_FIXED_PAYLOAD_SIZE,
               "Sai layout ACK/NACK");
_Static_assert((OTA_STATUS_FLAGS_OFFSET + 4U) ==
                   OTA_STATUS_RESPONSE_PAYLOAD_SIZE,
               "Sai layout STATUS_RESPONSE");

#endif /* AEGIS_OTA_PROTOCOL_H */
