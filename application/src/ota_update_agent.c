#include "ota_update_agent.h"

#include "active_firmware.h"
#include "artifact_storage.h"
#include "boot_mailbox_io.h"
#include "delta_apply.h"
#include "delta_auth.h"
#include "delta_storage.h"
#include "ext_flash.h"
#include "firmware_auth.h"
#include "memory_map.h"
#include "transport_session_store.h"
#include "watchdog.h"

#include <string.h>

typedef struct{
    uint8_t bytes[OTA_DATA_CHUNK_MAX_SIZE];
    uint16_t used;
    uint32_t output_offset;
} OtaAgent_DeltaWriter_t;

static OtaAgent_Status_t OtaAgent_BuildReply(
    OtaAgent_Context_t *context,
    const SecureFrame_Message_t *request,
    OtaProtocol_NackReason_t reason,
    uint32_t next_offset,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length
);

static bool OtaAgent_StagingReader(
    void *context,
    uint32_t offset,
    uint8_t *buffer,
    uint32_t length
){
    (void)context;
    Watchdog_Feed();
    return ArtifactStorage_StagingRead(offset, buffer, length) ==
        ARTIFACT_STORAGE_OK;
}

static bool OtaAgent_DeltaReader(
    void *context,
    uint32_t offset,
    uint8_t *buffer,
    uint32_t length
){
    (void)context;
    Watchdog_Feed();
    return DeltaStorage_Read(offset, buffer, length) == DELTA_STORAGE_OK;
}

static bool OtaAgent_InternalReader(
    void *context,
    uint32_t offset,
    uint8_t *buffer,
    uint32_t length
){
    const uint32_t size = *(const uint32_t *)context;

    if((offset > size) || (length > (size - offset))){
        return false;
    }
    Watchdog_Feed();
    memcpy(buffer,
           (const void *)(uintptr_t)(APPLICATION_BASE_ADDRESS + offset),
           length);
    return true;
}

static bool OtaAgent_FlushDeltaWriter(
    OtaAgent_DeltaWriter_t *writer,
    bool final
){
    if(writer->used == 0U){
        return true;
    }
    if(!final && (writer->used != OTA_DATA_CHUNK_MAX_SIZE)){
        return true;
    }
    Watchdog_Feed();
    const ArtifactStorage_Status_t status =
        ArtifactStorage_StagingWriteFragment(
            writer->output_offset,
            writer->bytes,
            writer->used
        );
    if((status != ARTIFACT_STORAGE_OK) &&
       (status != ARTIFACT_STORAGE_DUPLICATE)){
        return false;
    }
    writer->output_offset += writer->used;
    writer->used = 0U;
    return true;
}

static bool OtaAgent_StagingWriter(
    void *context,
    uint32_t offset,
    const uint8_t *buffer,
    uint32_t length
){
    OtaAgent_DeltaWriter_t *writer = context;
    uint32_t consumed = 0U;

    if((writer == NULL) || (buffer == NULL) ||
       (offset != (writer->output_offset + writer->used))){
        return false;
    }
    while(consumed < length){
        const uint32_t capacity = OTA_DATA_CHUNK_MAX_SIZE - writer->used;
        const uint32_t remaining = length - consumed;
        const uint32_t chunk = remaining < capacity ? remaining : capacity;

        memcpy(&writer->bytes[writer->used], &buffer[consumed], chunk);
        writer->used = (uint16_t)(writer->used + chunk);
        consumed += chunk;
        if((writer->used == OTA_DATA_CHUNK_MAX_SIZE) &&
           !OtaAgent_FlushDeltaWriter(writer, false)){
            return false;
        }
    }
    return true;
}

static uint32_t OtaAgent_NextMissingFull(const Staging_Manifest_t *manifest){
    for(uint16_t index = 0U; index < manifest->fragment_count; ++index){
        if((manifest->progress_bitmap[index >> 3U] &
            (uint8_t)(1U << (index & 7U))) != 0U){
            return (uint32_t)index * manifest->chunk_size;
        }
    }
    return manifest->data_size;
}

static uint32_t OtaAgent_NextMissingDelta(const Delta_Manifest_t *manifest){
    for(uint16_t index = 0U; index < manifest->fragment_count; ++index){
        if((manifest->progress_bitmap[index >> 3U] &
            (uint8_t)(1U << (index & 7U))) != 0U){
            return (uint32_t)index * manifest->chunk_size;
        }
    }
    return manifest->immutable_body.patch_size;
}

