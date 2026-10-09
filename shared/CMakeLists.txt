# =============================================================================
# MỤC ĐÍCH CỦA FILE NÀY:
# - Tạo target header-only chứa contract dùng chung giữa Bootloader/Application.
# - Công khai đúng thư mục shared/ để các target include memory_map.h và các ABI.
# - Không biên dịch source và không đưa shared/cu tham khảo vào production.
# =============================================================================

add_library(aegis_shared INTERFACE)
add_library(Aegis::Shared ALIAS aegis_shared)

target_include_directories(aegis_shared INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_features(aegis_shared INTERFACE
    c_std_11
)
