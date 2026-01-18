setlocal
cls
call config.cmd
@if not exist libq.a (
    echo create libq.a using make_libq.cmd in gyorokpeter/qutils
    exit /b 1
)
g++ -shared qtcp.cpp qtcpk.cpp -I%KX_KDB_PATH%/c/c -I%SD1MUX_PATH%/include -L. -lq -lws2_32 -lsd1mux -o qtcp_w32.dll -static --std=gnu++17
