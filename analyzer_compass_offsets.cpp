#include "analyzer_compass_offsets.h"

#include <stdio.h>

#include "util.h"
#include "analyzer_util.h"

bool Analyzer_Compass_Offsets::configure(INIReader *config) {
    if (!MAVLink_Message_Handler::configure(config)) {
	return false;
    }
    return true;
}

double vec_len(double vec[3]) {
    return sqrt(vec[0]*vec[0] + vec[1]*vec[1] + vec[2]*vec[2]);
}

// true if we have all elements of vector and we haven't processed
// this vector before
bool Analyzer_Compass_Offsets::new_compass_results()
{
    if (!_vehicle->param_seen("COMPASS_OFS_X") ||
        !_vehicle->param_seen("COMPASS_OFS_Y") ||
        !_vehicle->param_seen("COMPASS_OFS_Z")) {
        // we haven't seen the entire vector yet
        return false;
    }

    if (_vehicle->param_modtime("COMPASS_OFS_X") == modtime_compass_ofs[0] &&
        _vehicle->param_modtime("COMPASS_OFS_Y") == modtime_compass_ofs[1] &&
        _vehicle->param_modtime("COMPASS_OFS_Z") == modtime_compass_ofs[2]) {
        // nothing has changed; at time of writing we get called for
        // every parameter which is set (low-bandwidth, so *shrug*)
        return false;
    }

    modtime_compass_ofs[0] = _vehicle->param_modtime("COMPASS_OFS_X");
    modtime_compass_ofs[1] = _vehicle->param_modtime("COMPASS_OFS_Y");
    modtime_compass_ofs[2] = _vehicle->param_modtime("COMPASS_OFS_Z");

    return true;
}

void Analyzer_Compass_Offsets::evaluate(uint64_t T)
{

    if (compass_offset_results_offset == MAX_COMPASS_OFFSET_RESULTS) {
        compass_offset_results_overrun = true;
        return;
    }

    if (! new_compass_results()) {
        return;
    }

    double compass_ofs[3];
    compass_ofs[0] = _vehicle->param("COMPASS_OFS_X");
    compass_ofs[1] = _vehicle->param("COMPASS_OFS_Y");
    compass_ofs[2] = _vehicle->param("COMPASS_OFS_Z");

    double len = vec_len(compass_ofs);
    compass_offset_status status;
    if (len >= fail_offset) {
        status = compass_offset_fail;
        // ::printf("FAIL: COMPASS_OFS %f>%d\n", len, fail_offset);
    } else if (len >= warn_offset) {
        // ::printf("WARNING: COMPASS_OFS %f>%d\n", len, warn_offset);
        status = compass_offset_warn;
    } else if (is_zero(len)) {
        // ::printf("WARNING: Zero COMPASS_OFS - that's probably bad\n");
        status = compass_offset_zero;
    } else {
        // ::printf("OK: COMPASS_OFS %f looks good\n", len);
        status = compass_offset_ok;
    }

    compass_offset_results[compass_offset_results_offset].len = len;
    compass_offset_results[compass_offset_results_offset].status = status;
    compass_offset_results[compass_offset_results_offset].timestamp = T;
    compass_offset_results_offset++;
}

// swiped from AnalyzeTest_Compass in experimental ArduPilot tree:
void Analyzer_Compass_Offsets::handle_decoded_message(uint64_t T, mavlink_param_value_t &param)
{
    evaluate(T);
}

#include <stdlib.h>

const char * Analyzer_Compass_Offsets::results_json_compass_offsets_status_string(compass_offset_result result)
{
    switch(result.status) {
    case compass_offset_warn:
        return "WARNING";
    case compass_offset_ok:
        return "OK";
    case compass_offset_zero:
        return "WARNING";
    case compass_offset_fail:
        return "FAIL";
    default:
        return "Unknown";
    }
}

void Analyzer_Compass_Offsets::addStatusReason(Json::Value &root,compass_offset_result result)
{
    std::string reason  = std::string("");
    reason += "COMPASS_OFS " + to_string(result.len);
    switch(result.status) {
    case compass_offset_warn:
        reason += " > ";
        reason += to_string(warn_offset);
        break;
    case compass_offset_ok:
        reason += " <= ";
        reason += to_string(warn_offset);
        break;
    case compass_offset_zero:
        reason += " == 0";
        break;
    case compass_offset_fail:
        reason += " > ";
        reason += to_string(fail_offset);
        break;
    }
    root["reason"] = reason;
}

void Analyzer_Compass_Offsets::do_add_evilness(struct compass_offset_result result)
{
    switch(result.status) {
    case compass_offset_warn:
        add_evilness(2);
        break;
    case compass_offset_fail:
        add_evilness(5);
        break;
    case compass_offset_zero:
        add_evilness(4);
        break;
    case compass_offset_ok:
        break;
    }
}

void Analyzer_Compass_Offsets::results_json_compass_offsets(Json::Value &root)
{
    Json::Value ret(Json::arrayValue);
    for (uint8_t i=0; i < compass_offset_results_offset; i++) {
        Json::Value result(Json::objectValue);
        result["timestamp"] = (Json::UInt64)(compass_offset_results[i].timestamp);
        result["status"] = results_json_compass_offsets_status_string(compass_offset_results[i]);
        do_add_evilness(compass_offset_results[i]);
        Json::Value series(Json::arrayValue);
        series.append("PARM");
        result["series"] = series;
        addStatusReason(result,compass_offset_results[i]);
        root.append(result);
    }
    if (compass_offset_results_offset == 0) {
        Json::Value result(Json::objectValue);
        result["status"] = "FAIL";
        Json::Value series(Json::arrayValue);
        series.append("PARM");
        result["series"] = series;
        result["reason"] = "No compass offset parameter set seen";
        root.append(result);
        add_evilness(5);
    }
}


void Analyzer_Compass_Offsets::results_json_results(Json::Value &root)
{
    results_json_compass_offsets(root);
}