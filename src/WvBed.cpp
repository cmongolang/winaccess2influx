#pragma once
#include "WvBed.h"

void WvBed::processCfg() {
    try {
        includeMRN = bedCfg.get_child("includeMRN").get_value<bool>();
        alarmQueryInterval = std::chrono::seconds(bedCfg.get_child("timing.alarmInterval").get_value<int>());
        trendQueryInterval = std::chrono::seconds(bedCfg.get_child("timing.trendInterval").get_value<int>());
        waveQueryInterval = std::chrono::seconds(bedCfg.get_child("timing.waveInterval").get_value<int>());
        captureWaves = bedCfg.get_child("parameters.captureWaves").get_value<bool>();
        captureTrends = bedCfg.get_child("parameters.captureTrends").get_value<bool>();
        cfgTrendParamString = bedCfg.get_child("parameters.trends").get_value("");
        cfgWaveParamString = bedCfg.get_child("parameters.waves").get_value("");
        cfgTargetBeds = bedCfg.get_child("source.targetBeds").get_value("");
        cfgTargetCU = bedCfg.get_child("source.targetCU").get_value("");
        trendURL = bedCfg.get_child("influxdb.trendURL").get_value("");
        waveURL = bedCfg.get_child("influxdb.waveURL").get_value("");
        statusURL = bedCfg.get_child("influxdb.statusURL").get_value("");
        paramTextWanted = bedCfg.get_child("parameters.create_txt_of_avail_param").get_value<bool>();
        addParamOnAlarm = bedCfg.get_child("parameters.addParamToBedIfAlarming").get_value<bool>();

        boost::to_upper(cfgTrendParamString);
        boost::to_upper(cfgWaveParamString);
        boost::to_upper(cfgTargetBeds);
        boost::to_upper(cfgTargetCU);
        boost::replace_all(cfgTrendParamString, " ", "");
        boost::replace_all(cfgWaveParamString, " ", "");
        boost::replace_all(cfgTargetBeds, " ", "");
        boost::replace_all(cfgTargetCU, " ", "");
        boost::replace_all(trendURL, " ", "");
        boost::replace_all(waveURL, " ", "");
        boost::replace_all(statusURL, " ", "");
    }
    catch (std::exception& e) {
        BOOST_LOG_TRIVIAL(fatal) << "Failed to process bed config " << e.what() << "\n";
        running = false;
        return;
    }
    // apply minimum 1 second intervals
    if (alarmQueryInterval < std::chrono::seconds(1)) { alarmQueryInterval = std::chrono::seconds(1); }
    if (trendQueryInterval < alarmQueryInterval) { trendQueryInterval = alarmQueryInterval; }
    if (waveQueryInterval < alarmQueryInterval) { waveQueryInterval = alarmQueryInterval; }
}

void WvBed::tokenizeParam() {
    allTrends = FALSE;
    allWaves = FALSE;
    boost::char_separator<char> sep(",");
    boost::tokenizer<boost::char_separator<char>> trendTok(cfgTrendParamString, sep);
    for (boost::tokenizer<boost::char_separator<char>>::iterator beg = trendTok.begin(); beg != trendTok.end(); ++beg) {
        std::string trendLabel = *beg;
        auto it = WvApi::MapTrend.find(trendLabel); // Match string against enum, store parameter ID to config struct
        if (it != WvApi::MapTrend.end()) {
            trendParamIDs.push_back(it->second);
        }
        else if (trendLabel == "ALL") {
            allTrends = TRUE;   // don't push "ALL" to cfg.trendParamIDs
        }
        else {
            BOOST_LOG_TRIVIAL(warning) << "invalid trend parameter in config !! " << *beg;
        }
    }
    boost::tokenizer<boost::char_separator<char>> waveTok(cfgWaveParamString, sep);
    for (boost::tokenizer<boost::char_separator<char>>::iterator beg = waveTok.begin(); beg != waveTok.end(); ++beg) {
        std::string waveLabel = *beg;
        auto it = WvApi::MapWvf.find(waveLabel);    // Match string against enum, store parameter ID to config struct
        if (it != WvApi::MapWvf.end()) {
            waveParamIDs.push_back(it->second);
        }
        else if (waveLabel == "ALL") {
            allWaves = TRUE;
        }
        else {
            BOOST_LOG_TRIVIAL(warning) << "invalid Wave parameter in config !! " << *beg;
        }
    }
}

