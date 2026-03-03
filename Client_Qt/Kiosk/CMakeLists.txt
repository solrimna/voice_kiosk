# **빌드 과정:**
# 1. 소스 컴파일
# 2. 링킹
#    [대기 중... MANUAL_FINALIZATION 때문에]
# 3. 사이에 추가하고픈 작업들 실행
#    - DLL 복사
#    - 플러그인 추가
#    - 기타 설정
# 4. qt_finalize_executable 실행
#    - Qt 플러그인 정리
#    - 리소스 복사
# 5. 완성!
cmake_minimum_required(VERSION 3.16)

project(Kiosk VERSION 0.1 LANGUAGES CXX)

# 1. Qt 자동화 켜기
set(CMAKE_AUTOUIC ON)    # .ui(Qt Designer로 만든 UI) → C++ 헤더 자동 변환
                         # `mainwindow.ui` → `ui_mainwindow.h` 자동 생성
set(CMAKE_AUTOMOC ON)    # Q_OBJECT → 시그널/슬롯
                         # .h 파일에 Q_OBJECT 있으면 moc_*.cpp 자동 생성
set(CMAKE_AUTORCC ON)    # .qrc → 리소스
                         # 리소스를 실행파일에 내장(이미지 데이터를 C++ 배열로)

# 2. C++ 버전 설정
set(CMAKE_CXX_STANDARD 17)           # C++17 사용
set(CMAKE_CXX_STANDARD_REQUIRED ON)  # C++17 필수

# 3. Qt 버전 찾기 (Qt6 or Qt5)
# Qt6 6.5 고정형에서 5~6에서 실행할 수 있도록 수정함
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network Multimedia)
#            ↑ Qt6 우선, 없으면 Qt5

# 4. 실제 Qt 패키지 로드
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network Multimedia)
#            ↑ Qt6 또는 Qt5 (위에서 결정된 버전)

# ─── Azure Speech SDK 설정 ────────────────────────────────────────────
set(SPEECH_SDK_PATH "D:/Libs/SpeechSDK")

# Include 디렉토리 추가
include_directories(
    ${SPEECH_SDK_PATH}/include/cxx_api
    ${SPEECH_SDK_PATH}/include/c_api
)

# 라이브러리 경로 추가
link_directories(${SPEECH_SDK_PATH}/lib/x64)
# ──────────────────────────────────────────────────────────────────────

qt_standard_project_setup()

set(PROJECT_SOURCES     # PROJECT_SOURCES 변수 생성
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(Kiosk
        MANUAL_FINALIZATION # Qt6의 자동 최종화(finalization) 끄기
                            # Qt 플러그인 자동 추가와 리소스 자동 복사 사이에 추가하고 싶은게 있으므로
        ${PROJECT_SOURCES}
    )
else()
    add_executable(Kiosk
        ${PROJECT_SOURCES}
    )
endif()

target_link_libraries(Kiosk
    PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Multimedia
    Microsoft.CognitiveServices.Speech.core
)

# ─── DLL 파일 복사 (실행 시 필요) ────────────────────────────────────
add_custom_command(TARGET Kiosk POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SPEECH_SDK_PATH}/bin/x64/Microsoft.CognitiveServices.Speech.core.dll"
        $<TARGET_FILE_DIR:Kiosk>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SPEECH_SDK_PATH}/bin/x64/Microsoft.CognitiveServices.Speech.extension.audio.sys.dll"
        $<TARGET_FILE_DIR:Kiosk>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SPEECH_SDK_PATH}/bin/x64/Microsoft.CognitiveServices.Speech.extension.codec.dll"
        $<TARGET_FILE_DIR:Kiosk>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SPEECH_SDK_PATH}/bin/x64/Microsoft.CognitiveServices.Speech.extension.kws.dll"
        $<TARGET_FILE_DIR:Kiosk>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SPEECH_SDK_PATH}/bin/x64/Microsoft.CognitiveServices.Speech.extension.kws.ort.dll"
        $<TARGET_FILE_DIR:Kiosk>
)
# ──────────────────────────────────────────────────────────────────────
if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(Kiosk)
endif()

# 나중에 다 만들면 손대기(배포용)
# include(GNUInstallDirs) # 표준 설치 경로 변수 제공(CMAKE_INSTALL_BINDIR, CMAKE_INSTALL_LIBDIR)

# install(TARGETS Kiosk   # 빌드된 파일을 설치 위치로 복사
#     BUNDLE  DESTINATION .
#     RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
#     LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
# )

# qt_generate_deploy_app_script(  # Qt 배포용 스크립트 자동 생성
#     TARGET Kiosk
#     OUTPUT_SCRIPT deploy_script
#     NO_UNSUPPORTED_PLATFORM_ERROR
# )
# install(SCRIPT ${deploy_script})
