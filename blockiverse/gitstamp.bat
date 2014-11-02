@echo off
git rev-parse --abbrev-ref HEAD
git log -1 --format=%%h