void WvBed::checkIfBedSelected() {
    bool careUnitMatch = FALSE;
    bool bedMatch = FALSE;
    std::string careUnit = utf8_encode(bedDesc.CareUnit);
    std::string bedLabel = utf8_encode(bedDesc.BedLabel);
    boost::to_upper(careUnit);
    boost::to_upper(bedLabel);
    boost::erase_all(careUnit, " ");
    boost::erase_all(bedLabel, " ");

    boost::char_separator<char> sep(",");       // split string of beds from config file, check against this bed label
    boost::tokenizer< boost::char_separator<char> > bedTok(cfgTargetBeds, sep);
    for (boost::tokenizer< boost::char_separator<char> >::iterator beg = bedTok.begin(); beg != bedTok.end(); ++beg)
    {
        std::string tempBedLabel = *beg;
        if (tempBedLabel == bedLabel || tempBedLabel == "ALL") {
            bedMatch = TRUE;
            break;
        }
    }
    boost::tokenizer< boost::char_separator<char> > cuTok(cfgTargetCU, sep);
    for (boost::tokenizer< boost::char_separator<char> >::iterator beg = cuTok.begin(); beg != cuTok.end(); ++beg)
    {
        std::string tempCuLabel = *beg;
        if (tempCuLabel == careUnit || tempCuLabel == "ALL") {
            careUnitMatch = TRUE;
            break;
        }
    }

    if (bedMatch && careUnitMatch) {
        ignoreBed = FALSE;
    }
    else {
        ignoreBed = TRUE;
        BOOST_LOG_TRIVIAL(info) << bedLabel << " online but not selected in config so ignoring";
    }
}

// Connect if monitoring and not already connected
void WvBed::connectBed() {
    if (bedDesc.DeviceStatus == WV_OPERATING_MODE_MONITORING && bedDesc.ConnectID == 0) {
        WV_REQUEST_TYPE connectMode;
        if (captureWaves) { connectMode = WV_REQUEST_ALARMS_PARAMS_AND_WAVES; }
        else { connectMode = WV_REQUEST_ALARMS_AND_PARAMS; }

        int returnCode = WvConnectEx(bedDesc.dirEntryId, &bedDesc.ConnectID, WV_SDC_PREFERRED, connectMode);
        if (returnCode == WV_SUCCESS) {
            connectTime = boost::posix_time::second_clock::local_time();
            //BOOST_LOG_TRIVIAL(info) << "connectBed succeeded " << bedLabel << " connectID " << bedDesc.ConnectID;
            throw (returnCode);  // Throw to delay first calls to bed
        }
        else {
            BOOST_LOG_TRIVIAL(error) << "connectBed failed for " << bedLabel;
            throw (returnCode);
        }
    }
}

// Build wave filter as subset of avaiable waves per config
void WvBed::buildWaveFilter() {
    numFilteredWaves = 0;
    for (int i = 0; i < numWavesAvailable; i++) {
        if (allWaves || count(waveParamIDs.begin(), waveParamIDs.end(), wavesAvailable.WvWaveforms[i])) {
            waveListFiltered.WvWaveforms[numFilteredWaves] = wavesAvailable.WvWaveforms[i];
            numFilteredWaves += 1;
        }
    }
}

void WvBed::applyWaveFilter() {
    int returnCode = WvSetFilter(bedDesc.ConnectID, &waveListFiltered, &numFilteredWaves);
    if (returnCode != 0) {
        BOOST_LOG_TRIVIAL(error) << "WvSetFilter failed on bed " << bedLabel;
        throw (returnCode);
    }
}

