#include "ph5command.h"

Ph5Command::Ph5Command() : id(0), type(Ph5CommandType::UNKNOWN), posX(0), posY(0), speedX(0), speedY(0) {}

Ph5Command::Ph5Command(JsonDocument& doc)
    : id(0), type(Ph5CommandType::UNKNOWN), posX(0), posY(0), speedX(0), speedY(0) {
  set(doc);
}

Ph5CommandType Ph5Command::setMoveXY(JsonDocument& doc) {
  JsonVariant temp;
  bool hasX = false;
  bool hasY = false;
  temp = doc["x"];
  if (!temp.isNull()) {
    posX = temp;
    hasX = true;
  }
  temp = doc["y"];
  if (!temp.isNull()) {
    posY = temp;
    hasY = true;
  }
  temp = doc["speedX"];
  if (!temp.isNull()) {
    speedX = temp;
  }
  temp = doc["speedY"];
  if (!temp.isNull()) {
    speedY = temp;
  }
  if (hasX && hasY) {
    return MOVE_XY;
  }
  if (hasX) {
    return MOVE_X;
  }
  if (hasY) {
    return MOVE_Y;
  }
  return ERROR;
}

Ph5CommandType Ph5Command::setSpeedXY(JsonDocument& doc) {
  JsonVariant temp;
  bool hasX = false;
  bool hasY = false;
  temp = doc["speedX"];
  if (!temp.isNull()) {
    speedX = temp;
    hasX = true;
  }
  temp = doc["speedY"];
  if (!temp.isNull()) {
    speedY = temp;
    hasY = true;
  }
  if (hasX && hasY) {
    return SPEED_XY;
  }
  if (hasX) {
    return SPEED_X;
  }
  if (hasY) {
    return SPEED_Y;
  }
  return ERROR;
}

Ph5CommandType Ph5Command::setMoveToXY(JsonDocument& doc) {
  JsonVariant temp;
  bool hasX = false;
  bool hasY = false;
  temp = doc["x"];
  if (!temp.isNull()) {
    posX = temp;
    hasX = true;
  }
  temp = doc["y"];
  if (!temp.isNull()) {
    posY = temp;
    hasY = true;
  }
  temp = doc["speedX"];
  if (!temp.isNull()) {
    speedX = temp;
  }
  temp = doc["speedY"];
  if (!temp.isNull()) {
    speedY = temp;
  }
  if (hasX && hasY) {
    return MOVE_TO_XY;
  }
  if (hasX) {
    return MOVE_TO_X;
  }
  if (hasY) {
    return MOVE_TO_Y;
  }
  return ERROR;
}

Ph5CommandType Ph5Command::setPos(JsonDocument& doc) {
  JsonVariant temp;
  bool hasX = false;
  bool hasY = false;
  temp = doc["x"];
  if (!temp.isNull()) {
    posX = temp;
    hasX = true;
  }
  temp = doc["y"];
  if (!temp.isNull()) {
    posY = temp;
    hasY = true;
  }
  if (hasX && hasY) {
    return SET_POS_XY;
  }
  if (hasX) {
    return SET_POS_X;
  }
  if (hasY) {
    return SET_POS_Y;
  }
  return ERROR;
}

void Ph5Command::set(JsonDocument& doc) {
  JsonVariant temp;
  temp = doc["nr"];
  if (!temp.isNull()) {
    id = temp;
  } else {
    id = 0;
  }

  temp = doc["cmd"];
  if (!temp.isNull()) {
    String c = temp.as<const char*>();
    c.toLowerCase();
    if (c == "move") {
      type = setMoveXY(doc);

    } else if (c == "speed") {
      type = setSpeedXY(doc);

    } else if (c == "moveto") {
      type = setMoveToXY(doc);

    } else if (c == "stop") {
      type = Ph5CommandType::STOP_XY;

    } else if (c == "forcestop") {
      type = Ph5CommandType::FORCE_STOP;

    } else if (c == "setpos") {
      type = setPos(doc);

    } else {
      type = Ph5CommandType::UNKNOWN;
    }
  }
}

String Ph5Command::toString() {
  String temp = "Command(";
  temp = temp + "id: " + id;
  temp = temp + ", type: " + commandTypeToName(type);
  temp = temp + ", x: " + posX;
  temp = temp + ", speedX: " + speedX;
  temp = temp + ", y: " + posY;
  temp = temp + ", speedY: " + speedY;
  temp = temp + ")";
  return temp;
}

JsonCommandParser::JsonCommandParser(Stream& _console, int32_t _defaultSpeedX, int32_t _defaultSpeedY)
    : console(_console), defaultSpeedX(_defaultSpeedX), defaultSpeedY(_defaultSpeedY) {}

DeserializationError JsonCommandParser::parse(const char* text, Ph5Command& command) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, text);

  if (error) {
    console.print("[CMD] Error parsing command: ");
    console.println(error.c_str());
    console.print(text);
  } else {
    command.set(doc);

    // speedX
    if (command.type == Ph5CommandType::MOVE_X || command.type == Ph5CommandType::MOVE_TO_X ||
        command.type == Ph5CommandType::MOVE_XY || command.type == Ph5CommandType::MOVE_TO_XY) {
      if (command.speedX == 0) {
        command.speedX = defaultSpeedX;
      }
    }

    // speedY
    if (command.type == Ph5CommandType::MOVE_Y || command.type == Ph5CommandType::MOVE_TO_Y ||
        command.type == Ph5CommandType::MOVE_XY || command.type == Ph5CommandType::MOVE_TO_XY) {
      if (command.speedY == 0) {
        command.speedY = defaultSpeedY;
      }
    }
  }
  return error;
}
