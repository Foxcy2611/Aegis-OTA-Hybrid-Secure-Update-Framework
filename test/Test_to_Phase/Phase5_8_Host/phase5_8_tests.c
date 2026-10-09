#include "boot_state_machine.h"
#include "crc32.h"
#include "delta_apply.h"
#include "metadata_record.h"
#include "ota_transport.h"
#include "secure_frame.h"

#include "monocypher.h"

#include <stdio.h>
#include <string.h>

static unsigned checks;
static unsigned failures;

#define CHECK(condition) do{                                             \
    ++checks;                                                            \
    if(!(condition)){                                                    \
        ++failures;                                                      \
        printf("FAIL line %d: %s\n", __LINE__, #condition);             \
    }                                                                    \
}while(0)

typedef struct{
    const uint8_t *bytes;
    uint32_t size;
} Reader_t;

typedef struct{
    uint8_t bytes[64];
    uint32_t size;
} Writer_t;

typedef struct{
    uint8_t last_session[CRYPTO_SESSION_ID_SIZE];
    unsigned accepted_count;
} SessionAcceptor_t;

static bool AcceptSessionOnce(
    void *context,
    const uint8_t session_id[CRYPTO_SESSION_ID_SIZE]
){
    SessionAcceptor_t *acceptor = context;

    if((acceptor->accepted_count > 0U) &&
       (memcmp(acceptor->last_session, session_id,
               CRYPTO_SESSION_ID_SIZE) == 0)){
        return false;
    }
    memcpy(acceptor->last_session, session_id, CRYPTO_SESSION_ID_SIZE);
    ++acceptor->accepted_count;
    return true;
}

static bool ReadBytes(
    void *context,
    uint32_t offset,
    uint8_t *buffer,
    uint32_t length
){
    const Reader_t *reader = context;
    if((offset > reader->size) || (length > reader->size - offset)){
        return false;
    }
    memcpy(buffer, &reader->bytes[offset], length);
    return true;
}

static bool WriteBytes(
    void *context,
    uint32_t offset,
    const uint8_t *buffer,
    uint32_t length
){
    Writer_t *writer = context;
    if((offset > sizeof(writer->bytes)) ||
       (length > sizeof(writer->bytes) - offset)){
        return false;
    }
    memcpy(&writer->bytes[offset], buffer, length);
    if(writer->size < offset + length){
        writer->size = offset + length;
    }
    return true;
}

static void WriteU16(uint8_t *output, uint16_t value){
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8U);
}

static void WriteU32(uint8_t *output, uint32_t value){
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8U);
    output[2] = (uint8_t)(value >> 16U);
    output[3] = (uint8_t)(value >> 24U);
}

static size_t BuildHostFrame(
    const uint8_t key[32],
    const uint8_t session[8],
    uint32_t counter,
    uint8_t type,
    uint16_t sequence,
    const uint8_t *payload,
    uint16_t length,
    uint8_t *raw
){
    crypto_aead_ctx crypto;
    uint8_t nonce[12];
    uint8_t aad[5];

    raw[0] = CRYPTO_FRAME_SYNC_BYTE_0;
    raw[1] = CRYPTO_FRAME_SYNC_BYTE_1;
    memcpy(&raw[2], session, 8U);
    WriteU32(&raw[10], counter);
    raw[14] = type;
    WriteU16(&raw[15], sequence);
    WriteU16(&raw[17], length);
    memcpy(nonce, session, 8U);
    WriteU32(&nonce[8], counter);
    aad[0] = type;
    WriteU16(&aad[1], sequence);
    WriteU16(&aad[3], length);
    crypto_aead_init_ietf(&crypto, key, nonce);
    crypto_aead_write(&crypto, &raw[19], &raw[19 + length],
                      aad, sizeof(aad), payload, length);
    WriteU32(&raw[35 + length], Crc32_Compute(&raw[2], 33U + length));
    return 39U + length;
}