// send waveform samples for all waveforms in waveSamples vector to influxDB
void WvBed::sendWaveSamples() {
    unsigned long int localSampleCount = 0;
    try {
        auto influxdb = influxdb::InfluxDBFactory::Get(waveURL);
        influxdb::Point::floatsPrecision = 2;
        influxdb->batchOf(5000);    // optimal influxdb batch size
        std::string bedLabWaves = influxBedLabel + "_waves";
        for (auto wave : waveSamples) {
            localSampleCount += wave.numSamplesReturned;
            for (int i = 0; i < wave.numSamplesReturned; i++) {
                if (firstValidSample.is_not_a_date_time()) {
                    firstValidSample = boost::posix_time::second_clock::local_time();
                    firstValidTimeStr = boost::posix_time::to_simple_string(firstValidSample);
                    boost::replace_all(firstValidTimeStr, " ", "_");
                }

                influxdb->write(influxdb::Point{ bedLabWaves }
                    .addTag("patID", patID)
                    .addTag("wave-label", wave.Label)
                    .addField("first-Valid-Data", firstValidTimeStr)
                    .addField("unit", wave.UOM)
                    .addField("unitMuliplier", wave.unitMultiplier)
                    .addField("value", wave.samples[i])
                    .setTimestamp(wave.firstSampleUTC + i * wave.sampleInterval)
                );
            }
        }
        influxdb->flushBatch();
        waveSamplesSent = localSampleCount;
    }

    catch (influxdb::InfluxDBException& e) {
        BOOST_LOG_TRIVIAL(error) << "Influx error in sendWaveSamples " << bedLabel << " " << e.what();
    }
    waveSamples.clear();    //Reset the vector of wave samples
}

// Query waveform samples for all waveforms in waveListFiltered
void WvBed::getWaveSamples() {
    if (bedDesc.DeviceStatus == WV_OPERATING_MODE_MONITORING && captureWaves && bedDesc.ConnectID) {
        for (int i = 0; i < numFilteredWaves; i++) {
            waveSampleData              waveData;
            WV_WAVEFORM_DESCRIPTION_W   waveDescriptionFromApi;
            int64_t                     tickTimeFirstSampFromApi{};
            int32_t                     FirstSampleTimestamp_fromAPI;
            unsigned int                FirstSampleSequenceNumber;
            int                         msecDiff;

            // scalemax divided by sample max
            int returnCode = WvDescribeWaveform_W(bedDesc.ConnectID, waveListFiltered.WvWaveforms[i], &waveDescriptionFromApi);
            if (returnCode != 0) {
                throw (returnCode);
            }
            try {
                waveData.Label = utf8_encode(waveDescriptionFromApi.Label);
                boost::replace_all(waveData.Label, " ", "-");           // Remove spaces for writing to influxDB
                waveData.UOM = WvApi::MapUOM.find(waveDescriptionFromApi.Units)->second;
                waveData.sampleRate = std::stoi(waveDescriptionFromApi.SampleRate);
                waveData.sampleInterval = std::chrono::milliseconds(1000 / waveData.sampleRate);
                waveData.WvWaveformID = waveDescriptionFromApi.WvWaveformID;
                if (waveData.WvWaveformID < WV_WAVE_ECG_LEAD_V6) {
                    waveData.unitMultiplier = 5;    // All ECG waves, 5 uV per unit from API
                }
                else {
                    waveData.unitMultiplier = std::stof(waveDescriptionFromApi.ScaleMax) / std::stof(waveDescriptionFromApi.SampleMax);
                }
            }
            catch (std::exception& e) {
                BOOST_LOG_TRIVIAL(error) << "unexpected error processing wave description" << e.what();
                throw;
            }

            returnCode = WvGetWaveformSamples(bedDesc.ConnectID, waveListFiltered.WvWaveforms[i], waveData.samples, WV_MAX_SAMPLES,
                &waveData.numSamplesReturned, &FirstSampleTimestamp_fromAPI, &FirstSampleSequenceNumber,
                &msecDiff, &tickTimeFirstSampFromApi);
            if (returnCode != WV_SUCCESS) {
                throw (returnCode);
            }
            auto serverTimeFirstSample = std::chrono::system_clock::now() - (waveData.numSamplesReturned * waveData.sampleInterval);
            waveData.firstSampleUTC = std::chrono::system_clock::time_point(std::chrono::milliseconds(tickTimeFirstSampFromApi) + tickDiffUTC);
            if (waveData.numSamplesReturned > 200) {
                serverBedTdiff = std::chrono::duration_cast<std::chrono::milliseconds>(serverTimeFirstSample - waveData.firstSampleUTC);
            }
            
            if (serverBedTdiff > std::chrono::seconds(120)) {
                BOOST_LOG_TRIVIAL(warning) << "Difference between bed clock and server time " << serverBedTdiff.count() << "ms " << bedLabel;
            }
            waveSamples.push_back(waveData);
        }
    }
}

