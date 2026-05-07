if (NOT DEFINED BUNDLE_PATH OR NOT EXISTS "${BUNDLE_PATH}")
    message(FATAL_ERROR "Bundle target not found: ${BUNDLE_PATH}")
endif()

set(codesign_args
    /usr/bin/codesign
    --force
    --deep
    --sign -
    --options runtime)

if (DEFINED ENTITLEMENTS AND NOT "${ENTITLEMENTS}" STREQUAL "")
    if (NOT EXISTS "${ENTITLEMENTS}")
        message(FATAL_ERROR "Entitlements file not found: ${ENTITLEMENTS}")
    endif()

    list(APPEND codesign_args --entitlements "${ENTITLEMENTS}")
endif()

execute_process(
    COMMAND ${codesign_args} "${BUNDLE_PATH}"
    RESULT_VARIABLE codesign_result
)

if (NOT codesign_result EQUAL 0)
    message(FATAL_ERROR "Code signing failed for: ${BUNDLE_PATH}")
endif()
