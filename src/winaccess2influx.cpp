#pragma once
#include "winaccess2influx.h"
#include "WvApiObj.h"

std::string utf8_encode(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);

    return strTo;
}
BOOL WINAPI CtrlHandler(DWORD fdwCtrlType) {
    switch (fdwCtrlType) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:

        BOOST_LOG_TRIVIAL(info) << "shutdown triggered";
        running = FALSE;
        
        return true;
    default:
        return false;
    }
}
int windowsAppInit() {
    if (!SetConsoleCtrlHandler(CtrlHandler, TRUE)) {    // Allow ctrl C to exit cleanly with WvStop
        printf("\nERROR: Could not set control handler");
        running = FALSE;
        return 1;
    }
    CreateMutex(NULL, TRUE, L"Winaccess2InfluxMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        BOOST_LOG_TRIVIAL(fatal) << "duplicate instance of winaccess2influx detected, terminating";
        running = FALSE;
        std::this_thread::sleep_for(std::chrono::milliseconds(3000));   
    }
    else {
        running = true;
    }
    return 0;
}

void setBoostLogLevel() {
    try {
        std::string levStr = WvApi::cfgTree.get_child("logLevel").get_value("");
        boost::to_lower(levStr);
        boost::replace_all(levStr, " ", "");
        boost::log::trivial::severity_level logSeverity;
        if (levStr == "fatal" || levStr == "error" || levStr == "warning" || levStr == "info" || levStr == "debug" || levStr == "trace") {
            std::istringstream{ levStr } >> logSeverity;    // Set loglevel from config
            boost::log::core::get()->set_filter(boost::log::trivial::severity >= logSeverity);
        }
        else  {
            BOOST_LOG_TRIVIAL(error) << "Failed to set Boost Log Level, invalid config";
        }
    }
    catch (std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Failed to set Boost Log Level " << e.what();
    }
}

std::string uniqueBedID(WV_BED_DESCRIPTION_W bedDesc) {
    std::string sn = utf8_encode(bedDesc.SerialNumber);
    std::string bedLabel = utf8_encode(bedDesc.BedLabel);
    return bedLabel + " sn:" + sn;
}