// Query list of all available waveforms for this bed
void WvBed::updateAvailableWaves() {
    int returnCode = WvListAvailableWaveforms(bedDesc.ConnectID, &wavesAvailable, &numWavesAvailable);
    if (returnCode != 0) {
        numWavesAvailable = 0;
        throw (returnCode);
    }
}

// Queries current alarm status and uses that data to calculate offset between bed tickCount and bed Localtime
// This offset will persist until disconnection of bed to prevent waveform distortion if bedside realtime clock shifts.
// ticktime is a continuous millisecond counter in the bedside monitor
void WvBed::updateTickOffset() {
    if (bedDesc.ConnectID != 0 && tickDiffUTC.count() == 0) {
        WV_ALARM_INFO_W     AlarmInfo;
        int returnCode = WvGetHighestGradeAlarm_W(bedDesc.ConnectID, &AlarmInfo);
        if ( returnCode == WV_SUCCESS) {
            long long timeFromAlarm = AlarmInfo.AlarmTimeStamp;
            tickDiffUTC = std::chrono::milliseconds(timeFromAlarm * 1000 - AlarmInfo.AlarmTickTimeStamp);
        }
    }
}

// Function to filter SPO2 wave if all zeros, or other invalid wave samples value '-32768' from API
void WvBed::filterWaveSamples() {
    for (auto& wave : waveSamples) {
        // Remove SPO2 wave if all zeros, eg no probe attached
        bool noValidSamples = TRUE;
        // If all samples are either -32768 or 0, set numSamplesReturned to 0  
        for (int i = 0; i < wave.numSamplesReturned; i++) {
            if (wave.samples[i] == -32768) {
                wave.samples[i] = 0;
            }
            if (wave.samples[i] != 0) {
                noValidSamples = FALSE;
            }
        }
        if (noValidSamples) {
            wave.numSamplesReturned = 0;    // cheaper than erasing from vector
            BOOST_LOG_TRIVIAL(debug) << "No valid samples in " << wave.Label << " " << bedLabel;
        }
    }
}

