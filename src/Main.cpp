#pragma once
// WVAPI2Influx.cpp 
//
// Building in VS 2019, using vcpkg for dependencies
// from vcpkg package influxdb-cxx, lineprotocol.cxx has been modified to use milliseconds
// C++ 17
//
#define W_CLIENT
#define WVDLLVERSION    // Set unicode for Draeger Winacces API
#include "winaccess2influx.h"
#include "WvApiObj.h"
#include "WvBed.h"

std::atomic_bool running;   // flag to end primary while loop

int main(int argc, char** argv) {
    windowsAppInit();       // handle ctrl-c etc, ensure only single instance of this program
    BOOST_LOG_TRIVIAL(info) << "Start\n\n Starting winacccess2Influx tool\n\n Not validated for diagnostic purposes\n benn.blessing@pm.me\n ctrl-c to exit" ;
    static WvApi* pWvApi = new WvApi(argv);             // reads in config and initialises WvAPI and list for bed objects
    setBoostLogLevel();

    while (running) {
        auto loopStartTime = std::chrono::steady_clock::now();
        auto nextLoopStart = loopStartTime + std::chrono::seconds(5);
        int NumberOfBeds{};
        pWvApi->deleteExpiredBeds();

        int ReturnCode = WvListBeds_W(pWvApi->gwHost.c_str(), pWvApi->gwUser.c_str(), pWvApi->gwPass.c_str(), &pWvApi->BedList, &NumberOfBeds, pWvApi->pDomain.c_str(), WV_SDC_PREFERRED);
        if (ReturnCode != 0) { 
            if (ReturnCode == WV_NOT_INITIALIZED) {
                running = FALSE;    // shutdown program if WvApi has stopped
                BOOST_LOG_TRIVIAL(fatal) << "\nWvAPI Not initialised, terminating\n";
                break;
            }
            BOOST_LOG_TRIVIAL(error) << "WvListBeds failed, ReturnCode " << pWvApi->MapIntRetCodes.at(ReturnCode);
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }
        if (NumberOfBeds == 0) {
            BOOST_LOG_TRIVIAL(trace) << "WvListBeds API call to " << utf8_encode(pWvApi->gwHost) << " successful, but 0 beds returned";
        }

        // check each current bedMap entry for presence in bedList, mark false if no match
        for (auto bed : pWvApi->bedMap) {
            BOOST_LOG_TRIVIAL(trace) << "bedmap entry " << bed.first;
            bool match = FALSE;
            for (int i = 0; i < NumberOfBeds; i++) {
                if (bed.first == uniqueBedID(pWvApi->BedList.WvBeds[i])) {
                    match = TRUE;
                }
            }
            // If none of the bedlist entries match sn of an existing bedmap entry, mark for removal
            // Will be removed at start of next loop. The flag will also instruct bed object bedLoop thread to terminate
            if (match == FALSE) {
                bed.second->InBedList_Atomic = FALSE;       // mark for removal from bedmap
            }
        }

        // For each bed returned in bedlist. check if already in bed map
        // If already in bed map, call update function to provide bed object with latest bedList info
        // This will provide bed objec with changes to MRN and device Status, eg bed coming out of standby
        for (int i = 0; i < NumberOfBeds; i++) {
            std::string bedID = uniqueBedID(pWvApi->BedList.WvBeds[i]);
            auto bedIter = pWvApi->bedMap.find(bedID);
            if (bedIter != pWvApi->bedMap.end()) {
                bedIter->second->update(pWvApi->BedList.WvBeds[i]);
            }
            // If BedList entry is not found in the bed object map, inset into map as new bed object
            else if (bedIter == pWvApi->bedMap.end()) {
                auto ret = pWvApi->bedMap.try_emplace(bedID, new WvBed(pWvApi->BedList.WvBeds[i]));
                if (ret.second == TRUE) { BOOST_LOG_TRIVIAL(info) << "added " << bedID << " to bedlist";  }
                else { BOOST_LOG_TRIVIAL(error) << "failed to add " << bedID << " to bedlist"; }
            }   
        }
        // Check every 500ms to make terminate more responsive
        while (std::chrono::steady_clock::now() < nextLoopStart && running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }
    // clean up and call WvStop()
    delete pWvApi;      
    return 0;
}

