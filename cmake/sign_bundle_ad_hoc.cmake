if (NOT DEFINED ENTITLEMENTS OR NOT EXISTS "${ENTITLEMENTS}")
    message(FATAL_ERROR "Entitlements file not found: ${ENTITLEMENTS}")
endif()

if (NOT DEFINED BUNDLE_PATH OR NOT EXISTS "${BUNDLE_PATH}")
    message(FATAL_ERROR "Bundle target not found: ${BUNDLE_PATH}")
endif()

execute_process(
    COMMAND /usr/bin/codesign --force --deep --sign -
            --entitlements "${ENTITLEMENTS}"
            --options runtime
            "${BUNDLE_PATH}"
    RESULT_VARIABLE codesign_result
)

if (NOT codesign_result EQUAL 0)
    message(WARNING "Code signing failed, but build continues")
endif()