void WvBed::grabHighestAlarm(alarmMode mode) {
    WV_ALARM_INFO_W     AlarmInfo;
    if (mode == monitor) {
        int returnCode = WvGetHighestGradeAlarm_W(bedDesc.ConnectID, &AlarmInfo);
        if (returnCode != WV_SUCCESS) {
            BOOST_LOG_TRIVIAL(trace) << "Failed to get Alarm info from " << bedLabel;
            throw (returnCode);
        }
    }
    else if ( mode == vent ){
        int returnCode = WvGetHighestGradeVentAlarm_W(bedDesc.ConnectID, &AlarmInfo);
        if ( returnCode != WV_SUCCESS  ) {
           // BOOST_LOG_TRIVIAL(trace) << "No vent Alarm data for " << bedLabel <<
             //   " returnCode " << WvApi::MapIntRetCodes.find(returnCode)->second;
            return; // Don't throw on Vent data failure as maybe no vent connected
        }
    }

    if (AlarmInfo.AlarmState != WV_ALARM_STATE_NOT_ACTIVE) {
        try {
            auto influxdb = influxdb::InfluxDBFactory::Get(trendURL);
            influxdb::Point::floatsPrecision = 2;
            std::chrono::system_clock::time_point timeStamp{};
            timeStamp += std::chrono::milliseconds(AlarmInfo.AlarmTickTimeStamp) + tickDiffUTC;
            std::string alarmState = WvApi::MapAlarmState.find(AlarmInfo.AlarmState)->second;
            std::string alarmMessage = utf8_encode(AlarmInfo.AlarmMessage);
            std::string alarmGrade = WvApi::MapAlarmGrade.find(AlarmInfo.AlarmGrade)->second;
            std::string alarmParam = "none";
            if (AlarmInfo.WvParameterID != WV_PARAM_INVALID) {
                int returnCode = WvDescribeParameter_W(bedDesc.ConnectID, AlarmInfo.WvParameterID, &trend.paramDesc);
                if (returnCode == WV_SUCCESS) {
                    alarmParam = utf8_encode(trend.paramDesc.Label);
                }
                // Check if alarming parameter is configured for trend recording, if not add it
                if (addParamOnAlarm && count(trendParamIDs.begin(), trendParamIDs.end(), AlarmInfo.WvParameterID) == 0) {
                    trendParamIDs.push_back(AlarmInfo.WvParameterID);
                }
            }

            if (firstValidSample.is_not_a_date_time()) {
                firstValidSample = boost::posix_time::second_clock::local_time();
                firstValidTimeStr = boost::posix_time::to_simple_string(firstValidSample);
                boost::replace_all(firstValidTimeStr, " ", "_");
            }
            if (mode == monitor) {
                influxdb->write(influxdb::Point{ influxBedLabel + "_alarms" }
                    .addTag("First-Valid", firstValidTimeStr)
                    .addTag("patID", patID)
                    .addField("Alarm-Grade", alarmGrade)
                    .addField("Alarm-Message", alarmMessage)
                    .addField("Alarm-Parameter", alarmParam)
                    .addField("Alarm-State", alarmState)
                    .setTimestamp(timeStamp));
                influxdb->flushBatch();
            }
            if (mode == vent) {
                influxdb->write(influxdb::Point{ influxBedLabel + "-Alarms" }
                    .addTag("patID", patID)
                    .addTag("First-Valid", firstValidTimeStr)
                    .addField("VentAlarm-Grade", alarmGrade)
                    .addField("VentAlarm-Message", alarmMessage)
                    .addField("VentAlarm-Parameter", alarmParam)
                    .addField("VentAlarm-State", alarmState)
                    .setTimestamp(timeStamp));
                influxdb->flushBatch();
            }
        }
        catch (influxdb::InfluxDBException& E) {
            BOOST_LOG_TRIVIAL(error) << " Influx error in AlarmGrabber " << E.what();
        }
    }
}

