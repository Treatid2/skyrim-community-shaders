if(NOT DEFINED PROJECT_ROOT)
    message(FATAL_ERROR "PROJECT_ROOT is required")
endif()

file(READ "${PROJECT_ROOT}/src/Features/Upscaling.cpp" _source)
file(READ "${PROJECT_ROOT}/src/Features/Upscaling.h" _header)

function(require_text content needle description)
    string(FIND "${content}" "${needle}" _position)
    if(_position EQUAL -1)
        message(FATAL_ERROR "FSR main-pass fallback contract missing: ${description}")
    endif()
endfunction()

require_text(
    "${_header}"
    "enum class MainPassUpscaleResult : uint8_t"
    "typed main-pass completion result"
)
require_text(
    "${_source}"
    "runtimeResolutionPlan.upscaleMethod == UpscaleMethod::kFSR &&\n\t\tresult == MainPassUpscaleResult::Deferred"
    "FSR Deferred must leave dynamic resolution available to the fallback"
)

string(FIND "${_source}" "// Preserve the normal full color-upscaling path in VR" _main_start)
string(FIND "${_source}" "void Upscaling::SetScissorRect::thunk" _main_end)
if(_main_start EQUAL -1 OR _main_end EQUAL -1 OR _main_end LESS_EQUAL _main_start)
    message(FATAL_ERROR "FSR main-pass fallback contract cannot isolate Main_PostProcessing")
endif()
math(EXPR _main_length "${_main_end} - ${_main_start}")
string(SUBSTRING "${_source}" ${_main_start} ${_main_length} _main_path)

require_text(
    "${_main_path}"
    "mainPassResult == MainPassUpscaleResult::Deferred"
    "main-pass Deferred branch"
)
require_text(
    "${_main_path}"
    "BSImagespaceShaderISTemporalAA->taaEnabled = true;\n\t\t\tfunc(a_this, a3, a_target, a_4, a_5);"
    "vanilla temporal fallback before returning"
)
require_text(
    "${_main_path}"
    "BSImagespaceShaderISTemporalAA->taaEnabled = false;\n\t\t\treturn;"
    "deterministic TAA state after fallback"
)
