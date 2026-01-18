setlocal
cls
@call config.cmd
set "PATH=%MINGW64_PATH%;%PATH%
copy %KX_KDB_PATH%\w64\q.lib .\q64.lib
echo LIBRARY q.exe >q64.def
echo EXPORTS>>q64.def
bash -c "nm -p q64.lib|egrep 'T [^.]'|sed 's/0* T //'" >>q64.def
dlltool -v -l libq64.a -d q64.def
dlltool -v -l libsd1mux64.a -d %SD1MUX_PATH%\def\sd1mux64.def
