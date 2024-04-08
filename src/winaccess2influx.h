#pragma once
#include <windows.h>
#include <map>
#include <mutex> 
#include <stdexcept>
#include <stdlib.h>
#include <string>
#include <vector>
#include <iostream>
#include <filesystem>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>
#include "WvAPI.h"  // Draeger Winaccess DLL header file

extern std::atomic_bool running;
std::string uniqueBedID(WV_BED_DESCRIPTION_W bedDesc);
std::string utf8_encode(const std::wstring& wstr);
BOOL WINAPI CtrlHandler(DWORD fdwCtrlType);
int windowsAppInit();
void setBoostLogLevel();