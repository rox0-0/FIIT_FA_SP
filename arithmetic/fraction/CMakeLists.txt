# Создаем основную библиотеку fraction
add_library(
        mp_os_arthmtc_frctn
        src/fraction.cpp
)

# Указываем include директории
target_include_directories(
        mp_os_arthmtc_frctn
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        # Добавьте другие необходимые пути
)

# Линкуем зависимости
target_link_libraries(
        mp_os_arthmtc_frctn
        PUBLIC
        mp_os_allctr_allctr
        mp_os_arthmtc_bg_intgr
)

# Подключаем тесты
add_subdirectory(tests)