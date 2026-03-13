# xyzpan_ui: INTERFACE library (no sources yet — OpenGL sources added in Plan 06-02)
# PositionBridge.h is header-only; expose ui/ directory for #include "PositionBridge.h"
add_library(xyzpan_ui INTERFACE)
target_include_directories(xyzpan_ui
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
)