void WvBed::makeParamTextFile() {
    if ( paramTextWanted && numWavesAvailable ) {
        paramTextWanted = FALSE;    // only run once for life of ConnectID
        auto path = getenv("USERPROFILE") + std::string("\\.winaccess2influx\\paramRef");
        if (!std::filesystem::exists(path)) {
            std::filesystem::create_directories(path);
        }
        std::string input;
        input += "First Valid data since connection at " + firstValidTimeStr + "\n\n";
        input += "Valid loglevel values = fatal, error, warning, info, debug, trace\n\n";
        input += "Available Waveform parameters for " + bedLabel + "\n";
        for (int i = 0; i < numWavesAvailable; i++) {
            for (auto it : WvApi::MapWvf) {
                if (it.second == wavesAvailable.WvWaveforms[i]) {
                    input += (it.first + std::string(", "));
                    break;
                }
            }
        }

        input += "\n\nAvailalable trend parameters for bed " + bedLabel + "\n";
        for (int i = 0; i < trend.numParamReturned; i++) {
            for (auto it : WvApi::MapTrend) {
                if (it.second == trend.unfilteredParamList.WvParameters[i]) {
                    input += (it.first + std::string(", "));
                    break;
                }
            }
        }

        input += "\n\nComplete list of valid Waveform parameters\n";
        for (auto waveParam : WvApi::MapWvf) {
            input += waveParam.first + std::string(",\n");
        }

        input += "\n\nComplete list of valid trend parameters\n";
        for (auto trendParam : WvApi::MapTrend){
            input += trendParam.first + std::string(",\n");
            }

        auto filename = path + std::string("\\avail-Param-") + bedLabel + std::string(".txt");
        try {
            std::ofstream out(filename);
            out << input;
            out.close();
        }
        catch (std::exception& e) {
            BOOST_LOG_TRIVIAL(warning) << "Error created available trends txt for bed " << bedLabel << e.what();
        }
    }
}

void WvBed::bedLoop() {
    while (InBedList_Atomic && !ignoreBed && running ) {
        auto loopStart = std::chrono::steady_clock::now();
        auto nextLoopStart = loopStart + alarmQueryInterval;
        if ( bedDesc.ConnectID == 0) { nextLoopStart += std::chrono::seconds(3); }    // Soft start on first connect
        if (bedDesc.DeviceStatus == WV_OPERATING_MODE_MONITORING) {          
            /// START MUTEX ///////////////
            bedInfoMutex.lock();    // Prevent main calling update until loop completes
            try {
                connectBed();                   // throws on fail and first connect
                updateTickOffset();             // Must run for timestamp calculation, throw on fail
                getDemographics();              // checks for demographics enabled status in config
                grabHighestAlarm(monitor);      // 
                grabHighestAlarm(vent);         //

                ////////////////// WAVES ////////////////////////////////
                if (std::chrono::steady_clock::now() > nextWaveQuery) {
                    nextWaveQuery = std::chrono::steady_clock::now() + waveQueryInterval;
                    if (bedDesc.ConnectID != 0 && captureWaves) {
                        updateAvailableWaves();
                        buildWaveFilter();
                        applyWaveFilter();
                        getWaveSamples();
                        filterWaveSamples();    // delete blocks of all invalid or SPO2 all 0
                        sendWaveSamples();
                    }
                }
                //////////////////// TRENDS /////////////////////////////
                if (std::chrono::steady_clock::now() > nextTrendQuery) {
                    nextTrendQuery = std::chrono::steady_clock::now() + trendQueryInterval;
                    if (bedDesc.ConnectID != 0 && captureTrends) {
                        trendGrabber();
                        makeParamTextFile();    // run once for life of ConnectID
                    }

                }
            }
            catch (int returnCode) {
                // ConnectBed throws on first connection with WV_SUCCESS if succesful
                if (returnCode == WV_SUCCESS) {
                    BOOST_LOG_TRIVIAL(trace) << "connected " << bedLabel << " connectID " << bedDesc.ConnectID << " " << "returnCode WV_SUCCESS";
                }
                else if (returnCode == WV_PATIENT_DISCHARGED) {
                    disconnectBed();
                    // Delay next loop start beyond next WvListBeds in main loop
                    nextLoopStart += std::chrono::seconds(4);
                }
                else {
                    BOOST_LOG_TRIVIAL(debug) << "Data capture error " << bedLabel << " returnCode " << WvApi::MapIntRetCodes.at(returnCode);
                    nextLoopStart += std::chrono::seconds(4);   // delay next loop start on error to wait for another bedlist update
                }
                
            }
            //////////////////////////  END MUTEX   ///////////////
            bedInfoMutex.unlock();
        }
       
        auto busyTime = std::chrono::steady_clock::now() - loopStart;
        BOOST_LOG_TRIVIAL(trace) << bedLabel << " busy time " << std::chrono::duration_cast<std::chrono::milliseconds>(busyTime).count() << " ms";
        std::this_thread::sleep_until(nextLoopStart);
    }
}

