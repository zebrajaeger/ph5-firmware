#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

enum Ph5CommandType : uint8_t {
  MOVE_X = 1,
  MOVE_Y = 2,
  MOVE_XY = 3,

  MOVE_TO_X = 4,
  MOVE_TO_Y = 5,
  MOVE_TO_XY = 6,

  SPEED_X = 7,
  SPEED_Y = 8,
  SPEED_XY = 9,

  STOP_X = 32,
  STOP_Y = 33,
  STOP_XY = 34,

  FORCE_STOP = 48,

  SET_POS_X = 64,
  SET_POS_Y = 65,
  SET_POS_XY = 66,

  PING = 250,
  PONG = 251,

  ERROR = 254,
  UNKNOWN = 255
};

static const char* commandTypeToName(Ph5CommandType c) {
  switch (c) {
    case MOVE_X:
      return "MOVE_X";
    case MOVE_Y:
      return "MOVE_Y";
    case MOVE_XY:
      return "MOVE_XY";
    case MOVE_TO_X:
      return "MOVE_TO_X";
    case MOVE_TO_Y:
      return "MOVE_TO_Y";
    case MOVE_TO_XY:
      return "MOVE_TO_XY";
    case SPEED_X:
      return "SPEED_X";
    case SPEED_Y:
      return "SPEED_Y";
    case SPEED_XY:
      return "SPEED_XY";
    case STOP_X:
      return "STOP_X";
    case STOP_Y:
      return "STOP_Y";
    case STOP_XY:
      return "STOP_XY";
    case FORCE_STOP:
      return "FORCE_STOP";
    case SET_POS_X:
      return "SET_POS_X";
    case SET_POS_Y:
      return "SET_POS_Y";
    case SET_POS_XY:
      return "SET_POS_XY";
    case PING:
      return "PING";
    case PONG:
      return "PONG";
    case ERROR:
      return "ERROR";
    case UNKNOWN:
      return "UNKNOWN";
    default:
      return "?unspecific?";
  }
}

class Ph5Command {
  friend class JsonCommandParser;

 private:
  uint32_t id;
  Ph5CommandType type;
  int32_t posX;
  int32_t speedX;
  int32_t posY;
  int32_t speedY;

  Ph5CommandType setMoveXY(JsonDocument& doc);
  Ph5CommandType setSpeedXY(JsonDocument& doc);
  Ph5CommandType setMoveToXY(JsonDocument& doc);
  Ph5CommandType setPos(JsonDocument& doc);

 public:
  Ph5Command();
  Ph5Command(JsonDocument& doc);
  void set(JsonDocument& doc);

  inline uint32_t getId() { return id; };
  inline void setId(uint32_t _id) { id = _id; };

  inline Ph5CommandType getType() { return type; };
  inline void setType(Ph5CommandType _type) { type = _type; };

  inline int32_t getSpeedX() { return speedX; };
  inline void setSpeedX(int32_t speed) { speedX = speed; };

  inline int32_t getPosX() { return posX; };
  inline void setPosX(int32_t pos) { posX = pos; };

  inline int32_t getSpeedY() { return speedY; };
  inline void setSpeedY(int32_t speed) { speedY = speed; };

  inline int32_t getPosY() { return posY; };
  inline void setPosY(int32_t pos) { posY = pos; };

  String toString();
};

class JsonCommandParser {
 public:
  JsonCommandParser(Stream& _console, int32_t _defaultSpeedX, int32_t _defaultSpeedY);
  DeserializationError parse(const char* text, Ph5Command& command);

 private:
  Stream& console;
  int32_t defaultSpeedX;
  int32_t defaultSpeedY;
};
