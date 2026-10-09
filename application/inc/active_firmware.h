#ifndef AEGIS_ACTIVE_FIRMWARE_H
#define AEGIS_ACTIVE_FIRMWARE_H

/* Application chỉ đọc Metadata A/B để lấy identity App hiện tại. */

#include "metadata_struct.h"

#include <stdbool.h>

bool ActiveFirmware_LoadMetadata(Metadata_Record_t *record);

#endif /* AEGIS_ACTIVE_FIRMWARE_H */