void WvBed::disconnectBed() {
    BOOST_LOG_TRIVIAL(trace) << "Disconnecting discharged " << bedLabel;
    int returnCode = WvDisconnect(bedDesc.ConnectID);
    BOOST_LOG_TRIVIAL(trace) << "WvDisconnect returnCode " << WvApi::MapIntRetCodes.find(returnCode)->second;
    if (returnCode == WV_SUCCESS) {
        bedDesc.ConnectID = 0; // don't wait for next bedlist
        bedDesc.DeviceStatus = WV_OPERATING_MODE_DISCHARGE; // Set now to prevent reconnect attempt before bedlist updates
    }
    firstValidSample = boost::posix_time::ptime();  // Reset first sample timestamps
    firstValidTimeStr = "";
    serverBedTdiff = std::chrono::milliseconds(0);
}

// This function called by main to copy latest global bedList data into each bed object
// If the bed is busy might be locked, but busy means working so don't care if this is skipped.
void WvBed::update(WV_BED_DESCRIPTION_W updatedBedInfo) {
    if (bedInfoMutex.try_lock()) {
        bedDesc = updatedBedInfo;
        bedLabel = utf8_encode(bedDesc.BedLabel);
        if (bedDesc.DeviceStatus == WV_OPERATING_MODE_DISCHARGE && bedDesc.ConnectID) {
            disconnectBed();
        }
        // Ensure first sample timestamp is reset for disconnected bed
        if (!firstValidSample.is_not_a_date_time() && bedDesc.ConnectID == 0) {
            firstValidSample = boost::posix_time::ptime();
            firstValidTimeStr = "";
        }
        influxBedLabel = bedLabel + +"_sn_" + utf8_encode(bedDesc.SerialNumber);
        std::string bedStatus = WvApi::MapOpMode.find(bedDesc.DeviceStatus)->second;
        if (bedDesc.Wireless) {
            influxBedLabel += "-wireless";
        }
        std::string bedLabStatus = influxBedLabel + "_status";
        std::string devType = utf8_encode(bedDesc.DeviceType);
        std::string MRN = patID;
        long long currentWaveSampCount = waveSamplesSent;
        bool bedIgnored = ignoreBed;
        int conID = bedDesc.ConnectID;
        int tdiffms = serverBedTdiff.count();

        // Safe to unlock now before influx write if we only access local variables
        bedInfoMutex.unlock();
        
        try {
            auto influxdb = influxdb::InfluxDBFactory::Get(statusURL);
            influxdb->write(influxdb::Point{ bedLabStatus }
                .addTag("MRN", MRN)
                .addField("Bed-Ignored", bedIgnored)
                .addField("bed-Status", bedStatus)
                .addField("ConnectID", conID)
                .addField("device-type", devType)
                .addField("ms_diff_bed_to_server", tdiffms)
                .addField("Wave-Samples-Sent", currentWaveSampCount)
            );
            influxdb->flushBatch();
        }
        catch (influxdb::InfluxDBException& E) {
            BOOST_LOG_TRIVIAL(error) << " Influx error in update status " << bedLabel << E.what();
        }
        waveSamplesSent = 0;    // reset count
    }
    else {
        BOOST_LOG_TRIVIAL(trace) << "bed info update blocked by Mutex " << bedLabel;
    }
}

WvBed::WvBed(WV_BED_DESCRIPTION_W bedDescIn) {
    InBedList_Atomic = TRUE;
    bedCfg = WvApi::cfgTree;
    bedDesc = bedDescIn;        // Later bedDesc updates called from main via update function
    bedLabel = utf8_encode(bedDesc.BedLabel);
    processCfg();
    tokenizeParam();
    checkIfBedSelected();   // sets ignore bed flag if bed not selected in config

    // If bed is selected per care unit and bed, launch thread in this object to gather and export data
    if (!ignoreBed) {
        update(bedDescIn);
        bed_Thread = std::thread(&WvBed::bedLoop, this);
    }
}

