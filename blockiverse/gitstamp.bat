@echo off
echo /*>auto/version.h
echo **>>auto/version.h
echo ** AUTOMATICALLY GENERATED -- DO NOT HAND EDIT>>auto/version.h
echo **>>auto/version.h
echo ** version.h: automatic version string>>auto/version.h
echo **>>auto/version.h
echo */>>auto/version.h
echo.>>auto/version.h
for /f "delims=" %%r in ('git describe --always --dirty') do set verstamp=%%r
echo const char *auto_ver="%verstamp%";>>auto/version.h
