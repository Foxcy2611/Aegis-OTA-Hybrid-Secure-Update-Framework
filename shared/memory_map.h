#ifndef AEGIS_MEMORY_MAP_H
#define AEGIS_MEMORY_MAP_H

/*
 * Memory map chuẩn của STM32F103C8T6 và W25Q32.
 */

/* ==================== STM32F103C8T6 ==================== */

#define INTERNAL_FLASH_BASE_ADDRESS     0x08000000U
#define INTERNAL_FLASH_SIZE             0x00010000U /* 64 KB */
#define INTERNAL_FLASH_END_ADDRESS      (INTERNAL_FLASH_BASE_ADDRESS + INTERNAL_FLASH_SIZE)

#define FLASH_PAGE_SIZE                 0x00000400U /* 1 KB */

/* Bootloader: 0x08000000..0x08003FFF */

#define BOOTLOADER_BASE_ADDRESS         INTERNAL_FLASH_BASE_ADDRESS
#define BOOTLOADER_SIZE                 0x00004000U /* 16 KB */
#define BOOTLOADER_END_ADDRESS          (BOOTLOADER_BASE_ADDRESS + BOOTLOADER_SIZE)

/* Application: 0x08004000..0x0800F7FF */

#define APPLICATION_BASE_ADDRESS        BOOTLOADER_END_ADDRESS
#define APPLICATION_SIZE                0x0000B800U /* 46 KB */
#define APPLICATION_END_ADDRESS         (APPLICATION_BASE_ADDRESS + APPLICATION_SIZE)

/* Metadata A/B: 0x0800F800..0x0800FFFF */

#define METADATA_PAGE_SIZE              FLASH_PAGE_SIZE

#define METADATA_PAGE_A_ADDRESS         APPLICATION_END_ADDRESS
#define METADATA_PAGE_A_END_ADDRESS     (METADATA_PAGE_A_ADDRESS + METADATA_PAGE_SIZE)

#define METADATA_PAGE_B_ADDRESS         METADATA_PAGE_A_END_ADDRESS
#define METADATA_PAGE_B_END_ADDRESS     (METADATA_PAGE_B_ADDRESS + METADATA_PAGE_SIZE)

#define METADATA_AREA_SIZE              (2U * METADATA_PAGE_SIZE)
#define METADATA_END_ADDRESS            METADATA_PAGE_B_END_ADDRESS

/* SRAM: 0x20000000..0x20004FFF */

#define SRAM_START_ADDRESS              0x20000000U
#define SRAM_SIZE                       0x00005000U /* 20 KB */
#define SRAM_END_ADDRESS                (SRAM_START_ADDRESS + SRAM_SIZE)

/* ========================= W25Q32 ========================= */

#define W25Q32_BASE_ADDRESS             0x00000000U
#define W25Q32_TOTAL_SIZE               0x00400000U /* 4 MB */
#define W25Q32_END_ADDRESS              (W25Q32_BASE_ADDRESS + W25Q32_TOTAL_SIZE)

#define W25Q32_PAGE_SIZE                0x00000100U /* 256 bytes */
#define W25Q32_SECTOR_SIZE              0x00001000U /* 4 KB */

/* STAGING: 0x000000..0x03FFFF */

#define W25Q32_STAGING_BASE             W25Q32_BASE_ADDRESS
#define W25Q32_STAGING_SIZE             0x00040000U /* 256 KB */
#define W25Q32_STAGING_END              (W25Q32_STAGING_BASE + W25Q32_STAGING_SIZE)

/* STAGING_MANIFEST: 0x000000..0x000FFF */

#define W25Q32_STAGING_MANIFEST_BASE    W25Q32_STAGING_BASE
#define W25Q32_STAGING_MANIFEST_SIZE    W25Q32_SECTOR_SIZE
#define W25Q32_STAGING_MANIFEST_END     (W25Q32_STAGING_MANIFEST_BASE + W25Q32_STAGING_MANIFEST_SIZE)

/* STAGING_IMAGE: 0x001000..0x03FFFF */

