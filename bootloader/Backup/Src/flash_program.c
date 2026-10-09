#include "flash_program.h"

#include "memory_map.h"
#include "stm32f10x_flash.h"

#include <stdbool.h>
#include <stddef.h>

/* Ánh xạ trạng thái STM32 SPL sang contract của project. */
static Flash_Status_t FlashProgram_MapSTStatus(FLASH_Status status){
    switch(status){
        case FLASH_COMPLETE:    return FLASH_PROGRAM_OK;
        case FLASH_BUSY:        return FLASH_PROGRAM_BUSY;
        case FLASH_ERROR_PG:    return FLASH_PROGRAM_PROGRAM_ERROR;
        case FLASH_ERROR_WRP:   return FLASH_PROGRAM_WRITE_PROTECTED;
        case FLASH_TIMEOUT:     return FLASH_PROGRAM_TIMEOUT;
        default:                return FLASH_PROGRAM_PROGRAM_ERROR;
    }
}


// Chỉ Application và Metadata được phép ghi. 
static bool FlashProgram_IsValidRange(uint32_t address, uint32_t length){
    if(length == 0U){
        return false;
    }

    if((address < APPLICATION_BASE_ADDRESS) ||
        (address >= METADATA_END_ADDRESS)){
        return false;
    }

    return length <= (METADATA_END_ADDRESS - address);
}

// Check 1 page có trắng
static bool FlashProgram_IsErased(uint32_t address, uint32_t length){
    const volatile uint8_t *flash = (const volatile uint8_t *)(uintptr_t)address;

    for(uint32_t i = 0U; i < length; ++i){
        if(flash[i] != 0xFFU){
            return false;
        }
    }

    return true;
}

static Flash_Status_t FlashProgram_WriteHalfWordUnlocked(uint32_t address,uint16_t data){
    const FLASH_Status spl_status = FLASH_ProgramHalfWord(address, data);
    if(spl_status != FLASH_COMPLETE){
        return FlashProgram_MapSTStatus(spl_status);
    }

    if(*(const volatile uint16_t *)(uintptr_t)address != data){
        return FLASH_PROGRAM_VERIFY_FAILED;
    }

    return FLASH_PROGRAM_OK;
}

static Flash_Status_t FlashProgram_FinishTransaction(Flash_Status_t status){
    const Flash_Status_t lock_status = FlashProgram_Lock();
    return (status == FLASH_PROGRAM_OK) ? lock_status : status;
}

Flash_Status_t FlashProgram_Unlock(void){
    FLASH_Unlock();

    if((FLASH->CR & FLASH_CR_LOCK) != 0U){
        return FLASH_PROGRAM_LOCK_FAILED;
    }

    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

    return FLASH_PROGRAM_OK;
}

Flash_Status_t FlashProgram_Lock(void){
    FLASH_Lock();

    return ((FLASH->CR & FLASH_CR_LOCK) != 0U) ? FLASH_PROGRAM_OK : FLASH_PROGRAM_LOCK_FAILED;
}

Flash_Status_t FlashProgram_ErasePage(uint32_t page_addr){
    if(!FlashProgram_IsValidRange(page_addr, FLASH_PAGE_SIZE)){
        return FLASH_PROGRAM_OUT_OF_RANGE;
    }

    if((page_addr % FLASH_PAGE_SIZE) != 0U){
        return FLASH_PROGRAM_UNALIGNED;
    }

    Flash_Status_t status = FlashProgram_Unlock();
    if(status != FLASH_PROGRAM_OK){
        return status;
    }

    status = FlashProgram_MapSTStatus(FLASH_ErasePage(page_addr));
    if((status == FLASH_PROGRAM_OK) &&
        !FlashProgram_IsErased(page_addr, FLASH_PAGE_SIZE)){
        status = FLASH_PROGRAM_VERIFY_FAILED;
    }

    return FlashProgram_FinishTransaction(status);
}

Flash_Status_t FlashProgram_WriteHalfWord(uint32_t address, uint16_t data){
    if(!FlashProgram_IsValidRange(address, sizeof(data))){
        return FLASH_PROGRAM_OUT_OF_RANGE;
    }

    if((address % sizeof(data)) != 0U){
        return FLASH_PROGRAM_UNALIGNED;
    }

    Flash_Status_t status = FlashProgram_Unlock();
    if(status != FLASH_PROGRAM_OK){
        return status;
    }

    status = FlashProgram_WriteHalfWordUnlocked(address, data);
    return FlashProgram_FinishTransaction(status);
}

Flash_Status_t FlashProgram_WriteBuffer(uint32_t address, const uint8_t *data, uint32_t length){
    if((data == NULL) || (length == 0U)){
        return FLASH_PROGRAM_INVALID_ARGUMENT;
    }

    if(!FlashProgram_IsValidRange(address, length)){
        return FLASH_PROGRAM_OUT_OF_RANGE;
    }

    if((address % sizeof(uint16_t)) != 0U){
        return FLASH_PROGRAM_UNALIGNED;
    }

    const bool has_padding = (length % sizeof(uint16_t)) != 0U;
    if(has_padding){
        /* Byte padding cũng phải nằm trong writable range. */
        if(!FlashProgram_IsValidRange(address, length + 1U)){
            return FLASH_PROGRAM_OUT_OF_RANGE;
        }

        /* Padding 0xFF chỉ an toàn khi byte kế tiếp vẫn đang erased. */
        if(*(const volatile uint8_t *)(uintptr_t)(address + length) != 0xFFU){
            return FLASH_PROGRAM_PROGRAM_ERROR;
        }
    }

    Flash_Status_t status = FlashProgram_Unlock();
    if(status != FLASH_PROGRAM_OK){
        return status;
    }

    for(uint32_t i = 0U; i < length; i += sizeof(uint16_t)){
        uint16_t half_word = data[i];

        if((i + 1U) < length){
            half_word |= (uint16_t)((uint16_t)data[i + 1U] << 8U);
        } else {
            half_word |= 0xFF00U;
        }

        status = FlashProgram_WriteHalfWordUnlocked(address + i, half_word);
        if(status != FLASH_PROGRAM_OK){
            break;
        }
    }

    if(status == FLASH_PROGRAM_OK){
        status = FlashProgram_Verify(address, data, length);
    }

    return FlashProgram_FinishTransaction(status);
}

Flash_Status_t FlashProgram_Verify(uint32_t address, const uint8_t *expected, uint32_t length){
    if((expected == NULL) || (length == 0U)){
        return FLASH_PROGRAM_INVALID_ARGUMENT;
    }

    if(!FlashProgram_IsValidRange(address, length)){
        return FLASH_PROGRAM_OUT_OF_RANGE;
    }

    const volatile uint8_t *flash =
        (const volatile uint8_t *)(uintptr_t)address;

    for(uint32_t i = 0U; i < length; ++i){
        if(flash[i] != expected[i]){
            return FLASH_PROGRAM_VERIFY_FAILED;
        }
    }

    return FLASH_PROGRAM_OK;
}
