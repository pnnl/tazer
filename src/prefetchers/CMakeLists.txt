set(PREFETCH_HEADERS
    ${TAZER_PREFETCH_INC_DIR}/DeltaPrefetcher.h
    ${TAZER_PREFETCH_INC_DIR}/PerfectPrefetcher.h
    ${TAZER_PREFETCH_INC_DIR}/Prefetcher.h
)

set(PREFETCH_FILES
    DeltaPrefetcher.cpp
    PerfectPrefetcher.cpp
    Prefetcher.cpp
)

add_library(prefetch SHARED ${PREFETCH_HEADERS} ${PREFETCH_FILES})
target_include_directories(prefetch PUBLIC ${TAZER_INC_DIR} ${TAZER_PREFETCH_INC_DIR})
