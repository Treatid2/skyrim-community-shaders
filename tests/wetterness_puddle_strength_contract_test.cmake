if(NOT DEFINED PROJECT_ROOT)
    get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()

file(READ "${PROJECT_ROOT}/package/Shaders/Lighting.hlsl" LIGHTING_SOURCE)
file(READ "${PROJECT_ROOT}/src/Features/Wetterness.cpp" WETTERNESS_SOURCE)

set(REQUIRED_LIGHTING_CONTRACTS
    "float puddleStrengthScale ="
    "CS_WETNESS_SETTINGS.MaxPuddleWetness * 0.25;"
    "puddle = puddleWetness * (puddleStrengthScale + 0.5) * puddleSlopeMask;"
    "puddleSignal = puddleSignal * puddleStrengthScale + 0.5;"
)

foreach(CONTRACT IN LISTS REQUIRED_LIGHTING_CONTRACTS)
    string(FIND "${LIGHTING_SOURCE}" "${CONTRACT}" CONTRACT_POSITION)
    if(CONTRACT_POSITION EQUAL -1)
        message(FATAL_ERROR "Missing puddle strength contract: ${CONTRACT}")
    endif()
endforeach()

set(REQUIRED_FALLBACK_CONTRACTS
    "effectivePuddleMaskMode == PuddleMaskMode::Textured"
    "effectivePuddleMaskMode == PuddleMaskMode::TexturedHighQuality"
    "effectivePuddleMaskMode = PuddleMaskMode::Simple;"
)

foreach(CONTRACT IN LISTS REQUIRED_FALLBACK_CONTRACTS)
    string(FIND "${WETTERNESS_SOURCE}" "${CONTRACT}" CONTRACT_POSITION)
    if(CONTRACT_POSITION EQUAL -1)
        message(FATAL_ERROR "Missing puddle fallback contract: ${CONTRACT}")
    endif()
endforeach()
