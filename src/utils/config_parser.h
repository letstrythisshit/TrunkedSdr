#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include "types.h"
#include <string>
#include <map>
#include <json/json.h>

namespace TrunkSDR {

struct AudioConfig {
    std::string output_device;
    CodecType codec;
    uint32_t sample_rate;
    bool record_calls;
    std::string recording_path;
};

struct TalkgroupConfig {
    std::vector<TalkgroupID> enabled;
    std::map<TalkgroupID, Priority> priorities;
    std::map<TalkgroupID, std::string> labels;
};

struct Config {
    SDRConfig sdr;
    SystemInfo system;
    AudioConfig audio;
    TalkgroupConfig talkgroups;
};

class ConfigParser {
public:
    ConfigParser() = default;

    bool loadFromFile(const std::string& filename);
    bool loadFromString(const std::string& json_str);

    const Config& getConfig() const { return config_; }

    static SystemType stringToSystemType(const std::string& str);
    static CodecType stringToCodecType(const std::string& str);
    static std::string systemTypeToString(SystemType type);

private:
    bool parseJSON(const Json::Value& root);
    bool parseSDRConfig(const Json::Value& sdr_node);
    bool parseSystemConfig(const Json::Value& system_node);
    bool parseAudioConfig(const Json::Value& audio_node);
    bool parseTalkgroupConfig(const Json::Value& tg_node);

    Config config_;
};

} // namespace TrunkSDR

#endif // CONFIG_PARSER_H
