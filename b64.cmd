setlocal
cls
call config.cmd
@if not exist libq64.a (
    echo create libq64.a using make_libq.cmd in gyorokpeter/qutils
    exit /b 1
)
g++ -shared qtcp.cpp qtcpk.cpp -I%KX_KDB_PATH%/c/c -I%SD1MUX_PATH%/include -L. -lq64 -lws2_32 -lsd1mux64 -o qtcp_w64.dll -static --std=gnu++17
