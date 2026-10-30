# ====================================================================
# Compiler-specific flags — auto-detected from CMAKE_CXX_COMPILER_ID
# 优先级: Clang(clang-cl) > MSVC(cl.exe) > GCC > fallback
# ====================================================================

# ── Clang (including clang-cl on Windows) ──
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    if(WIN32)
        # clang-cl on Windows — MSVC-compatible flags
        add_compile_options(
            /utf-8
            /bigobj
            /W4
            /wd4324
            /wd4244
            /wd4267
            /EHsc
        )
        add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
        add_compile_definitions(_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)
        add_compile_definitions(_WIN32_WINNT=0x0A00)
        # clang-cl 只有 Windows-Clang 的 Embedded 映射（CMake 未提供 ProgramDatabase 的
        # clang 编译选项），Debug 由 Embedded CodeView 经链接器 /debug /pdb:<TARGET_PDB>
        # 汇总为外部 godot-autopilot.pdb，deploy 再随 DLL 一并复制。
        set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug>:Embedded>" CACHE INTERNAL "")
        message(STATUS "[gda] clang-cl flags applied")
    else()
        add_compile_options(
            -Wall -Wextra -Wpedantic
            -Wno-unused-parameter
        )
        if(NOT GDA_IS_CI)
            add_compile_options(-march=native)
        endif()
        message(STATUS "[gda] Clang flags applied")
    endif()

# ── MSVC (cl.exe only) ──
elseif(MSVC)
    add_compile_options(
        /utf-8
        /bigobj
        /W4
        /wd4324
        /wd4244
        /wd4267
        /EHsc
        # lambda 捕获的符合性开关，CMake 默认未启用。缺它时 MSVC 拒绝
        # 「lambda 经 [&] 隐式引用捕获局部 const/constexpr 变量」并报 error C3493，
        # 而 clang 接受同一写法。该开关拉齐两者，使同一份源码在 clang-cl 与
        # MSVC 下都能编译。
        /Zc:lambda
    )
    add_compile_definitions(
        _CRT_SECURE_NO_WARNINGS
        _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
        _WIN32_WINNT=0x0A00
    )
    # Debug 落外部程序数据库（/Zi + PDB）供崩溃符号化与 T-04 复现；Release
    # 保持 Embedded（/Z7），不新增发布期符号产物。
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
        "$<IF:$<CONFIG:Debug>,ProgramDatabase,Embedded>" CACHE INTERNAL "")
    message(STATUS "[gda] MSVC flags applied")

# ── GNU GCC ──
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
    )
    if(NOT GDA_IS_CI)
        add_compile_options(-march=native)
    endif()
    message(STATUS "[gda] GCC flags applied")

else()
    message(WARNING "[gda] Unknown compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()

# ── Threads — required for std::thread on macOS/Clang (Unity builds) ──
include(FindThreads)
find_package(Threads REQUIRED)

# ── PIC ──
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
