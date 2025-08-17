setlocal
cls
call config.cmd
@if not exist libq64.a (
    echo create libq64.a using make_libq.cmd in gyorokpeter/qutils
    exit /b 1
)
g++ -shared qtcp.cpp qtcpk.cpp selectable-socketpair/socketpair.c -I%KX_KDB_PATH%/c/c -L. -lq64 -lws2_32 -o qtcp_w64.dll -static --std=gnu++17
