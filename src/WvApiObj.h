#pragma once
#include "winaccess2influx.h"
#include "WvAPI.h"
#include "WvBed.h"

class WvBed;

class WvApi {
private:
    void wvapi2influxinit(int MajorRev, int MinorRev);
    void readConfig();
    void buildDefaultIni();
    void readIni();
public:
    std::wstring gwHost;
    std::wstring gwUser;
    std::wstring gwPass;
    std::wstring pDomain;

    static boost::property_tree::ptree cfgTree;
    static std::map<std::string, WV_WAVEFORM_ID> MapWvf;
    static std::map<std::string, WV_PARAMETER_ID> MapTrend;
    static std::map<WV_NET_UNITS_OF_MEASURE, std::string> MapUOM;
    static std::map<int, std::string> MapIntRetCodes;
    static std::map<WV_ALARM_STATE, std::string> MapAlarmState;
    static std::map<WV_ALARM_GRADE, std::string> MapAlarmGrade;
    static std::map<WV_OPERATING_MODE, std::string> MapOpMode;
    std::map<const std::string, std::shared_ptr<WvBed> >    bedMap;     // Map of WvBed objects against sn or bed label
    WV_BED_LIST_W       BedList;
    void deleteExpiredBeds();
    WvApi(char** argv);
    ~WvApi();
};