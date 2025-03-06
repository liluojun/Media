@echo off
set NDK_PATH=G:\sdk\ndk\25.2.9519653\toolchains\llvm\prebuilt\windows-x86_64\bin
set SO_FILE=F:\git_media\Media\app\build\intermediates\merged_native_libs\debug\mergeDebugNativeLibs\out\lib\arm64-v8a\libmedie.so
set CRASH_ADDR=000000000002d250  # 替换为实际地址

%NDK_PATH%\llvm-addr2line -e %SO_FILE% %CRASH_ADDR%
pause