#define W25Q32_STAGING_IMAGE_BASE       W25Q32_STAGING_MANIFEST_END
#define W25Q32_STAGING_IMAGE_SIZE       (W25Q32_STAGING_END - W25Q32_STAGING_IMAGE_BASE)
#define W25Q32_STAGING_IMAGE_END        (W25Q32_STAGING_IMAGE_BASE + W25Q32_STAGING_IMAGE_SIZE)

/* BACKUP: 0x040000..0x07FFFF */

#define W25Q32_BACKUP_SLOT_BASE         W25Q32_STAGING_END
#define W25Q32_BACKUP_SLOT_SIZE         0x00040000U /* 256 KB */
#define W25Q32_BACKUP_SLOT_END          (W25Q32_BACKUP_SLOT_BASE + W25Q32_BACKUP_SLOT_SIZE)

/* BACKUP_MANIFEST: 0x040000..0x040FFF */

#define W25Q32_BACKUP_MANIFEST_BASE     W25Q32_BACKUP_SLOT_BASE
#define W25Q32_BACKUP_MANIFEST_SIZE     W25Q32_SECTOR_SIZE
#define W25Q32_BACKUP_MANIFEST_END      (W25Q32_BACKUP_MANIFEST_BASE + W25Q32_BACKUP_MANIFEST_SIZE)

/* BACKUP_IMAGE: 0x041000..0x07FFFF */

#define W25Q32_BACKUP_IMAGE_BASE        W25Q32_BACKUP_MANIFEST_END
#define W25Q32_BACKUP_IMAGE_SIZE        (W25Q32_BACKUP_SLOT_END - W25Q32_BACKUP_IMAGE_BASE)
#define W25Q32_BACKUP_IMAGE_END         (W25Q32_BACKUP_IMAGE_BASE + W25Q32_BACKUP_IMAGE_SIZE)

/* DELTA: 0x080000..0x0BFFFF */

#define W25Q32_DELTA_AREA_BASE          W25Q32_BACKUP_SLOT_END
#define W25Q32_DELTA_AREA_SIZE          0x00040000U /* 256 KB */
#define W25Q32_DELTA_AREA_END           (W25Q32_DELTA_AREA_BASE + W25Q32_DELTA_AREA_SIZE)

/* DELTA_MANIFEST: 0x080000..0x080FFF */

#define W25Q32_DELTA_MANIFEST_BASE      W25Q32_DELTA_AREA_BASE
#define W25Q32_DELTA_MANIFEST_SIZE      W25Q32_SECTOR_SIZE
#define W25Q32_DELTA_MANIFEST_END       (W25Q32_DELTA_MANIFEST_BASE + W25Q32_DELTA_MANIFEST_SIZE)

/* DELTA_DATA: 0x081000..0x0BFFFF */

#define W25Q32_DELTA_DATA_BASE          W25Q32_DELTA_MANIFEST_END
#define W25Q32_DELTA_DATA_SIZE          (W25Q32_DELTA_AREA_END - W25Q32_DELTA_DATA_BASE)
#define W25Q32_DELTA_DATA_END           (W25Q32_DELTA_DATA_BASE + W25Q32_DELTA_DATA_SIZE)

/* RESERVED: 0x0C0000..0x3FFFFF */

#define W25Q32_RESERVED_BASE            W25Q32_DELTA_AREA_END
#define W25Q32_RESERVED_SIZE            (W25Q32_END_ADDRESS - W25Q32_RESERVED_BASE)
#define W25Q32_RESERVED_END             (W25Q32_RESERVED_BASE + W25Q32_RESERVED_SIZE)

/* Append-only session journal nằm trong vùng Reserved: 0x0C0000..0x0CFFFF. */

#define W25Q32_SESSION_JOURNAL_BASE     W25Q32_RESERVED_BASE
#define W25Q32_SESSION_JOURNAL_SIZE     0x00010000U /* 64 KB */
#define W25Q32_SESSION_JOURNAL_END      \
    (W25Q32_SESSION_JOURNAL_BASE + W25Q32_SESSION_JOURNAL_SIZE)