/*
 * Metadata active là nguồn xác nhận target đã install/confirm thật sự.
 * Trong Trial, operation_id còn phải khớp để không nhận nhầm transaction.
 */
static void OtaAgent_AddInstalledFlags(
    uint64_t operation_id,
    const Firmware_SignedHeader_t *target_header,
    const uint8_t target_signature[FIRMWARE_SIGNATURE_SIZE],
    OtaTransport_StatusResponse_t *status
){
    Metadata_Record_t active;

    if((target_header == NULL) || (target_signature == NULL) ||
       (status == NULL) ||
       !ActiveFirmware_LoadMetadata(&active) ||
       (memcmp(&active.active_header, target_header,
               sizeof(active.active_header)) != 0) ||
       (memcmp(active.active_signature, target_signature,
               sizeof(active.active_signature)) != 0)){
        return;
    }
    if((active.trial_boot_flag == METADATA_TRIAL_BOOT_TRUE) &&
       (active.operation_id != operation_id)){
        return;
    }
    status->flags |= OTA_STATUS_FLAG_INSTALLED;
    if((active.trial_boot_flag == METADATA_TRIAL_BOOT_FALSE) &&
       (active.operation_state == METADATA_OPERATION_IDLE) &&
       (active.operation_id == UINT64_C(0))){
        status->flags |= OTA_STATUS_FLAG_CONFIRMED;
    }
}

static OtaProtocol_NackReason_t OtaAgent_PrepareStorage(
    OtaAgent_Context_t *context
){
    if(context->transfer.artifact_type == OTA_ARTIFACT_FULL){
        Staging_Manifest_t manifest;

        if(context->transfer.manifest_size != sizeof(manifest)){
            return OTA_NACK_BAD_FORMAT;
        }
        memcpy(&manifest, context->manifest_buffer, sizeof(manifest));
        if((manifest.transfer_id != context->transfer.transfer_id) ||
           (manifest.operation_id != context->transfer.operation_id) ||
           (memcmp(manifest.artifact_id, context->transfer.artifact_id,
                   OTA_ARTIFACT_ID_SIZE) != 0) ||
           (manifest.image_header.image_size != context->transfer.data_size) ||
           (manifest.chunk_size != context->transfer.chunk_size)){
            return OTA_NACK_BAD_ARTIFACT;
        }
        if(ArtifactStorage_StagingBegin(&manifest) != ARTIFACT_STORAGE_OK){
            return OTA_NACK_STORAGE_ERROR;
        }
    }else{
        Delta_Manifest_t manifest;

        if(context->transfer.manifest_size != sizeof(manifest)){
            return OTA_NACK_BAD_FORMAT;
        }
        memcpy(&manifest, context->manifest_buffer, sizeof(manifest));
        if((manifest.transfer_id != context->transfer.transfer_id) ||
           (manifest.operation_id != context->transfer.operation_id) ||
           (memcmp(manifest.immutable_body.artifact_id,
                   context->transfer.artifact_id,
                   OTA_ARTIFACT_ID_SIZE) != 0) ||
           (manifest.immutable_body.patch_size != context->transfer.data_size) ||
           (manifest.chunk_size != context->transfer.chunk_size)){
            return OTA_NACK_BAD_ARTIFACT;
        }
        if(DeltaStorage_Begin(&manifest) != DELTA_STORAGE_OK){
            return OTA_NACK_STORAGE_ERROR;
        }
    }
    context->storage_prepared = true;
    return OTA_NACK_NONE;
}

