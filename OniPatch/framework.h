#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclure les en-têtes Windows rarement utilisés
// Fichiers d'en-tête Windows
#include <windows.h>
#include <iostream>
#include <detours.h>
#include <d3d9helper.h>
#include <stdio.h>
#include <wchar.h>
#include <tchar.h>
#include <deque>
#include <vector>

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <Xinput.h>

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "Xinput.lib")