static void TestTransport(void){
    OtaTransport_Begin_t input = {
        .transfer_id = 0x0102030405060708ULL,
        .operation_id = 0x1112131415161718ULL,
        .artifact_type = OTA_ARTIFACT_FULL,
        .manifest_size = 404U,
        .data_size = 1234U,
        .chunk_size = 224U
    };
    OtaTransport_Begin_t output;
    uint8_t payload[256];
    uint16_t length;
    const uint8_t data[3] = {1U, 2U, 3U};
    OtaTransport_DataView_t view;
    OtaTransport_StatusResponse_t status_input;
    OtaTransport_StatusResponse_t status_output;

    memset(input.artifact_id, 0xA5, sizeof(input.artifact_id));
    CHECK(OtaTransport_BuildBegin(&input, payload) == OTA_TRANSPORT_OK);
    CHECK(OtaTransport_ParseBegin(payload, 33U, &output) == OTA_TRANSPORT_OK);
    CHECK(output.data_size == input.data_size);
    CHECK(output.transfer_id == input.transfer_id);
    CHECK(OtaTransport_ParseBegin(payload, 32U, &output) ==
          OTA_TRANSPORT_BAD_LENGTH);
    CHECK(OtaTransport_BuildData(input.transfer_id, OTA_SECTION_DATA, 448U,
                                 data, sizeof(data), payload, sizeof(payload),
                                 &length) == OTA_TRANSPORT_OK);
    CHECK(OtaTransport_ParseData(payload, length, &view) == OTA_TRANSPORT_OK);
    CHECK(view.offset == 448U && view.data_length == 3U);
    payload[OTA_DATA_LENGTH_OFFSET] = 4U;
    CHECK(OtaTransport_ParseData(payload, length, &view) ==
          OTA_TRANSPORT_BAD_LENGTH);

    memset(&status_input, 0, sizeof(status_input));
    status_input.receive_state = OTA_RECEIVE_VALID;
    status_input.artifact_type = OTA_ARTIFACT_FULL;
    status_input.transfer_id = input.transfer_id;
    memset(status_input.artifact_id, 0x5A, sizeof(status_input.artifact_id));
    status_input.data_size = input.data_size;
    status_input.chunk_size = input.chunk_size;
    status_input.next_missing_offset = input.data_size;
    status_input.flags = OTA_STATUS_FLAG_HEADER_COMMITTED |
        OTA_STATUS_FLAG_ARTIFACT_VALID |
        OTA_STATUS_FLAG_STAGING_READY |
        OTA_STATUS_FLAG_INSTALLED |
        OTA_STATUS_FLAG_CONFIRMED;
    CHECK(OtaTransport_BuildStatusResponse(&status_input, payload) ==
          OTA_TRANSPORT_OK);
    CHECK(OtaTransport_ParseStatusResponse(
              payload, OTA_STATUS_RESPONSE_PAYLOAD_SIZE, &status_output
          ) == OTA_TRANSPORT_OK);
    CHECK(status_output.transfer_id == status_input.transfer_id &&
          status_output.flags == status_input.flags);
    payload[OTA_STATUS_ARTIFACT_TYPE_OFFSET] = 0x7FU;
    CHECK(OtaTransport_ParseStatusResponse(
              payload, OTA_STATUS_RESPONSE_PAYLOAD_SIZE, &status_output
          ) == OTA_TRANSPORT_BAD_VALUE);

    status_input.flags = OTA_STATUS_FLAG_CONFIRMED;
    CHECK(OtaTransport_BuildStatusResponse(&status_input, payload) ==
          OTA_TRANSPORT_BAD_VALUE);
    status_input.flags = OTA_STATUS_FLAG_INSTALLED;
    CHECK(OtaTransport_BuildStatusResponse(&status_input, payload) ==
          OTA_TRANSPORT_OK);
    status_input.artifact_type = OTA_ARTIFACT_FULL;
    status_input.flags = OTA_STATUS_FLAG_APPLY_STARTED |
        OTA_STATUS_FLAG_ARTIFACT_VALID;
    CHECK(OtaTransport_BuildStatusResponse(&status_input, payload) ==
          OTA_TRANSPORT_BAD_VALUE);
}

