/*
 * Departures Board - freight-tracker client (see freightDataClient.h)
 *
 * This work is licensed under Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International.
 */
#include <freightDataClient.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

freightDataClient::freightDataClient(sharedBufferSpace *sharedBuffer) : js(sharedBuffer) {}

// Right-hand status column text
void freightDataClient::statusText(const freightService& s, char* out, size_t len) {
    if (strcmp(s.st, "CAN") == 0)         strlcpy(out, "Cancelled", len);
    else if (strcmp(s.st, "PASSED") == 0) strlcpy(out, "Passed", len);
    else if (strcmp(s.st, "NOT") == 0)    strlcpy(out, "Not active", len);
    else if (strcmp(s.st, "ACT") == 0)    strlcpy(out, "Activated", len);
    else if (s.hasVar && s.var == 0)      strlcpy(out, "On time", len);
    else if (s.exp[0])                    snprintf(out, len, "Exp %s", s.exp);
    else                                  strlcpy(out, "Running", len);
}

int freightDataClient::fetchBoard(rdStation *station, const char *baseUrl, const char *tiploc) {
    js->lastResultMessage[0] = '\0';
    boardChanged = false;

    if (!baseUrl[0] || !tiploc[0]) {
        strcpy(js->lastResultMessage, "Error: freight server or TIPLOC not set");
        return UPD_DATA_ERROR;
    }

    char base[MAXFREIGHTURLSIZE];
    strlcpy(base, baseUrl, sizeof(base));
    size_t blen = strlen(base);
    while (blen && base[blen-1] == '/') base[--blen] = '\0';
    char url[MAXFREIGHTURLSIZE + MAXTIPLOCSIZE + 16];
    snprintf(url, sizeof(url), "%s/board/%s.json", base, tiploc);

    unsigned long perfTimer = millis();
    WiFiClient client;
    HTTPClient http;
    http.setConnectTimeout(5000);
    http.setTimeout(8000);
    if (!http.begin(client, url)) {
        strlcpy(js->lastResultMessage, "Error: bad freight server URL", sizeof(js->lastResultMessage));
        return UPD_DATA_ERROR;
    }
    int code = http.GET();
    if (code <= 0) {
        snprintf(js->lastResultMessage, sizeof(js->lastResultMessage), "Error: %s", http.errorToString(code).c_str());
        http.end();
        return UPD_NO_RESPONSE;
    }
    if (code != 200) {
        snprintf(js->lastResultMessage, sizeof(js->lastResultMessage), "Error: HTTP %d from freight server", code);
        http.end();
        return (code == 404) ? UPD_DATA_ERROR : UPD_HTTP_ERROR;
    }

    // Only keep the fields we use
    JsonDocument filter;
    filter["loc"] = true;
    JsonObject f = filter["svcs"][0].to<JsonObject>();
    for (const char* k : {"t", "half", "d", "hc", "op", "from", "to", "type", "st", "exp", "var", "last", "reason"}) f[k] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
    http.end();
    if (err) {
        snprintf(js->lastResultMessage, sizeof(js->lastResultMessage), "Error: JSON %s", err.c_str());
        return UPD_DATA_ERROR;
    }

    strlcpy(locName, doc["loc"] | tiploc, sizeof(locName));
    JsonArray svcs = doc["svcs"].as<JsonArray>();
    int n = 0;
    for (JsonObject s : svcs) {
        if (n >= MAXBOARDSERVICES) break;
        freightService &x = services[n];
        strlcpy(x.t, s["t"] | "", sizeof(x.t));
        x.half = s["half"] | false;
        x.dayOffset = s["d"] | 0;
        strlcpy(x.hc, s["hc"] | "", sizeof(x.hc));
        strlcpy(x.op, s["op"] | "", sizeof(x.op));
        strlcpy(x.from, s["from"] | "", sizeof(x.from));
        strlcpy(x.to, s["to"] | "", sizeof(x.to));
        x.isPass = strcmp(s["type"] | "", "PASS") == 0;
        strlcpy(x.st, s["st"] | "NOT", sizeof(x.st));
        strlcpy(x.exp, s["exp"] | "", sizeof(x.exp));
        x.hasVar = !s["var"].isNull();
        x.var = s["var"] | 0;
        strlcpy(x.last, s["last"] | "", sizeof(x.last));
        strlcpy(x.reason, s["reason"] | "", sizeof(x.reason));
        n++;
    }

    // Has the board changed? Compare a compact signature (headcode + status + estimate per row)
    char sig[sizeof(signature)] = "";
    for (int i = 0; i < n; i++) {
        char row[24];
        snprintf(row, sizeof(row), "%s%s%s|", services[i].hc, services[i].st, services[i].exp);
        strlcat(sig, row, sizeof(sig));
    }
    boardChanged = (strcmp(sig, signature) != 0) || (n != station->numServices);
    strlcpy(signature, sig, sizeof(signature));
    numServices = n;

    snprintf(js->lastResultMessage, sizeof(js->lastResultMessage), "OK: %s S:%d T:%lu", boardChanged ? "UP" : "NC", n, millis() - perfTimer);
    return boardChanged ? UPD_SUCCESS : UPD_NO_CHANGE;
}

void freightDataClient::loadBoard(rdStation *station) {
    station->boardChanged = boardChanged;
    station->numServices = numServices;
    for (int i = 0; i < numServices; i++) {
        const freightService &x = services[i];
        rdService &r = station->service[i];
        // Left column: "10:57 6C92" (tomorrow's trains are marked with a leading '+')
        snprintf(r.via, sizeof(r.via), "%s%s %s", x.dayOffset > 0 ? "+" : "", x.t, x.hc);
        strlcpy(r.sTime, x.t, sizeof(r.sTime));
        if (x.op[0]) snprintf(r.destination, sizeof(r.destination), "%s (%s)", x.to, x.op);
        else strlcpy(r.destination, x.to, sizeof(r.destination));
        statusText(x, r.etd, sizeof(r.etd));
        strlcpy(r.opco, x.op, sizeof(r.opco));
        r.isCancelled = strcmp(x.st, "CAN") == 0;
        r.isDelayed = x.hasVar && x.var > 0;
        r.platform[0] = '\0';
        r.serviceType = TRAIN;
    }
}

void freightDataClient::serviceDetail(int index, char *out, size_t len) {
    if (index < 0 || index >= numServices) { out[0] = '\0'; return; }
    const freightService &x = services[index];
    char extra[MAXBUSTUBELOCATIONSIZE + 32] = "";
    if (strcmp(x.st, "CAN") == 0 && x.reason[0]) snprintf(extra, sizeof(extra), ", cancelled (%s)", x.reason);
    else if (x.last[0]) {
        if (x.hasVar && x.var) snprintf(extra, sizeof(extra), ", %s (%d %s)", x.last, abs(x.var), x.var > 0 ? "late" : "early");
        else snprintf(extra, sizeof(extra), ", %s", x.last);
    }
    snprintf(out, len, "%s %s from %s%s", x.hc, x.isPass ? "passing," : "calling,", x.from, extra);
}