static OtaProtocol_NackReason_t OtaAgent_HandleBegin(
    OtaAgent_Context_t *context,
    const SecureFrame_Message_t *message,
    uint32_t *next_offset
){
    OtaTransport_Begin_t begin;

    if(OtaTransport_ParseBegin(
           message->payload,
           message->payload_length,
           &begin
       ) != OTA_TRANSPORT_OK){
        return OTA_NACK_BAD_FORMAT;
    }
    if(((begin.artifact_type == OTA_ARTIFACT_FULL) &&
        ((begin.manifest_size != sizeof(Staging_Manifest_t)) ||
         (begin.data_size > APPLICATION_SIZE))) ||
       ((begin.artifact_type == OTA_ARTIFACT_DELTA) &&
        ((begin.manifest_size != sizeof(Delta_Manifest_t)) ||
         (begin.data_size > W25Q32_DELTA_DATA_SIZE)))){
        return OTA_NACK_OUT_OF_RANGE;
    }
    context->transfer = begin;
    context->manifest_received = 0U;
    context->receive_state = OTA_RECEIVE_IN_PROGRESS;
    context->transfer_active = true;
    context->storage_prepared = false;
    context->resume_existing = false;
    memset(context->manifest_buffer, 0, sizeof(context->manifest_buffer));

    /* BEGIN cùng identity nối lại record persistent thay vì erase nhận lại. */
    if(begin.artifact_type == OTA_ARTIFACT_FULL){
        Staging_Manifest_t manifest;

        if((ArtifactStorage_StagingReadManifest(&manifest) ==
            ARTIFACT_STORAGE_OK) &&
           (manifest.header_commit_marker ==
                STAGING_HEADER_COMMIT_MARKER_VALUE) &&
           (manifest.transfer_id == begin.transfer_id) &&
           (manifest.operation_id == begin.operation_id) &&
           (manifest.data_size == begin.data_size) &&
           (manifest.chunk_size == begin.chunk_size) &&
           (memcmp(manifest.artifact_id, begin.artifact_id,
                   OTA_ARTIFACT_ID_SIZE) == 0)){
            memcpy(context->manifest_buffer, &manifest, sizeof(manifest));
            context->storage_prepared = true;
            context->resume_existing = true;
            *next_offset = OtaAgent_NextMissingFull(&manifest);
        }
    }else{
        Delta_Manifest_t manifest;

        if((DeltaStorage_ReadManifest(&manifest) == DELTA_STORAGE_OK) &&
           (manifest.transfer_id == begin.transfer_id) &&
           (manifest.operation_id == begin.operation_id) &&
           (manifest.immutable_body.patch_size == begin.data_size) &&
           (manifest.chunk_size == begin.chunk_size) &&
           (memcmp(manifest.immutable_body.artifact_id, begin.artifact_id,
                   OTA_ARTIFACT_ID_SIZE) == 0)){
            memcpy(context->manifest_buffer, &manifest, sizeof(manifest));
            context->storage_prepared = true;
            context->resume_existing = true;
            *next_offset = OtaAgent_NextMissingDelta(&manifest);
        }
    }
    return OTA_NACK_NONE;
}

static OtaProtocol_NackReason_t OtaAgent_HandleData(
    OtaAgent_Context_t *context,
    const SecureFrame_Message_t *message,
    uint32_t *next_offset
){
    OtaTransport_DataView_t data;

    if(OtaTransport_ParseData(
           message->payload,
           message->payload_length,
           &data
       ) != OTA_TRANSPORT_OK){
        return OTA_NACK_BAD_FORMAT;
    }
    if(!context->transfer_active ||
       (data.transfer_id != context->transfer.transfer_id)){
        return OTA_NACK_BAD_TRANSFER;
    }
    if(data.section == OTA_SECTION_MANIFEST){
        if((data.offset != context->manifest_received) ||
           (data.offset > context->transfer.manifest_size) ||
           (data.data_length >
                (context->transfer.manifest_size - data.offset))){
            return OTA_NACK_UNEXPECTED_OFFSET;
        }
        if(!context->resume_existing){
            memcpy(&context->manifest_buffer[data.offset], data.data,
                   data.data_length);
        }
        context->manifest_received = (uint16_t)(
            context->manifest_received + data.data_length
        );
        *next_offset = context->manifest_received;
        if(context->manifest_received == context->transfer.manifest_size){
            if(!context->storage_prepared){
                const OtaProtocol_NackReason_t reason =
                    OtaAgent_PrepareStorage(context);
                if(reason != OTA_NACK_NONE){
                    context->receive_state = OTA_RECEIVE_INVALID;
                    return reason;
                }
            }
            if(context->transfer.artifact_type == OTA_ARTIFACT_FULL){
                Staging_Manifest_t manifest;
                memcpy(&manifest, context->manifest_buffer, sizeof(manifest));
                *next_offset = OtaAgent_NextMissingFull(&manifest);
            }else{
                Delta_Manifest_t manifest;
                memcpy(&manifest, context->manifest_buffer, sizeof(manifest));
                *next_offset = OtaAgent_NextMissingDelta(&manifest);
            }
        }
        return OTA_NACK_NONE;
    }
    if(!context->storage_prepared){
        return OTA_NACK_BAD_STATE;
    }

    if(context->transfer.artifact_type == OTA_ARTIFACT_FULL){
        const ArtifactStorage_Status_t status =
            ArtifactStorage_StagingWriteFragment(
                data.offset, data.data, data.data_length
            );
        if(status == ARTIFACT_STORAGE_CONFLICTING_DUPLICATE){
            return OTA_NACK_CONFLICTING_DUPLICATE;
        }
        if((status != ARTIFACT_STORAGE_OK) &&
           (status != ARTIFACT_STORAGE_DUPLICATE)){
            return OTA_NACK_STORAGE_ERROR;
        }
        Staging_Manifest_t manifest;
        if(ArtifactStorage_StagingReadManifest(&manifest) !=
           ARTIFACT_STORAGE_OK){
            return OTA_NACK_STORAGE_ERROR;
        }
        *next_offset = OtaAgent_NextMissingFull(&manifest);
    }else{
        const DeltaStorage_Status_t status = DeltaStorage_WriteFragment(
            data.offset, data.data, data.data_length
        );
        if(status == DELTA_STORAGE_CONFLICTING_DUPLICATE){
            return OTA_NACK_CONFLICTING_DUPLICATE;
        }
        if((status != DELTA_STORAGE_OK) &&
           (status != DELTA_STORAGE_DUPLICATE)){
            return OTA_NACK_STORAGE_ERROR;
        }
        Delta_Manifest_t manifest;
        if(DeltaStorage_ReadManifest(&manifest) != DELTA_STORAGE_OK){
            return OTA_NACK_STORAGE_ERROR;
        }
        *next_offset = OtaAgent_NextMissingDelta(&manifest);
    }
    return OTA_NACK_NONE;
}