/* Phần Reserved còn lại dành cho mở rộng sau này: 0x0D0000..0x3FFFFF. */

#define W25Q32_FUTURE_RESERVED_BASE     W25Q32_SESSION_JOURNAL_END
#define W25Q32_FUTURE_RESERVED_SIZE     \
    (W25Q32_RESERVED_END - W25Q32_FUTURE_RESERVED_BASE)
#define W25Q32_FUTURE_RESERVED_END      \
    (W25Q32_FUTURE_RESERVED_BASE + W25Q32_FUTURE_RESERVED_SIZE)

/* ==================== Kiểm tra Internal Flash ==================== */

_Static_assert(BOOTLOADER_END_ADDRESS == APPLICATION_BASE_ADDRESS,
               "Bootloader phải kết thúc tại đầu Application");

_Static_assert(APPLICATION_END_ADDRESS == METADATA_PAGE_A_ADDRESS,
               "Application phải kết thúc tại đầu Metadata A");

_Static_assert(METADATA_PAGE_A_END_ADDRESS == METADATA_PAGE_B_ADDRESS,
               "Hai page Metadata phải liên tiếp");

_Static_assert(METADATA_PAGE_B_END_ADDRESS == INTERNAL_FLASH_END_ADDRESS,
               "Metadata B phải kết thúc tại biên Internal Flash");

_Static_assert(METADATA_AREA_SIZE == 0x00000800U,
               "Vùng Metadata phải chứa đúng hai page Flash");

_Static_assert((BOOTLOADER_BASE_ADDRESS % FLASH_PAGE_SIZE) == 0U,
               "Địa chỉ đầu Bootloader phải thẳng hàng page Flash");

_Static_assert((BOOTLOADER_SIZE % FLASH_PAGE_SIZE) == 0U,
               "Kích thước Bootloader phải gồm các page Flash nguyên vẹn");

_Static_assert((APPLICATION_BASE_ADDRESS % FLASH_PAGE_SIZE) == 0U,
               "Địa chỉ đầu Application phải thẳng hàng page Flash");

_Static_assert((APPLICATION_SIZE % FLASH_PAGE_SIZE) == 0U,
               "Kích thước Application phải gồm các page Flash nguyên vẹn");

_Static_assert((METADATA_PAGE_A_ADDRESS % FLASH_PAGE_SIZE) == 0U,
               "Metadata A phải thẳng hàng page Flash");

_Static_assert((METADATA_PAGE_B_ADDRESS % FLASH_PAGE_SIZE) == 0U,
               "Metadata B phải thẳng hàng page Flash");

_Static_assert(APPLICATION_SIZE == 0x0000B800U,
               "Kích thước Application phải giữ cố định ở 46 KB");

/* ======================= Kiểm tra W25Q32 ======================= */

_Static_assert((W25Q32_TOTAL_SIZE % W25Q32_SECTOR_SIZE) == 0U,
               "Tổng dung lượng W25Q32 phải thẳng hàng sector");

_Static_assert((W25Q32_SECTOR_SIZE % W25Q32_PAGE_SIZE) == 0U,
               "Mỗi sector W25Q32 phải chứa các page nguyên vẹn");

_Static_assert(W25Q32_STAGING_BASE == W25Q32_BASE_ADDRESS,
               "Staging phải bắt đầu tại địa chỉ đầu W25Q32");

_Static_assert(W25Q32_STAGING_END == W25Q32_BACKUP_SLOT_BASE,
               "Staging và Backup phải liên tiếp");

_Static_assert(W25Q32_BACKUP_SLOT_END == W25Q32_DELTA_AREA_BASE,
               "Backup và Delta phải liên tiếp");

_Static_assert(W25Q32_DELTA_AREA_END == W25Q32_RESERVED_BASE,
               "Delta và Reserved phải liên tiếp");

_Static_assert(W25Q32_RESERVED_END == W25Q32_END_ADDRESS,
               "Reserved phải kết thúc tại biên W25Q32");

_Static_assert((W25Q32_STAGING_BASE % W25Q32_SECTOR_SIZE) == 0U,
               "Địa chỉ đầu Staging phải thẳng hàng sector");

