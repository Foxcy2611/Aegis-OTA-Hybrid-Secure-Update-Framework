# =============================================================================
# MỤC ĐÍCH CỦA FILE NÀY:
# - Cung cấp một "Hàm" (function) tên là `aegis_configure_firmware`.
# - Hàm này sẽ bóp nặn file chạy mặc định của máy tính (output executable) thành file ELF/BIN/HEX chuẩn của vi điều khiển.
# - Nó gài sẵn các lệnh tiện ích như: in dung lượng RAM/Flash, dọn rác, và nạp code (flash).
#
# File này không tạo Bootloader/Application target.
# CMakeLists.txt của từng firmware phải gọi add_executable() trước, sau đó gọi:
#
# aegis_configure_firmware(
#     <target>
#     <linker-script>
# )
# =============================================================================


# =============================================================================
# MẸO LÁCH LUẬT "SCRIPT MODE" ĐỂ TẠO FILE ASSEMBLY (.LST)
# =============================================================================
# 1. Mục đích: 
#    Ta cần tạo ra file .lst (chứa mã Assembly dịch ngược) để con người đọc và gỡ lỗi 
#    (còn chip thì chỉ quan tâm file .bin/.hex).
#
# 2. Vấn đề: 
#    Lệnh trên terminal thường dùng là: `objdump -S file.elf > file.lst`
#    Tuy nhiên, hệ thống Ninja build rất nhanh vì nó không chạy ngầm qua CMD hay Bash. 
#    Do đó, nó KHÔNG HIỂU dấu `>` (chuyển hướng đầu ra) là gì và sẽ báo lỗi.
#
# 3. Cách giải quyết (Đoạn if dưới đây):
#    - Dùng mẹo ép CMake tự gọi lại CHÍNH FILE NÀY như một script độc lập.
#    - Khi lọt vào đoạn if, nó sẽ dùng lệnh `execute_process` với tính năng OUTPUT_FILE 
#      để hứng chữ và xuất ra file .lst (thay thế hoàn hảo cho dấu `>`).
#    - Xong việc, gặp lệnh `return()` nó sẽ thoát ngay lập tức để không chạy phần cấu hình bên dưới.
# =============================================================================
if(DEFINED AEGIS_HELPER_MODE AND AEGIS_HELPER_MODE STREQUAL "OBJDUMP")

    # 1. Kiểm tra an toàn: Đảm bảo Ninja đã truyền đủ tool vào đây .
    # (Thiếu tool objdump, thiếu file đầu vào ELF, hoặc thiếu tên file đầu ra LST là báo rồi thoát ngay)
    if(NOT DEFINED AEGIS_OBJDUMP OR AEGIS_OBJDUMP STREQUAL "")
        message(FATAL_ERROR
            "Script objdump thiếu biến AEGIS_OBJDUMP"
        )
    endif()

    if(NOT DEFINED AEGIS_INPUT OR AEGIS_INPUT STREQUAL "")
        message(FATAL_ERROR
            "Script objdump thiếu biến AEGIS_INPUT"
        )
    endif()

    if(NOT DEFINED AEGIS_OUTPUT OR
       AEGIS_OUTPUT STREQUAL "")
        message(FATAL_ERROR
            "Script objdump thiếu biến AEGIS_OUTPUT"
        )
    endif()

    # Lệnh này chính là sự thay thế cho: arm-none-eabi-objdump -h -S file.elf > file.lst (Từ elf -> assembly)
    execute_process(
        COMMAND "${AEGIS_OBJDUMP}" -h -S "${AEGIS_INPUT}"
        
        # Hứng toàn bộ chữ in ra màn hình và GHI THẲNG vào file LST (thay cho dấu >)
        OUTPUT_FILE "${AEGIS_OUTPUT}"

        # Nếu có lỗi, nhét thông báo lỗi vào biến objdump_error
        ERROR_VARIABLE objdump_error

        # Lưu mã trạng thái (0 là thành công, 1 là fail) vào biến objdump_result
        RESULT_VARIABLE objdump_result
    )
    # 3. Kiểm tra xem lệnh vừa chạy có lỗi không.
    if(NOT objdump_result EQUAL 0)
        message(FATAL_ERROR "Không thể tạo disassembly:\n${objdump_error}")
    endif()
    return() # Xong việc thì thoát luôn, không chạy phần dưới nữa.
endif()

# Ngăn file bị include và định nghĩa lại function nhiều lần trong cùng build tree.
include_guard(GLOBAL)

# =============================================================================
# 2. CẤU HÌNH CÔNG CỤ NẠP CODE (OPENOCD)
# =============================================================================

# Tìm phần mềm OpenOCD trên máy tính 
find_program(AEGIS_OPENOCD openocd)