static OtaProtocol_NackReason_t OtaAgent_HandleEnd(
    OtaAgent_Context_t *context,
    const SecureFrame_Message_t *message
){
    uint64_t transfer_id;
    Metadata_Record_t active;

    if(OtaTransport_ParseId(
           message->payload,
           message->payload_length,
           &transfer_id
       ) != OTA_TRANSPORT_OK){
        return OTA_NACK_BAD_FORMAT;
    }
    if(!context->storage_prepared ||
       (transfer_id != context->transfer.transfer_id)){
        return OTA_NACK_BAD_TRANSFER;
    }
    if(!ActiveFirmware_LoadMetadata(&active)){
        return OTA_NACK_BAD_STATE;
    }
    context->receive_state = OTA_RECEIVE_COMPLETE_UNVERIFIED;
    if(context->transfer.artifact_type == OTA_ARTIFACT_FULL){
        Staging_Manifest_t manifest;

        if(ArtifactStorage_StagingReadManifest(&manifest) !=
           ARTIFACT_STORAGE_OK ||
           (OtaAgent_NextMissingFull(&manifest) != manifest.data_size)){
            return OTA_NACK_INCOMPLETE;
        }
        if(manifest.image_header.image_version <=
           active.active_header.image_version){
            context->receive_state = OTA_RECEIVE_INVALID;
            return OTA_NACK_POLICY_REJECTED;
        }
        if(FirmwareAuth_Verify(
               &manifest.image_header,
               manifest.image_signature,
               context->public_key,
               OtaAgent_StagingReader,
               NULL
           ) != FIRMWARE_AUTH_OK){
            context->receive_state = OTA_RECEIVE_INVALID;
            return OTA_NACK_AUTH_FAILED;
        }
        if(ArtifactStorage_StagingPublishValid() != ARTIFACT_STORAGE_OK){
            return OTA_NACK_STORAGE_ERROR;
        }
    }else{
        Delta_Manifest_t manifest;

        if(DeltaStorage_ReadManifest(&manifest) != DELTA_STORAGE_OK ||
           (OtaAgent_NextMissingDelta(&manifest) !=
                manifest.immutable_body.patch_size)){
            return OTA_NACK_INCOMPLETE;
        }
        if((manifest.immutable_body.base_version !=
                active.active_header.image_version) ||
           (manifest.immutable_body.target_header.image_version <=
                active.active_header.image_version)){
            context->receive_state = OTA_RECEIVE_INVALID;
            return OTA_NACK_POLICY_REJECTED;
        }
        if((DeltaAuth_VerifyManifest(&manifest, context->public_key) !=
            DELTA_AUTH_OK) ||
           (DeltaAuth_VerifyStreamHash(
                OtaAgent_DeltaReader,
                NULL,
                manifest.immutable_body.patch_size,
                manifest.immutable_body.patch_sha512
            ) != DELTA_AUTH_OK)){
            context->receive_state = OTA_RECEIVE_INVALID;
            return OTA_NACK_AUTH_FAILED;
        }
        if(DeltaStorage_PublishValid() != DELTA_STORAGE_OK){
            return OTA_NACK_STORAGE_ERROR;
        }
    }
    context->receive_state = OTA_RECEIVE_VALID;
    return OTA_NACK_NONE;
}

