#pragma once
#include "WvApiObj.h"
#include "WvBed.h"

boost::property_tree::ptree WvApi::cfgTree{};

void WvApi::wvapi2influxinit(int MajorRev, int MinorRev) {
    int returnCode = WvStart(&MajorRev, &MinorRev);
    BOOST_LOG_TRIVIAL(info) << "Winacces API WvStart return Code " << WvApi::MapIntRetCodes.at(returnCode);
    if (returnCode != WV_SUCCESS) {
        BOOST_LOG_TRIVIAL(fatal) << "Failed to initialise Winaccess API, terminating";
        if (returnCode == WV_VERSION_MISMATCH) {
            BOOST_LOG_TRIVIAL(fatal) << "we're using API version " << WVAPI_MAJOR_REV << "." << WVAPI_MINOR_REV
                                     << "API DLL returned " << MajorRev << "." << MinorRev;
        }
        returnCode = WvStop();
        BOOST_LOG_TRIVIAL(info) << "WvStop return Code " << WvApi::MapIntRetCodes.at(returnCode);
        running = FALSE;
    }
}

void WvApi::deleteExpiredBeds() {
    for (auto bed = bedMap.cbegin(); bed != bedMap.cend();/*no increment*/)
    {
        if (bed->second->InBedList_Atomic == FALSE)
        {
            BOOST_LOG_TRIVIAL(info) << "dropping inactive bed " << bed->second->bedLabel << " From bedMap";
            bed = bedMap.erase(bed);
        }
        else
        {
            ++bed;
        }
    }
}

void WvApi::readConfig() {
    namespace fs = std::filesystem;
    auto dirPath = getenv("USERPROFILE") + std::string("\\.winaccess2influx");
    auto cfgFilePath = dirPath + std::string("\\winaccess.cfg");
    try {
        if (!fs::exists(dirPath)) {
            fs::create_directory(dirPath);
        }
        if (!fs::exists(cfgFilePath)) {
            BOOST_LOG_TRIVIAL(info) << "config file not found at " << cfgFilePath << " creating config from defaults";
            boost::property_tree::write_ini(cfgFilePath, cfgTree);
        }
    }
    catch (std::exception& e) {
        BOOST_LOG_TRIVIAL(fatal) << "Failed to write cfg file " << e.what();
        running = FALSE;
        return;
    }
    if (fs::exists(cfgFilePath)) {
        try {
            BOOST_LOG_TRIVIAL(info) << "Reading config " << cfgFilePath;
            boost::property_tree::read_ini(cfgFilePath, cfgTree);
        }
        catch (std::exception& e) {
            BOOST_LOG_TRIVIAL(fatal) << "Error reading in config file from " << cfgFilePath << e.what();
            running = FALSE;
            return;
        }
    }

    try {
        std::string host = cfgTree.get_child("Gateway.gwHost").get_value("");
        std::string user = cfgTree.get_child("Gateway.gwUser").get_value("");
        std::string pass = cfgTree.get_child("Gateway.gwPass").get_value("");

        gwHost = std::wstring(host.begin(), host.end());
        gwUser = std::wstring(user.begin(), user.end());
        gwPass = std::wstring(pass.begin(), pass.end());
    }
    catch (std::exception& e) {
        BOOST_LOG_TRIVIAL(fatal) << "Failed to process Gateway host details " << e.what();
        running = FALSE;
        return;
    }
}

void WvApi::buildDefaultIni() {
    std::string defaultTrendParam = " ECG_HR, SPO2_SAT, SPO2_PR, ART_D, ART_S, ART_M, MBUSX_RESP_ETCO2, MBUSX_RESP_FIO2, ";
    defaultTrendParam += "MBUSX_ETO2, MBUSX_ETN20, MBUSX_ETSEV, MBUSX_ETDES, MIB_BIS, MIB_SQI, MBUSX_RESP_VT, MBUSX_RESP_PIP, ";
    defaultTrendParam += "MBUSX_RESP_PEEP, MBUSX_RESP_MV, MIB_BIS, MIB_SQI, NIBP_S, NIBP_D, NIBP_M, TEMP_BASIC_A, TEMP_BASIC_B";
    cfgTree.put<std::string>("comment", "Config for winaccess2influx tool. This file will be recreated if deleted");
    cfgTree.put<std::string>("influxdb.waveURL", "http://localhost:8086/?db=waves");
    cfgTree.put<std::string>("influxdb.trendURL", "http://localhost:8086/?db=trends");
    cfgTree.put<std::string>("influxdb.statusURL", "http://localhost:8086/?db=status");
    cfgTree.put<std::string>("Gateway.gwHost", "localhost");
    cfgTree.put<std::string>("Gateway.gwUser", "gwuser");
    cfgTree.put<std::string>("Gateway.gwPass", "Welcome1!");
    cfgTree.put<std::string>("Gateway.pDomain", "localhost");
    cfgTree.put<std::string>("parameters.trends", defaultTrendParam);
    cfgTree.put<std::string>("parameters.waves", "ECG_LEAD_II, SPO2, ART, CVP");
    cfgTree.put<int>("timing.trendInterval", 5);
    cfgTree.put<int>("timing.alarmInterval", 1);
    cfgTree.put<int>("timing.waveInterval", 5);
    cfgTree.put<std::string>("source.targetBeds", "ALL");
    cfgTree.put<std::string>("source.targetCU", "ALL");
    cfgTree.put<bool>("includeMRN", TRUE);
    cfgTree.put<bool>("parameters.captureWaves", TRUE);
    cfgTree.put<bool>("parameters.captureTrends", TRUE);
    cfgTree.put<bool>("parameters.addParamToBedIfAlarming", TRUE);
    cfgTree.put<bool>("parameters.create_txt_of_avail_param", TRUE);
    cfgTree.put<std::string>("logLevel", "info");
}

WvApi::WvApi(char** argv) {
    buildDefaultIni();
    readConfig();
    wvapi2influxinit(WVAPI_MAJOR_REV, WVAPI_MINOR_REV);
}
WvApi::~WvApi() {
    int returnCode = WvStop();
    BOOST_LOG_TRIVIAL(trace) << "WvStop returnCode = " << returnCode;
}