# Đặt mặc định dùng mạch nạp ST-Link, nạp cho chip STM32F1, với tốc độ 400kHz.
# Chữ CACHE STRING cho phép đổi các thông số này sau này nếu đổi mạch nạp khác (vd J-Link).
set(AEGIS_OPENOCD_INTERFACE "interface/stlink.cfg" CACHE STRING "OpenOCD interface config")
set(AEGIS_OPENOCD_TARGET    "target/stm32f1x.cfg"  CACHE STRING "OpenOCD target config")
set(AEGIS_OPENOCD_SPEED     "400"                  CACHE STRING "OpenOCD adapter speed")

# =============================================================================
# 3. HÀM ĐÓNG GÓI CHÍNH (aegis_configure_firmware)
# =============================================================================

function(aegis_configure_firmware target linker_script)
    # --- BƯỚC 1: KIỂM TRA ĐẦU VÀO ---
    # Chắc chắn rằng 'target' là một file chạy (EXECUTABLE) và tìm ra đường dẫn tuyệt đối của Linker Script.
    if(NOT TARGET "${target}")
        message(FATAL_ERROR
            "Không tồn tại firmware target '${target}'. "
            "Phải gọi add_executable() trước aegis_configure_firmware()."
        )
    endif()

    get_target_property(target_type "${target}" TYPE)

    if(NOT target_type STREQUAL "EXECUTABLE")
        message(FATAL_ERROR
            "Target '${target}' phải là EXECUTABLE, hiện tại là '${target_type}'."
        )
    endif()

    get_filename_component(linker_script_absolute "${linker_script}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

    if(NOT EXISTS "${linker_script_absolute}")
        message(FATAL_ERROR
            "Không tìm thấy linker script: ${linker_script_absolute}"
        )
    endif()

    foreach(required_tool CMAKE_OBJCOPY CMAKE_OBJDUMP CMAKE_SIZE)
        if(NOT DEFINED ${required_tool} OR "${${required_tool}}" STREQUAL "")
            message(FATAL_ERROR
                "Toolchain chưa khai báo ${required_tool}."
            )
        endif()
    endforeach()

    # --- BƯỚC 2: CHUẨN BỊ SẴN TÊN CÁC OUTPUT ---
    # Cấu hình sẵn tên các file sinh ra và nằm trong build/
    set(output_directory "${CMAKE_CURRENT_BINARY_DIR}")
    set(map_file "${output_directory}/${target}.map") # Hóa đơn báo cáo chi tiết vùng nhớ
    set(bin_file "${output_directory}/${target}.bin") # File nhị phân thuần túy để nạp OTA/Bootloader
    set(hex_file "${output_directory}/${target}.hex") # File nạp có chứa địa chỉ 
    set(lst_file "${output_directory}/${target}.lst") # File dịch ngược ra Assembly
    set(helper_script "${CMAKE_CURRENT_FUNCTION_LIST_FILE}")

    # --- BƯỚC 3: ÉP KHUÔN FILE ELF ---
    set_target_properties("${target}" PROPERTIES
        SUFFIX ".elf" # Đuôi file gốc phải là .elf

        # Bảo NINJA: Hễ sửa Linker Script thì phải build lại
        LINK_DEPENDS "${linker_script_absolute}"
    )

    # --- BƯỚC 4: LUẬT LẮP RÁP (LINKER OPTIONS) ---
    target_link_options("${target}" PRIVATE
        "-T${linker_script_absolute}" # Dùng Linker Script này để xếp chỗ cho code.
        -nostartfiles                 # Bỏ qua các hàm khởi động mặc định của HĐH máy tính (vì ta chạy bare-metal).
        -static                       # Gom mọi thứ vào 1 cục (không gọi DLL/SO bên ngoài).
        --specs=nano.specs            # Xài thư viện C siêu nhỏ gọn (newlib-nano) để tiết kiệm Flash.
        --specs=nosys.specs           # Tắt các hàm gọi hệ thống ảo (như printf ra màn hình) vì chip không có màn hình.
        -Wl,--gc-sections             # Rất quan trọng: Hàm nào viết ra mà không ai gọi, linker sẽ GỌT BỎ để giảm dung lượng.
        -Wl,--fatal-warnings          # Cảnh báo lúc link cũng coi là lỗi, cấm build!
        -Wl,--cref                    # Tạo bảng tra cứu chéo.
        -Wl,--print-memory-usage      # In ngay ra màn hình tổng số % Flash và RAM đã dùng sau khi build.
        "-Wl,-Map=${map_file}"        # Xuất hóa đơn chi tiết vị trí từng biến ra file .map.
    )

    # --- BƯỚC 5: NHÚNG THƯ VIỆN C CƠ BẢN ---
    target_link_libraries("${target}" PRIVATE
        # Khóa --start-group và --end-group bắt Linker lặp đi lặp lại việc quét 3 thư viện (C, Toán học, GCC)
        # để gỡ rối tình trạng "thằng này gọi thằng kia vòng tròn" (dependency vòng).
        "-Wl,--start-group"
        c
        m
        gcc
        "-Wl,--end-group"
    )

    # --- BƯỚC 6: TỰ ĐỘNG ĐẺ RA BIN/HEX (POST-BUILD) ---
    # Dặn CMake: "Cứ build xong file ELF là CMake tự động chạy 3 lệnh này".
    add_custom_command(
        TARGET "${target}" POST_BUILD
        COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:${target}>" "${bin_file}" # Bóc ELF ra BIN
        COMMAND "${CMAKE_OBJCOPY}" -O ihex "$<TARGET_FILE:${target}>" "${hex_file}"   # Bóc ELF ra HEX
        COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:${target}>"                            # Báo cáo dung lượng
        BYPRODUCTS "${bin_file}" "${hex_file}"
        COMMENT "Sinh BIN/HEX và kiểm tra size: ${target}"
        VERBATIM
    )

    # --- BƯỚC 7: Target size riêng ---
    add_custom_target("${target}_size"

        COMMAND
            "${CMAKE_SIZE}"
            "$<TARGET_FILE:${target}>"

        DEPENDS
            "${target}"

        COMMENT
            "Kích thước firmware: ${target}"

        USES_TERMINAL
        VERBATIM
    )

    
    # --- BƯỚC 8: Target disassembly riêng ---
    add_custom_target("${target}_disasm"

        COMMAND
            "${CMAKE_COMMAND}"
            "-DAEGIS_HELPER_MODE=OBJDUMP"
            "-DAEGIS_OBJDUMP=${CMAKE_OBJDUMP}"
            "-DAEGIS_INPUT=$<TARGET_FILE:${target}>"
            "-DAEGIS_OUTPUT=${lst_file}"
            -P
            "${helper_script}"

        DEPENDS
            "${target}"

        BYPRODUCTS
            "${lst_file}"

        COMMENT
            "Tạo disassembly: ${lst_file}"

        USES_TERMINAL
        VERBATIM
    )

    # --- BƯỚC 9: DỌN RÁC ---
    # Dặn "ninja clean" xóa luôn mấy file .map .bin .hex
    set_property(
        TARGET "${target}" APPEND
        PROPERTY ADDITIONAL_CLEAN_FILES "${map_file}" "${bin_file}" "${hex_file}" "${lst_file}"
    )

    # --- BƯỚC 10: TẠO NÚT "NẠP CODE" ---
    if(AEGIS_OPENOCD)
        # Nếu tìm thấy OpenOCD ở bước 2, tạo ra lệnh `ninja <tên_app>_flash`.
        # Gõ lệnh này là OpenOCD tự cắm cáp ST-Link, nạp file, tự kiểm tra (verify), và tự reset chip cho nó chạy!
        add_custom_target("${target}_flash"
            COMMAND 
                "${AEGIS_OPENOCD}" 
                -f "${AEGIS_OPENOCD_INTERFACE}" 
                -f "${AEGIS_OPENOCD_TARGET}"
                -c "adapter speed ${AEGIS_OPENOCD_SPEED}"
                -c "program {$<TARGET_FILE:${target}>} verify reset exit"
            DEPENDS 
                "${target}"
            COMMENT 
                "Flash ${target} bằng OpenOCD"
            
            USES_TERMINAL 
            VERBATIM
        )
        # Lệnh dự phòng: Nạp code trong lúc đè nút Reset cứng trên mạch (dành cho lúc chip bị treo).
        # ... (Tương tự ở trên nhưng có thêm lệnh reset_config) ...
        add_custom_target(
            "${target}_flash_connect_under_reset"

            COMMAND
                "${AEGIS_OPENOCD}"
                -f "${AEGIS_OPENOCD_INTERFACE}"
                -f "${AEGIS_OPENOCD_TARGET}"
                -c "adapter speed 100"
                -c
                "reset_config srst_only srst_nogate connect_assert_srst"
                -c
                "program {$<TARGET_FILE:${target}>} verify reset exit"

            DEPENDS
                "${target}"

            COMMENT
                "Flash ${target} bằng connect-under-reset"

            USES_TERMINAL
            VERBATIM
        )

    else()

        message(STATUS
            "Không tìm thấy OpenOCD; "
            "không tạo flash target cho ${target}."
        )

    endif()

    # --- BƯỚC 11: Báo output khi configure ---
    message(STATUS
        "Firmware target    : ${target}"
    )

    message(STATUS
        "Linker script      : ${linker_script_absolute}"
    )

    message(STATUS
        "ELF/BIN/HEX/MAP/LST: ${output_directory}"
    )

endfunction()