static OtaProtocol_NackReason_t OtaAgent_HandleAbort(
    OtaAgent_Context_t *context,
    const SecureFrame_Message_t *message
){
    uint64_t transfer_id;

    if(OtaTransport_ParseId(
           message->payload,
           message->payload_length,
           &transfer_id
       ) != OTA_TRANSPORT_OK){
        return OTA_NACK_BAD_FORMAT;
    }
    if(!context->transfer_active ||
       (transfer_id != context->transfer.transfer_id)){
        return OTA_NACK_BAD_TRANSFER;
    }
    if(context->transfer.artifact_type == OTA_ARTIFACT_FULL){
        if(ArtifactStorage_StagingInvalidate() != ARTIFACT_STORAGE_OK){
            return OTA_NACK_STORAGE_ERROR;
        }
    }else if(DeltaStorage_Invalidate() != DELTA_STORAGE_OK){
        return OTA_NACK_STORAGE_ERROR;
    }
    context->receive_state = OTA_RECEIVE_INVALID;
    context->transfer_active = false;
    context->storage_prepared = false;
    context->resume_existing = false;
    return OTA_NACK_NONE;
}

static OtaProtocol_NackReason_t OtaAgent_ReconstructDelta(
    OtaAgent_Context_t *context
){
    Delta_Manifest_t delta;
    Metadata_Record_t active;
    Staging_Manifest_t staging;
    OtaAgent_DeltaWriter_t writer;
    uint32_t base_size;

    if((DeltaStorage_ReadManifest(&delta) != DELTA_STORAGE_OK) ||
       (delta.artifact_valid_marker != DELTA_ARTIFACT_VALID_MARKER_VALUE) ||
       !ActiveFirmware_LoadMetadata(&active)){
        return OTA_NACK_BAD_STATE;
    }
    if((active.active_header.image_version != delta.immutable_body.base_version) ||
       (active.active_header.image_size != delta.immutable_body.base_size)){
        return OTA_NACK_POLICY_REJECTED;
    }
    base_size = delta.immutable_body.base_size;
    if(DeltaAuth_VerifyStreamHash(
           OtaAgent_InternalReader,
           &base_size,
           base_size,
           delta.immutable_body.base_sha512
       ) != DELTA_AUTH_OK){
        return OTA_NACK_POLICY_REJECTED;
    }
    if(DeltaStorage_MarkApplyStarted() != DELTA_STORAGE_OK){
        return OTA_NACK_STORAGE_ERROR;
    }

    memset(&staging, 0, sizeof(staging));
    staging.operation_id = delta.operation_id;
    staging.transfer_id = delta.transfer_id;
    memcpy(staging.artifact_id,
           delta.immutable_body.artifact_id,
           sizeof(staging.artifact_id));
    staging.image_header = delta.immutable_body.target_header;
    memcpy(staging.image_signature,
           delta.immutable_body.target_signature,
           sizeof(staging.image_signature));
    staging.chunk_size = OTA_DATA_CHUNK_MAX_SIZE;
    if(ArtifactStorage_StagingBegin(&staging) != ARTIFACT_STORAGE_OK){
        return OTA_NACK_STORAGE_ERROR;
    }

    memset(&writer, 0, sizeof(writer));
    context->receive_state = OTA_RECEIVE_APPLY_IN_PROGRESS;
    if((DeltaApply_Run(
            &delta.immutable_body,
            OtaAgent_InternalReader,
            &base_size,
            OtaAgent_DeltaReader,
            NULL,
            OtaAgent_StagingWriter,
            &writer
        ) != DELTA_APPLY_OK) ||
       !OtaAgent_FlushDeltaWriter(&writer, true)){
        return OTA_NACK_STORAGE_ERROR;
    }
    if((DeltaAuth_VerifyStreamHash(
            OtaAgent_StagingReader,
            NULL,
            staging.image_header.image_size,
            delta.immutable_body.target_sha512
        ) != DELTA_AUTH_OK) ||
       (FirmwareAuth_Verify(
           &staging.image_header,
           staging.image_signature,
           context->public_key,
           OtaAgent_StagingReader,
           NULL
       ) != FIRMWARE_AUTH_OK)){
        return OTA_NACK_AUTH_FAILED;
    }
    if(ArtifactStorage_StagingPublishValid() != ARTIFACT_STORAGE_OK){
        return OTA_NACK_STORAGE_ERROR;
    }
    context->receive_state = OTA_RECEIVE_VALID;
    return OTA_NACK_NONE;
}

