setlocal
cls
call config.cmd
@if not exist libq.a (
    echo create libq.a using make_libq.cmd in gyorokpeter/qutils
    exit /b 1
)
g++ -shared qtcp.cpp qtcpk.cpp selectable-socketpair/socketpair.c -I%KX_KDB_PATH%/c/c -L. -lq -lws2_32 -o qtcp.dll -static --std=gnu++17
