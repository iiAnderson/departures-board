/*
 * Departures Board - freight-tracker client
 *
 * Reads a board from a freight-tracker service (darwin-connect/freight-tracker), which tracks
 * freight / non-passenger trains at any TIPLOC using Network Rail TRUST + CIF data.
 *
 *   GET <baseUrl>/board/<TIPLOC>.json
 *
 * This work is licensed under Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 */
#pragma once
#include <Arduino.h>
#include <sharedDataStructs.h>
#include <responseCodes.h>

#define MAXFREIGHTURLSIZE 64
#define MAXTIPLOCSIZE 8

struct freightService {
    char t[6];                       // booked HH:MM at the location
    bool half;                       // WTT half minute
    int dayOffset;                   // 1 = tomorrow
    char hc[5];                      // headcode
    char op[12];                     // operator short name (or TRUST toc id)
    char from[MAXBUSTUBELOCATIONSIZE];
    char to[MAXBUSTUBELOCATIONSIZE];
    bool isPass;
    char st[8];                      // NOT / ACT / RUN / PASSED / CAN
    char exp[6];                     // estimated HH:MM or ""
    bool hasVar;
    int var;                         // minutes late (+) / early (-)
    char last[MAXBUSTUBELOCATIONSIZE];  // "Parson Street 10:53"
    char reason[4];                  // cancellation reason code
};

class freightDataClient {
    private:
        freightService services[MAXBOARDSERVICES];
        int numServices = 0;
        char locName[MAXLOCATIONSIZE] = "";
        bool boardChanged = false;
        char signature[MAXBOARDSERVICES * 24] = "";   // last board's headcode/status/estimate summary
        sharedBufferSpace* js = nullptr;

        static void statusText(const freightService& s, char* out, size_t len);

    public:
        freightDataClient(sharedBufferSpace *sharedBuffer);
        // Fetch on the background core. Returns UPD_SUCCESS (changed), UPD_NO_CHANGE or an error code.
        int fetchBoard(rdStation *station, const char *baseUrl, const char *tiploc);
        // Copy the fetched board into the display structure (call on the display core).
        void loadBoard(rdStation *station);
        // A one-line description of a service for the scrolling third line.
        void serviceDetail(int index, char *out, size_t len);
        const char* locationName() { return locName; }
};