static OtaProtocol_NackReason_t OtaAgent_HandleBoot(
    OtaAgent_Context_t *context,
    const SecureFrame_Message_t *message,
    bool *reset_requested
){
    uint64_t operation_id;
    Metadata_Record_t active;
    Staging_Manifest_t staging;

    if(OtaTransport_ParseId(
           message->payload,
           message->payload_length,
           &operation_id
       ) != OTA_TRANSPORT_OK){
        return OTA_NACK_BAD_FORMAT;
    }
    if(context->receive_state != OTA_RECEIVE_VALID ||
       (operation_id != context->transfer.operation_id)){
        return OTA_NACK_BAD_STATE;
    }
    if(context->transfer.artifact_type == OTA_ARTIFACT_DELTA){
        const OtaProtocol_NackReason_t reason =
            OtaAgent_ReconstructDelta(context);
        if(reason != OTA_NACK_NONE){
            return reason;
        }
    }
    if(!ActiveFirmware_LoadMetadata(&active) ||
       (ArtifactStorage_StagingReadManifest(&staging) !=
            ARTIFACT_STORAGE_OK) ||
       (staging.artifact_valid_marker != STAGING_ARTIFACT_VALID_MARKER_VALUE) ||
       (staging.operation_id != operation_id)){
        return OTA_NACK_BAD_STATE;
    }
    if(ArtifactStorage_BackupCreate(
           operation_id,
           &active.active_header,
           active.active_signature
       ) != ARTIFACT_STORAGE_OK){
        return OTA_NACK_STORAGE_ERROR;
    }
    if(BootMailboxIo_Write(
           BOOT_MAILBOX_COMMAND_UPDATE_READY,
           operation_id
       ) != BOOT_MAILBOX_IO_OK){
        return OTA_NACK_STORAGE_ERROR;
    }
    *reset_requested = true;
    return OTA_NACK_NONE;
}

/*
 * Chuyển Staging Manifest bền vững thành STATUS_RESPONSE.
 * query_id bằng 0 nghĩa là chấp nhận record hiện có; giá trị khác 0 phải khớp.
 */
static bool OtaAgent_ReadFullStatus(
    uint64_t query_id,
    OtaTransport_StatusResponse_t *status,
    uint64_t *operation_id
){
    Staging_Manifest_t manifest;

    if((status == NULL) || (operation_id == NULL) ||
       (ArtifactStorage_StagingReadManifest(&manifest) !=
            ARTIFACT_STORAGE_OK) ||
       ((query_id != UINT64_C(0)) &&
        (manifest.transfer_id != query_id))){
        return false;
    }

    memset(status, 0, sizeof(*status));
    status->artifact_type = OTA_ARTIFACT_FULL;
    status->transfer_id = manifest.transfer_id;
    memcpy(status->artifact_id, manifest.artifact_id,
           sizeof(status->artifact_id));
    status->data_size = manifest.data_size;
    status->chunk_size = manifest.chunk_size;
    status->next_missing_offset = OtaAgent_NextMissingFull(&manifest);
    status->flags = OTA_STATUS_FLAG_HEADER_COMMITTED;
    status->receive_state = status->next_missing_offset == status->data_size ?
        OTA_RECEIVE_COMPLETE_UNVERIFIED : OTA_RECEIVE_IN_PROGRESS;
    if(manifest.artifact_valid_marker ==
       STAGING_ARTIFACT_VALID_MARKER_VALUE){
        status->flags |= OTA_STATUS_FLAG_ARTIFACT_VALID |
            OTA_STATUS_FLAG_STAGING_READY;
        status->receive_state = OTA_RECEIVE_VALID;
    }
    OtaAgent_AddInstalledFlags(
        manifest.operation_id,
        &manifest.image_header,
        manifest.image_signature,
        status
    );
    *operation_id = manifest.operation_id;
    return true;
}

