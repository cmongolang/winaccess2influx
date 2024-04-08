#pragma once
#include "winaccess2influx.h"
#include <chrono>
#include <thread>
#include <string>
#include <vector>
#include <mutex> 
#include <InfluxDBFactory.h>
#include "WvAPI.h"
#include "WvApiObj.h"
#include <boost/date_time.hpp>


struct waveSampleData {
    int                                         sampleRate{};
    int                                         numSamplesReturned{};
    float                                       unitMultiplier{};   // 5x for ECG, ScaleMax / SampleMax for others
    std::string                                 Label{};
    WV_WAVEFORM_ID                              WvWaveformID{};
    std::string                                 UOM{};
    WV_WAVEFORM_SAMPLE                          samples[WV_MAX_SAMPLES]{};
    std::chrono::system_clock::time_point       firstSampleUTC{};
    std::chrono::milliseconds                   sampleInterval{};
};

class WvBed {
private:
    bool                    includeMRN;
    bool                    ignoreBed;
    bool                    captureWaves;
    bool                    captureTrends;
    bool                    allTrends;
    bool                    allWaves;
    bool                    addParamOnAlarm;
    bool                    paramTextWanted;
    long                    waveSamplesSent;
    long                    trendValuesSent;
    std::thread             bed_Thread;
    std::mutex              bedInfoMutex;
    std::chrono::seconds    alarmQueryInterval{};
    std::chrono::seconds    trendQueryInterval{};
    std::chrono::seconds    waveQueryInterval{};
    std::chrono::milliseconds   serverBedTdiff{};
    std::chrono::steady_clock::time_point   nextAlarmQuery{};
    std::chrono::steady_clock::time_point   nextTrendQuery{};
    std::chrono::steady_clock::time_point   nextWaveQuery{};
    std::string             statusURL{};
    std::string             patID{};
    std::string             trendURL{};
    std::string             waveURL{};
    std::string             cfgTrendParamString;
    std::string             cfgWaveParamString;
    std::string             cfgTargetBeds;
    std::string             cfgTargetCU;
    std::string             firstValidTimeStr;
    std::string             influxBedLabel;
    boost::posix_time::ptime        connectTime{};
    boost::posix_time::ptime        firstValidSample{};
    boost::property_tree::ptree     bedCfg;
    WV_WAVEFORM_LIST                wavesAvailable{};
    WV_WAVEFORM_LIST                waveListFiltered{};
    int                             numFilteredWaves{};
    int                             numWavesAvailable{};
    WV_BED_DESCRIPTION_W            bedDesc{};
    std::chrono::milliseconds       tickDiffUTC{};
    std::vector<WV_PARAMETER_ID>    trendParamIDs{};
    std::vector<WV_WAVEFORM_ID>     waveParamIDs{};
    std::vector<waveSampleData>     waveSamples{};
    enum alarmMode { monitor, vent };
    struct trendStruct {
        int     numParamReturned{};
        WV_PARAMETER_LIST           unfilteredParamList{};
        WV_PARAMETER_DESCRIPTION_W  paramDesc{};
    };
    trendStruct trend{};
    void    checkIfBedSelected();
    void    connectBed();
    void    sendWaveSamples();
    void    grabHighestAlarm(alarmMode mode);
    void    filterWaveSamples();
    void    getWaveSamples();
    void    buildWaveFilter();
    void    applyWaveFilter();
    void    getDemographics();
    void    trendGrabber();
    void    updateTickOffset();
    void    bedLoop();
    void    updateAvailableWaves();
    void    processCfg();
    void    tokenizeParam();
    void    makeParamTextFile();
    void    disconnectBed();
    std::string sanitiseForInflux(std::string inString);

public:
    std::string     bedLabel{};
    std::atomic_bool InBedList_Atomic;

    void update(WV_BED_DESCRIPTION_W newBedInfo);
    WvBed(WV_BED_DESCRIPTION_W bedDescIn);
    ~WvBed();
};