WvBed::~WvBed() {
    InBedList_Atomic = false;
    int returnCode = WvDisconnect(bedDesc.ConnectID);
    if (bed_Thread.joinable()) {
        bed_Thread.join();
    }
}

void WvBed::getDemographics() {
    if (includeMRN) {
        patID = sanitiseForInflux(utf8_encode(bedDesc.PatientID));
    }
    if (includeMRN == FALSE || patID.empty()) {
        patID = "no-MRN";
    }
}

void WvBed::trendGrabber() {
    try{
        auto influxdb = influxdb::InfluxDBFactory::Get(trendURL);
        influxdb->batchOf(WV_MAX_PARAMETERS_PER_BED);   //must flush before destructor called
        influxdb::Point::floatsPrecision = 2;

        int returnCode = WvListParameters(bedDesc.ConnectID, &trend.unfilteredParamList, &trend.numParamReturned);
        if (returnCode != 0) {
            BOOST_LOG_TRIVIAL(debug) << "WvListParameters Failed for " << bedLabel;
            throw (returnCode);
        }
        for (int i = 0; i < trend.numParamReturned; i++) {
            if (allTrends || count(trendParamIDs.begin(), trendParamIDs.end(), trend.unfilteredParamList.WvParameters[i])) {
                returnCode = WvDescribeParameter_W(bedDesc.ConnectID, trend.unfilteredParamList.WvParameters[i], &trend.paramDesc);
                if (returnCode != WV_SUCCESS) {
                    BOOST_LOG_TRIVIAL(error) << "WvDescribe parameter Failed, bed " << bedLabel;
                    throw (returnCode);
                }
                std::string paramLabel = sanitiseForInflux(utf8_encode(trend.paramDesc.Label));

                if (trend.paramDesc.isSetting) {
                    paramLabel += "-setting";
                }
                std::string UOM = WvApi::MapUOM.find(trend.paramDesc.Units)->second;
                float value{};
                try {
                    value = std::stof(trend.paramDesc.Value);
                }
                // Expect exception on null value, break loop on NULL
                catch (const std::invalid_argument& e) {
                    BOOST_LOG_TRIVIAL(debug) << e.what() << " maybe null param value " << bedLabel << " " << paramLabel;
                    continue;
                }
                catch (const std::out_of_range& e) {
                    BOOST_LOG_TRIVIAL(debug) << e.what() << " maybe null param value" << bedLabel << " " << paramLabel;
                    continue;
                }
                if (firstValidSample.is_not_a_date_time()) {
                    firstValidSample = boost::posix_time::second_clock::local_time();
                    firstValidTimeStr = boost::posix_time::to_simple_string(firstValidSample);
                    boost::replace_all(firstValidTimeStr, " ", "_");
                }

                std::chrono::system_clock::time_point timeStamp{};
                timeStamp += (std::chrono::milliseconds(trend.paramDesc.ValueTickTimeStamp) + tickDiffUTC);

                influxdb->write(influxdb::Point{ influxBedLabel + "_trends" }
                    .addTag("First-Valid-Data", firstValidTimeStr)
                    .addTag("paramLabel", paramLabel)
                    .addTag("patID", patID)
                    .addField("unit", UOM)
                    .addField("value", value)
                    .addField("WvParamID", trend.paramDesc.WvParameterID)
                    .setTimestamp(timeStamp)
                );
            }
        }
        influxdb->flushBatch();
    }
    catch (influxdb::InfluxDBException& E) {
        BOOST_LOG_TRIVIAL(error) << " Influx error in trendGrabber " << E.what();
    }
}

// Remove spaces, '=', commas for use with influxDB
std::string WvBed::sanitiseForInflux(std::string inString) {
    boost::replace_all(inString, " ", "-");
    boost::replace_all(inString, ",", "");
    boost::replace_all(inString, "=", "");
    return inString;
}