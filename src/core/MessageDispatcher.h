#pragma once
#include <string>
#include <functional>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Tipos de mensagem do protocolo de controle DevLink
enum class MsgType {
    // Android → PC
    Hello,        // {"type":"hello","deviceName":"...","deviceId":"..."}
    StartCamera,  // {"type":"start_camera"}
    StopCamera,   // {"type":"stop_camera"}
    Ping,         // {"type":"ping"}

    // PC → Android
    Welcome,      // {"type":"welcome","deviceName":"...","deviceId":"..."}
    Pong,         // {"type":"pong"}
    Error,        // {"type":"error","message":"..."}

    PairRequest,
    PairAccepted,
    PairRejected,

    Unknown
};

struct ControlMessage {
    MsgType type = MsgType::Unknown;
    json    payload;
};

inline MsgType msgTypeFromString(const std::string& s)
{
    if (s == "hello")        return MsgType::Hello;
    if (s == "start_camera") return MsgType::StartCamera;
    if (s == "stop_camera")  return MsgType::StopCamera;
    if (s == "ping")         return MsgType::Ping;
    if (s == "pair_request")  return MsgType::PairRequest;
    if (s == "pair_accepted") return MsgType::PairAccepted;
    if (s == "pair_rejected") return MsgType::PairRejected;
    if (s == "welcome")      return MsgType::Welcome;
    if (s == "pong")         return MsgType::Pong;
    if (s == "error")        return MsgType::Error;
    return MsgType::Unknown;
}

inline ControlMessage parseMessage(const std::string& raw)
{
    ControlMessage msg;
    try {
        msg.payload = json::parse(raw);
        msg.type    = msgTypeFromString(msg.payload.value("type", ""));
    } catch (...) {
        msg.type = MsgType::Unknown;
    }
    return msg;
}