/* Chuyển Delta Manifest bền vững thành STATUS_RESPONSE. */
static bool OtaAgent_ReadDeltaStatus(
    uint64_t query_id,
    OtaTransport_StatusResponse_t *status,
    uint64_t *operation_id
){
    Delta_Manifest_t manifest;
    Staging_Manifest_t staging;

    if((status == NULL) || (operation_id == NULL) ||
       (DeltaStorage_ReadManifest(&manifest) != DELTA_STORAGE_OK) ||
       ((query_id != UINT64_C(0)) &&
        (manifest.transfer_id != query_id))){
        return false;
    }

    memset(status, 0, sizeof(*status));
    status->artifact_type = OTA_ARTIFACT_DELTA;
    status->transfer_id = manifest.transfer_id;
    memcpy(status->artifact_id, manifest.immutable_body.artifact_id,
           sizeof(status->artifact_id));
    status->data_size = manifest.immutable_body.patch_size;
    status->chunk_size = manifest.chunk_size;
    status->next_missing_offset = OtaAgent_NextMissingDelta(&manifest);
    status->flags = OTA_STATUS_FLAG_HEADER_COMMITTED;
    status->receive_state = status->next_missing_offset == status->data_size ?
        OTA_RECEIVE_COMPLETE_UNVERIFIED : OTA_RECEIVE_IN_PROGRESS;
    if(manifest.artifact_valid_marker == DELTA_ARTIFACT_VALID_MARKER_VALUE){
        status->flags |= OTA_STATUS_FLAG_ARTIFACT_VALID;
        status->receive_state = OTA_RECEIVE_VALID;
    }
    if(manifest.apply_started_marker == DELTA_APPLY_STARTED_MARKER_VALUE){
        status->flags |= OTA_STATUS_FLAG_APPLY_STARTED;
        status->receive_state = OTA_RECEIVE_APPLY_IN_PROGRESS;
    }
    if((ArtifactStorage_StagingReadManifest(&staging) ==
            ARTIFACT_STORAGE_OK) &&
       (staging.artifact_valid_marker ==
            STAGING_ARTIFACT_VALID_MARKER_VALUE) &&
       (staging.operation_id == manifest.operation_id) &&
       (memcmp(&staging.image_header,
               &manifest.immutable_body.target_header,
               sizeof(staging.image_header)) == 0) &&
       (memcmp(staging.image_signature,
               manifest.immutable_body.target_signature,
               sizeof(staging.image_signature)) == 0)){
        status->flags |= OTA_STATUS_FLAG_STAGING_READY;
        status->receive_state = OTA_RECEIVE_VALID;
    }
    OtaAgent_AddInstalledFlags(
        manifest.operation_id,
        &manifest.immutable_body.target_header,
        manifest.immutable_body.target_signature,
        status
    );
    *operation_id = manifest.operation_id;
    return true;
}

static OtaAgent_Status_t OtaAgent_BuildStatusReply(
    OtaAgent_Context_t *context,
    const SecureFrame_Message_t *request,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length
){
    OtaTransport_StatusResponse_t status;
    Backup_Manifest_t backup;
    uint8_t payload[OTA_STATUS_RESPONSE_PAYLOAD_SIZE];
    uint64_t query_id;
    uint64_t operation_id = UINT64_C(0);
    bool found = false;

    if(OtaTransport_ParseId(
           request->payload,
           request->payload_length,
           &query_id
       ) != OTA_TRANSPORT_OK){
        return OtaAgent_BuildReply(
            context, request, OTA_NACK_BAD_FORMAT, 0U,
            response, response_capacity, response_length
        );
    }

    /* Khi đang chạy, loại artifact trong context là lựa chọn không mơ hồ. */
    if(context->transfer_active &&
       ((query_id == UINT64_C(0)) ||
        (query_id == context->transfer.transfer_id))){
        found = context->transfer.artifact_type == OTA_ARTIFACT_FULL ?
            OtaAgent_ReadFullStatus(query_id, &status, &operation_id) :
            OtaAgent_ReadDeltaStatus(query_id, &status, &operation_id);
    }
    /* Sau reset, tìm record persistent khớp transfer_id. */
    if(!found){
        found = OtaAgent_ReadFullStatus(query_id, &status, &operation_id);
    }
    if(!found){
        found = OtaAgent_ReadDeltaStatus(query_id, &status, &operation_id);
    }
    if(!found){
        if(query_id != UINT64_C(0)){
            return OtaAgent_BuildReply(
                context, request, OTA_NACK_BAD_TRANSFER, 0U,
                response, response_capacity, response_length
            );
        }
        memset(&status, 0, sizeof(status));
        status.receive_state = OTA_RECEIVE_EMPTY;
    }

    if(found &&
       (ArtifactStorage_BackupReadManifest(&backup) == ARTIFACT_STORAGE_OK) &&
       (backup.operation_id == operation_id)){
        status.flags |= OTA_STATUS_FLAG_BACKUP_VALID;
    }
    if(OtaTransport_BuildStatusResponse(&status, payload) !=
       OTA_TRANSPORT_OK){
        return OTA_AGENT_RESPONSE_ERROR;
    }
    return SecureFrame_Encode(
        &context->secure_frame,
        OTA_RESPONSE_STATUS,
        request->sequence,
        payload,
        sizeof(payload),
        response,
        response_capacity,
        response_length
    ) == SECURE_FRAME_OK ? OTA_AGENT_OK : OTA_AGENT_RESPONSE_ERROR;
}

