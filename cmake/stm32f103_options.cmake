# =============================================================================
# MỤC ĐÍCH CỦA FILE NÀY:
# - Chứa tất cả các "cấu hình gốc" cho chip STM32F103 (thư viện, cờ biên dịch, phần cứng).
# - File này KHÔNG tạo ra file chạy (.hex/.bin). Nó giống như một "bản thiết kế".
# - Các dự án con (như Bootloader hay App) chỉ cần "link" với file này là tự động
#   thừa kế mọi thông số chuẩn, không cần bạn phải tự tay viết lại từ đầu.
# =============================================================================

include_guard(GLOBAL)

# ====================================================
# 1. Khai báo đường dẫn thư viện lõi của hãng
# ====================================================

# Link dẫn tới CMSIS và SPL
set(STM32F103_CMSIS_CORE_DIR
    "${PROJECT_SOURCE_DIR}/libraries/CMSIS/CM3/CoreSupport"
)

set(STM32F103_CMSIS_DEVICE_DIR
    "${PROJECT_SOURCE_DIR}/libraries/CMSIS/CM3/DeviceSupport"
)

set(STM32F103_SPL_INCLUDE_DIR
    "${PROJECT_SOURCE_DIR}/libraries/STM32F10x_StdPeriph_Driver/inc"
)

set(STM32F103_SPL_CONFIG_DIR
    "${PROJECT_SOURCE_DIR}/libraries/Template"
)

# Kiểm tra đường dẫn có tồn tại hay không
foreach(required_directory
    "${STM32F103_CMSIS_CORE_DIR}"
    "${STM32F103_CMSIS_DEVICE_DIR}"
    "${STM32F103_SPL_INCLUDE_DIR}"
    "${STM32F103_SPL_CONFIG_DIR}"
)
    if(NOT IS_DIRECTORY "${required_directory}")
        message(FATAL_ERROR
            "Không tìm thấy thư mục STM32 bắt buộc: "
            "${required_directory}"
        )
    endif()

endforeach()

# ====================================================
# 2. Cấu hình MCU dùng chung
# ====================================================

# Tạo 1 thư viện ảo (INTERFACE) tên là stm32f103_options
# Tác dụng gom các cấu hình
add_library(stm32f103_options INTERFACE)

# Header CMSIS và SPL ép dùng chuẩn C11
target_compile_features(stm32f103_options INTERFACE
    c_std_11
)

# Truyền đường dẫn các file .h của STM32 cho bất cứ target nào dùng stm32f103_options
# SYSTEM: Nhắc compiler rằng đây là code hãng ST, nếu có thừa, xấu thì compiler sẽ không quan tâm
target_include_directories(stm32f103_options SYSTEM INTERFACE
    "${STM32F103_CMSIS_CORE_DIR}"
    "${STM32F103_CMSIS_DEVICE_DIR}"
    "${STM32F103_SPL_INCLUDE_DIR}"
    "${STM32F103_SPL_CONFIG_DIR}"
)

# Tự động chèn các macro này vào mọi C
# Tương tự như #define ở dòng đầu các file code
target_compile_definitions(stm32f103_options INTERFACE
    $<$<COMPILE_LANGUAGE:C>:STM32F10X_MD>
    $<$<COMPILE_LANGUAGE:C>:USE_STDPERIPH_DRIVER>
    $<$<COMPILE_LANGUAGE:C>:HSE_VALUE=8000000UL> # 8 MHz
)

# Các cờ biên dịch (Chỉ định cách compiler biên dịch code C thành mã máy)
target_compile_options(stm32f103_options INTERFACE
    $<$<COMPILE_LANGUAGE:C,ASM>:-mcpu=cortex-m3>  # Khai báo dòng chip
    $<$<COMPILE_LANGUAGE:C,ASM>:-mthumb>          # Khai báo tập lệnh
    $<$<COMPILE_LANGUAGE:C,ASM>:-mfloat-abi=soft> # F103 không có FPU

    # Đóng gói từng hàm/biến vào 1 section riêng lẻ
    # Nhờ thế mà lúc link mà hàm không gọi đến sẽ tự vứt bỏ để tiết kiệm
    $<$<COMPILE_LANGUAGE:C>:-ffunction-sections>
    $<$<COMPILE_LANGUAGE:C>:-fdata-sections>

    # Chặn lỗi định nghĩa trùng
    $<$<COMPILE_LANGUAGE:C>:-fno-common>

    # Báo cáo bộ nhớ Stack (RAM) bị tiêu thụ bởi mỗi hàm để theo dõi tránh tràn bộ nhớ.
    $<$<COMPILE_LANGUAGE:C>:-fstack-usage>

    # Firmware final-first phải fit đúng partition cả ở Debug. Giữ symbol -g3
    # nhưng dùng -Os; một số local variable có thể bị tối ưu khi debug.
    $<$<AND:$<COMPILE_LANGUAGE:C>,$<CONFIG:Debug>>:-Os>
    $<$<AND:$<COMPILE_LANGUAGE:C,ASM>,$<CONFIG:Debug>>:-g3>

    # Khi build cấu hình RELEASE: Ép dung lượng code sinh ra phải "NHỎ NHẤT" (-Os).
    # Đồng thời bật -flto để Compiler tối ưu hóa toàn cục mức độ sâu nhất.
    $<$<AND:$<COMPILE_LANGUAGE:C>,$<CONFIG:Release>>:-Os>
    $<$<AND:$<COMPILE_LANGUAGE:C,ASM>,$<CONFIG:Release>>:-g3>
    $<$<AND:$<COMPILE_LANGUAGE:C>,$<CONFIG:Release>>:-flto>
)

# Các cờ liên kết (Chỉ định cách Linker chắp vá các mảnh mã máy lại với nhau)
target_link_options(stm32f103_options INTERFACE
    -mcpu=cortex-m3
    -mthumb
    -mfloat-abi=soft

    # Phải dặn Linker bật -flto tương ứng nếu lúc compile (phía trên) đã dùng -flto.
    $<$<CONFIG:Release>:-flto>
)

# ====================================================
# 3. Quy định lại viết code
# ====================================================

# Tạo thêm một "túi" ảo nữa, chỉ chứa các quy tắc bắt lỗi.
# Mục đích: Code của hãng ST (phía trên) thì bỏ qua, nhưng code của cá nhân viết thì phải thật sạch.
add_library(aegis_warnings INTERFACE)

target_compile_options(aegis_warnings INTERFACE
    $<$<COMPILE_LANGUAGE:C>:-Wall>   # Bật các cảnh báo phổ biến
    $<$<COMPILE_LANGUAGE:C>:-Wextra> # Bật thêm các cảnh báo nâng cao
    $<$<COMPILE_LANGUAGE:C>:-Werror> # Cực kỳ nghiêm ngặt: Coi tất cả các cảnh báo (warning) như là LỖI. Có warning là cấm build!
)
