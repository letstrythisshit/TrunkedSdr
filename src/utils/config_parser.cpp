#include "config_parser.h"
#include "logger.h"
#include <fstream>
#include <sstream>

namespace TrunkSDR {

bool ConfigParser::loadFromFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open config file:", filename);
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    return loadFromString(buffer.str());
}

bool ConfigParser::loadFromString(const std::string& json_str) {
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errors;

    std::istringstream stream(json_str);
    if (!Json::parseFromStream(builder, stream, &root, &errors)) {
        LOG_ERROR("JSON parse error:", errors);
        return false;
    }

    return parseJSON(root);
}

bool ConfigParser::parseJSON(const Json::Value& root) {
    if (!parseSDRConfig(root["sdr"])) {
        return false;
    }

    if (!parseSystemConfig(root["system"])) {
        return false;
    }

    if (!parseAudioConfig(root["audio"])) {
        return false;
    }

    if (!parseTalkgroupConfig(root["talkgroups"])) {
        return false;
    }

    return true;
}

bool ConfigParser::parseSDRConfig(const Json::Value& sdr_node) {
    if (sdr_node.isNull()) {
        LOG_ERROR("Missing SDR configuration");
        return false;
    }

    config_.sdr.device_index = sdr_node.get("device_index", 0).asUInt();
    config_.sdr.sample_rate = sdr_node.get("sample_rate", DEFAULT_SAMPLE_RATE).asUInt();
    config_.sdr.ppm_correction = sdr_node.get("ppm_correction", 0).asInt();

    std::string gain_str = sdr_node.get("gain", "auto").asString();
    if (gain_str == "auto") {
        config_.sdr.auto_gain = true;
        config_.sdr.gain = 0;
    } else {
        config_.sdr.auto_gain = false;
        config_.sdr.gain = std::stod(gain_str);
    }

    LOG_INFO("SDR config: device =", config_.sdr.device_index,
             "sample_rate =", config_.sdr.sample_rate);

    return true;
}

bool ConfigParser::parseSystemConfig(const Json::Value& system_node) {
    if (system_node.isNull()) {
        LOG_ERROR("Missing system configuration");
        return false;
    }

    std::string type_str = system_node.get("type", "p25").asString();
    config_.system.type = stringToSystemType(type_str);

    config_.system.system_id = system_node.get("system_id", 0).asUInt();
    config_.system.nac = system_node.get("nac", 0).asUInt();
    config_.system.wacn = system_node.get("wacn", 0).asUInt();
    config_.system.name = system_node.get("name", "Unknown").asString();

    // Parse control channels
    const Json::Value& channels = system_node["control_channels"];
    if (channels.isArray()) {
        for (const auto& ch : channels) {
            config_.system.control_channels.push_back(ch.asDouble());
        }
    }

    if (config_.system.control_channels.empty()) {
        LOG_ERROR("No control channels configured");
        return false;
    }

    LOG_INFO("System config:", systemTypeToString(config_.system.type),
             "control channels =", config_.system.control_channels.size());

    return true;
}

bool ConfigParser::parseAudioConfig(const Json::Value& audio_node) {
    if (audio_node.isNull()) {
        // Use defaults
        config_.audio.output_device = "default";
        config_.audio.codec = CodecType::IMBE;
        config_.audio.sample_rate = AUDIO_SAMPLE_RATE;
        config_.audio.record_calls = false;
        config_.audio.recording_path = "/tmp";
        return true;
    }

    config_.audio.output_device = audio_node.get("output_device", "default").asString();
    config_.audio.sample_rate = audio_node.get("sample_rate", AUDIO_SAMPLE_RATE).asUInt();
    config_.audio.record_calls = audio_node.get("record_calls", false).asBool();
    config_.audio.recording_path = audio_node.get("recording_path", "/tmp").asString();

    std::string codec_str = audio_node.get("codec", "imbe").asString();
    config_.audio.codec = stringToCodecType(codec_str);

    LOG_INFO("Audio config: device =", config_.audio.output_device,
             "sample_rate =", config_.audio.sample_rate);

    return true;
}

bool ConfigParser::parseTalkgroupConfig(const Json::Value& tg_node) {
    if (tg_node.isNull()) {
        // No talkgroup filtering - allow all
        return true;
    }

    // Parse enabled talkgroups
    const Json::Value& enabled = tg_node["enabled"];
    if (enabled.isArray()) {
        for (const auto& tg : enabled) {
            config_.talkgroups.enabled.push_back(tg.asUInt());
        }
    }

    // Parse priorities
    const Json::Value& priorities = tg_node["priority"];
    if (priorities.isObject()) {
        for (const auto& key : priorities.getMemberNames()) {
            TalkgroupID tg = std::stoul(key);
            Priority priority = priorities[key].asUInt();
            config_.talkgroups.priorities[tg] = priority;
        }
    }

    // Parse labels
    const Json::Value& labels = tg_node["labels"];
    if (labels.isObject()) {
        for (const auto& key : labels.getMemberNames()) {
            TalkgroupID tg = std::stoul(key);
            std::string label = labels[key].asString();
            config_.talkgroups.labels[tg] = label;
        }
    }

    LOG_INFO("Talkgroup config: enabled =", config_.talkgroups.enabled.size());

    return true;
}

SystemType ConfigParser::stringToSystemType(const std::string& str) {
    if (str == "p25" || str == "p25_phase1") return SystemType::P25_PHASE1;
    if (str == "p25_phase2") return SystemType::P25_PHASE2;
    if (str == "smartnet") return SystemType::SMARTNET;
    if (str == "smartzone") return SystemType::SMARTZONE;
    if (str == "edacs") return SystemType::EDACS;
    if (str == "ltr") return SystemType::LTR;
    if (str == "dmr") return SystemType::DMR;
    if (str == "nxdn") return SystemType::NXDN;
    return SystemType::UNKNOWN;
}

CodecType ConfigParser::stringToCodecType(const std::string& str) {
    if (str == "imbe") return CodecType::IMBE;
    if (str == "ambe") return CodecType::AMBE;
    if (str == "provoice") return CodecType::PROVOICE;
    if (str == "dmr" || str == "dmr_codec") return CodecType::DMR_CODEC;
    if (str == "codec2") return CodecType::CODEC2;
    if (str == "fm" || str == "analog") return CodecType::ANALOG_FM;
    return CodecType::IMBE;  // Default
}

std::string ConfigParser::systemTypeToString(SystemType type) {
    switch (type) {
        case SystemType::P25_PHASE1: return "P25 Phase 1";
        case SystemType::P25_PHASE2: return "P25 Phase 2";
        case SystemType::SMARTNET: return "Motorola SmartNet";
        case SystemType::SMARTZONE: return "Motorola SmartZone";
        case SystemType::EDACS: return "EDACS";
        case SystemType::LTR: return "LTR";
        case SystemType::DMR: return "DMR";
        case SystemType::NXDN: return "NXDN";
        default: return "Unknown";
    }
}

} // namespace TrunkSDR
