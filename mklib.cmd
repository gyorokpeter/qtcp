setlocal
cls
@call config.cmd
set "PATH=%MINGW32_PATH%;%PATH%
copy %KX_KDB_PATH%\w32\q.lib .\q.lib
echo LIBRARY q.exe >q.def
echo EXPORTS>>q.def
bash -c "nm -p q.lib|egrep 'T [^.]'|sed 's/0* T _//'" >>q.def
dlltool -v -l libq.a -d q.def
dlltool -v -l libsd1mux.a -d %SD1MUX_PATH%\def\sd1mux.def
