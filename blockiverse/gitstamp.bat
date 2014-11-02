@echo off
echo "/*" >auto/version.h
echo "** AUTOMATICALLY GENERATED -- DO NOT AHND EDIT" >>auto/version.h
echo "**" >>auto/version.h
echo "*/" >>auto/version.h
git describe --always --dirty