_Static_assert((W25Q32_STAGING_SIZE % W25Q32_SECTOR_SIZE) == 0U,
               "Kích thước Staging phải thẳng hàng sector");

_Static_assert(W25Q32_STAGING_MANIFEST_SIZE == W25Q32_SECTOR_SIZE,
               "Staging manifest phải chiếm đúng một sector");

_Static_assert(W25Q32_STAGING_MANIFEST_END == W25Q32_STAGING_IMAGE_BASE,
               "Staging Image phải bắt đầu sau Staging manifest");

_Static_assert(W25Q32_STAGING_IMAGE_END == W25Q32_STAGING_END,
               "Staging Image phải kết thúc tại biên Staging");

_Static_assert((W25Q32_BACKUP_SLOT_BASE % W25Q32_SECTOR_SIZE) == 0U,
               "Địa chỉ đầu Backup phải thẳng hàng sector");

_Static_assert((W25Q32_BACKUP_SLOT_SIZE % W25Q32_SECTOR_SIZE) == 0U,
               "Kích thước Backup phải thẳng hàng sector");

_Static_assert((W25Q32_DELTA_AREA_BASE % W25Q32_SECTOR_SIZE) == 0U,
               "Địa chỉ đầu Delta phải thẳng hàng sector");

_Static_assert((W25Q32_DELTA_AREA_SIZE % W25Q32_SECTOR_SIZE) == 0U,
               "Kích thước Delta phải thẳng hàng sector");

_Static_assert(W25Q32_DELTA_MANIFEST_SIZE == W25Q32_SECTOR_SIZE,
               "Delta manifest phải chiếm đúng một sector");

_Static_assert(W25Q32_DELTA_MANIFEST_END == W25Q32_DELTA_DATA_BASE,
               "Delta Data phải bắt đầu sau Delta manifest");

_Static_assert(W25Q32_DELTA_DATA_END == W25Q32_DELTA_AREA_END,
               "Delta Data phải kết thúc tại biên Delta");

_Static_assert((W25Q32_RESERVED_BASE % W25Q32_SECTOR_SIZE) == 0U,
               "Địa chỉ đầu Reserved phải thẳng hàng sector");

_Static_assert((W25Q32_RESERVED_SIZE % W25Q32_SECTOR_SIZE) == 0U,
               "Kích thước Reserved phải thẳng hàng sector");

_Static_assert(W25Q32_BACKUP_MANIFEST_SIZE == W25Q32_SECTOR_SIZE,
               "Backup manifest phải chiếm đúng một sector");

_Static_assert(W25Q32_BACKUP_MANIFEST_END ==
                   W25Q32_BACKUP_IMAGE_BASE,
               "Backup Image phải bắt đầu sau Backup manifest");

_Static_assert(W25Q32_BACKUP_IMAGE_END ==
                   W25Q32_BACKUP_SLOT_END,
               "Backup Image phải kết thúc tại biên Backup Slot");

_Static_assert(APPLICATION_SIZE <= W25Q32_STAGING_IMAGE_SIZE,
               "Application phải nằm gọn trong Staging Image");

_Static_assert(APPLICATION_SIZE <= W25Q32_BACKUP_IMAGE_SIZE,
               "Application phải nằm gọn trong Backup Image");

_Static_assert(W25Q32_RESERVED_SIZE == 0x00340000U,
               "Vùng Reserved phải giữ cố định ở 3.25 MB");

_Static_assert((W25Q32_SESSION_JOURNAL_BASE % W25Q32_SECTOR_SIZE) == 0U,
               "Session journal phải thẳng hàng sector");

_Static_assert((W25Q32_SESSION_JOURNAL_SIZE % W25Q32_SECTOR_SIZE) == 0U,
               "Session journal phải gồm các sector nguyên vẹn");

_Static_assert(W25Q32_FUTURE_RESERVED_END == W25Q32_RESERVED_END,
               "Future Reserved phải kết thúc tại biên Reserved");

#endif /* AEGIS_MEMORY_MAP_H */
