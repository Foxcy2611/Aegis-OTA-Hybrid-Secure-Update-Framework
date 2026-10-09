#include "active_firmware.h"

#include "metadata_record.h"

#include <stdint.h>
#include <string.h>

bool ActiveFirmware_LoadMetadata(Metadata_Record_t *record){
    Metadata_Record_t first;
    Metadata_Record_t second;
    bool first_valid;
    bool second_valid;

    if(record == NULL){
        return false;
    }
    memcpy(&first,
           (const void *)(uintptr_t)METADATA_PAGE_A_ADDRESS,
           sizeof(first));
    memcpy(&second,
           (const void *)(uintptr_t)METADATA_PAGE_B_ADDRESS,
           sizeof(second));
    first_valid = MetadataRecord_IsStructurallyValid(&first);
    second_valid = MetadataRecord_IsStructurallyValid(&second);
    if(first_valid && second_valid){
        const uint32_t difference = first.generation - second.generation;

        if(difference == UINT32_C(0x80000000)){
            return false;
        }
        *record = ((difference == 0U) ||
                   (difference < UINT32_C(0x80000000))) ? first : second;
        return true;
    }
    if(first_valid){
        *record = first;
        return true;
    }
    if(second_valid){
        *record = second;
        return true;
    }
    return false;
}
