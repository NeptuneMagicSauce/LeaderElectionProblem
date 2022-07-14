#pragma once

#include <iostream>
#include <string>
#include <optional>

using namespace std;

namespace json
{

    struct Message
    {
        enum struct Type
        {
            Greetings,
            ElectionStart, // specified as "election-message" which is ambiguous
            ElectedLeader,
        };
        uint16_t id;
        Type type;
        string value; // question to specifier: what is this field used for?
    };

    const string prefixID = "\t\"source\": ";
    const string prefixType = "\t\"type\": ";
    const string prefixValue = "\t\"value\": ";

    auto to_string(Message const& msg)
    {
        ostringstream ret;
        ret << "{" << endl;
        ret << prefixID << std::to_string(msg.id) << "," << endl;
        ret << prefixType << std::to_string(static_cast<int>(msg.type)) << "," << endl;
        ret << prefixValue << msg.value << endl;
        ret << "}";
        return ret.str();
    }

    optional<Message> from_string(string const& str)
    {
        try
        {
            Message msg;
            istringstream iss{str};
            string line;
            if (!getline(iss, line)) { return {}; }
            if (line != "{") { return {}; }
            if (!getline(iss, line)) { return {}; }
            auto posID = line.find(prefixID);
            if (posID == string::npos) { return {}; }
            posID = prefixID.size();
            auto lineID = line.substr(posID, line.size() - posID);
            msg.id = stoi(lineID);
            if (!getline(iss, line)) { return {}; }
            auto posType = line.find(prefixType);
            if (posType == string::npos) { return {}; }
            posType = prefixType.size();
            auto lineType = line.substr(posType, line.size() - posType);
            int type = stoi(lineType);
            msg.type = static_cast<Message::Type>(type);
            if (!getline(iss, line)) { return {}; }
            auto posValue = line.find(prefixValue);
            if (posValue == string::npos) { return {}; }
            posValue = prefixValue.size();
            auto lineValue = line.substr(posValue, line.size() - posValue);
            // if (!lineValue.size() || lineValue.at(lineValue.size() - 1) != ',') { return {}; }
            // msg.value = lineValue.substr(0, lineValue.size() - 1);
            msg.value = lineValue;
            return msg;
        }
        catch (std::exception const&)
        {
            return {};
        }
    }
}