static void TestSecureFrame(void){
    SecureFrame_Context_t context;
    SecureFrame_Message_t message;
    uint8_t key[32];
    uint8_t session[8];
    uint8_t second_session[8];
    uint8_t raw[128];
    const uint8_t payload[4] = {9U, 8U, 7U, 6U};
    SessionAcceptor_t acceptor = {{0U}, 0U};
    size_t length;

    memset(key, 0x33, sizeof(key));
    memset(session, 0x44, sizeof(session));
    CHECK(SecureFrame_Init(&context, key) == SECURE_FRAME_OK);
    SecureFrame_SetSessionAcceptor(&context, AcceptSessionOnce, &acceptor);
    length = BuildHostFrame(key, session, 7U, OTA_COMMAND_BEGIN, 2U,
                            payload, sizeof(payload), raw);
    CHECK(SecureFrame_Decode(&context, raw, length, &message) ==
          SECURE_FRAME_OK);
    CHECK(message.payload_length == sizeof(payload));
    CHECK(memcmp(message.payload, payload, sizeof(payload)) == 0);
    CHECK(SecureFrame_Decode(&context, raw, length, &message) ==
          SECURE_FRAME_REPLAY);
    SecureFrame_ResetSession(&context);
    CHECK(SecureFrame_Decode(&context, raw, length, &message) ==
          SECURE_FRAME_BAD_SESSION);
    CHECK(acceptor.accepted_count == 1U);

    memset(second_session, 0x55, sizeof(second_session));
    length = BuildHostFrame(
        key, second_session, 8U, OTA_COMMAND_STATUS, 3U,
        payload, sizeof(payload), raw
    );
    CHECK(SecureFrame_Decode(&context, raw, length, &message) ==
          SECURE_FRAME_OK);
    CHECK(acceptor.accepted_count == 2U);

    raw[20] ^= 1U;
    CHECK(SecureFrame_Decode(&context, raw, length, &message) ==
          SECURE_FRAME_BAD_CRC);
}

static void TestDeltaApply(void){
    static const uint8_t base[] = "abcdef";
    static const uint8_t target[] = "abcXYZdef";
    const uint8_t patch[] = {
        DELTA_PATCH_OPCODE_COPY, 0U, 0U, 0U, 0U, 3U, 0U,
        DELTA_PATCH_OPCODE_ADD, 3U, 0U, 'X', 'Y', 'Z',
        DELTA_PATCH_OPCODE_COPY, 3U, 0U, 0U, 0U, 3U, 0U,
        DELTA_PATCH_OPCODE_END
    };
    Reader_t base_reader = {base, sizeof(base) - 1U};
    Reader_t patch_reader = {patch, sizeof(patch)};
    Writer_t writer = {{0U}, 0U};
    Delta_ImmutableBody_t body;

    memset(&body, 0, sizeof(body));
    body.algorithm_id = DELTA_ALGORITHM_COPY_ADD_V1;
    body.base_size = base_reader.size;
    body.target_header.image_size = sizeof(target) - 1U;
    body.patch_size = sizeof(patch);
    CHECK(DeltaApply_Run(&body, ReadBytes, &base_reader,
                         ReadBytes, &patch_reader,
                         WriteBytes, &writer) == DELTA_APPLY_OK);
    CHECK(writer.size == sizeof(target) - 1U);
    CHECK(memcmp(writer.bytes, target, writer.size) == 0);
}

static void TestBootDecision(void){
    BootState_Input_t input;

    memset(&input, 0, sizeof(input));
    input.metadata_status = BOOT_INPUT_METADATA_VALID;
    input.operation_state = METADATA_OPERATION_IDLE;
    input.app_valid = true;
    CHECK(BootState_Decide(&input) == BOOT_ACTION_START_APP);
    input.metadata_status = BOOT_INPUT_METADATA_FIRST_BOOT;
    CHECK(BootState_Decide(&input) == BOOT_ACTION_RECOVERY);
    input.metadata_status = BOOT_INPUT_METADATA_VALID;
    input.update_requested = true;
    input.staging_valid = true;
    input.backup_valid = true;
    CHECK(BootState_Decide(&input) == BOOT_ACTION_INSTALL_UPDATE);
    input.update_requested = false;
    input.app_valid = false;
    CHECK(BootState_Decide(&input) == BOOT_ACTION_ROLLBACK);
}

static void TestStableMetadataInvariant(void){
    Metadata_Record_t record;

    CHECK(MetadataRecord_InitializeDefault(&record));
    record.active_header.image_size = 1024U;
    record.active_header.image_version = 1U;
    CHECK(MetadataRecord_RefreshCrc32(&record));
    CHECK(MetadataRecord_IsStructurallyValid(&record));

    record.operation_id = UINT64_C(1);
    CHECK(MetadataRecord_RefreshCrc32(&record));
    CHECK(!MetadataRecord_IsStructurallyValid(&record));

    record.trial_boot_flag = METADATA_TRIAL_BOOT_TRUE;
    CHECK(MetadataRecord_RefreshCrc32(&record));
    CHECK(MetadataRecord_IsStructurallyValid(&record));
}

int main(void){
    TestTransport();
    TestSecureFrame();
    TestDeltaApply();
    TestBootDecision();
    TestStableMetadataInvariant();
    printf("Phase5-8 host: %u checks, %u failures\n", checks, failures);
    return failures == 0U ? 0 : 1;
}