static OtaAgent_Status_t OtaAgent_BuildReply(
    OtaAgent_Context_t *context,
    const SecureFrame_Message_t *request,
    OtaProtocol_NackReason_t reason,
    uint32_t next_offset,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length
){
    OtaTransport_Ack_t ack = {
        .request_type = request->type,
        .status_or_reason = (uint8_t)reason,
        .transfer_id = context->transfer_active ?
            context->transfer.transfer_id : UINT64_C(0),
        .next_missing_offset = next_offset
    };
    uint8_t payload[OTA_ACK_FIXED_PAYLOAD_SIZE];

    if(OtaTransport_BuildAck(&ack, payload) != OTA_TRANSPORT_OK){
        return OTA_AGENT_RESPONSE_ERROR;
    }
    return SecureFrame_Encode(
        &context->secure_frame,
        reason == OTA_NACK_NONE ? OTA_RESPONSE_ACK : OTA_RESPONSE_NACK,
        request->sequence,
        payload,
        sizeof(payload),
        response,
        response_capacity,
        response_length
    ) == SECURE_FRAME_OK ? OTA_AGENT_OK : OTA_AGENT_RESPONSE_ERROR;
}

OtaAgent_Status_t OtaAgent_Init(
    OtaAgent_Context_t *context,
    const uint8_t transport_key[CRYPTO_TRANSPORT_KEY_SIZE],
    const uint8_t public_key[CRYPTO_ED25519_PUBLIC_KEY_SIZE]
){
    if((context == NULL) || (transport_key == NULL) || (public_key == NULL)){
        return OTA_AGENT_INVALID_ARGUMENT;
    }
    memset(context, 0, sizeof(*context));
    memcpy(context->public_key, public_key, sizeof(context->public_key));
    if(W25Q32_Init() != EXT_FLASH_OK){
        return OTA_AGENT_STORAGE_ERROR;
    }
    if(SecureFrame_Init(&context->secure_frame, transport_key) !=
       SECURE_FRAME_OK){
        return OTA_AGENT_NOT_READY;
    }
    SecureFrame_SetSessionAcceptor(
        &context->secure_frame,
        TransportSessionStore_AcceptCallback,
        NULL
    );
    context->receive_state = OTA_RECEIVE_EMPTY;
    context->initialized = true;
    return OTA_AGENT_OK;
}

OtaAgent_Status_t OtaAgent_ProcessFrame(
    OtaAgent_Context_t *context,
    const uint8_t *request,
    size_t request_length,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length,
    bool *reset_requested
){
    SecureFrame_Message_t message;
    OtaProtocol_NackReason_t reason = OTA_NACK_NONE;
    uint32_t next_offset = 0U;

    if((context == NULL) || (request == NULL) || (response == NULL) ||
       (response_length == NULL) || (reset_requested == NULL)){
        return OTA_AGENT_INVALID_ARGUMENT;
    }
    if(!context->initialized){
        return OTA_AGENT_NOT_READY;
    }
    *response_length = 0U;
    *reset_requested = false;
    if(SecureFrame_Decode(
           &context->secure_frame,
           request,
           request_length,
           &message
       ) != SECURE_FRAME_OK){
        return OTA_AGENT_FRAME_ERROR;
    }

    switch(message.type){
        case OTA_COMMAND_BEGIN:
            reason = OtaAgent_HandleBegin(context, &message, &next_offset);
            break;
        case OTA_COMMAND_DATA:
            reason = OtaAgent_HandleData(context, &message, &next_offset);
            break;
        case OTA_COMMAND_END:
            reason = OtaAgent_HandleEnd(context, &message);
            break;
        case OTA_COMMAND_ABORT:
            reason = OtaAgent_HandleAbort(context, &message);
            break;
        case OTA_COMMAND_BOOT:
            reason = OtaAgent_HandleBoot(
                context, &message, reset_requested
            );
            break;
        case OTA_COMMAND_STATUS:
            return OtaAgent_BuildStatusReply(
                context, &message, response, response_capacity,
                response_length
            );
        default:
            reason = OTA_NACK_BAD_FORMAT;
            break;
    }
    return OtaAgent_BuildReply(
        context, &message, reason, next_offset,
        response, response_capacity, response_length
    );
}
