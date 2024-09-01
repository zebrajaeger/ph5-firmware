// #pragma once

// #include <Arduino.h>
// #include <ArduinoJson.h>
// #include "command.h"

// class JsonCommand {
//  public:
//   JsonCommand() : nr(0), cmd(), x(0), y(0), speedX(0), speedY(0)
//   // focus(0), trigger(0), 
//   // valid(false) 
//   {}

//   CommandType setMoveXY(JsonDocument& doc) {
//     JsonVariant temp;
//     bool hasX = false;
//     bool hasY = false;
//     temp = doc["x"];
//     if (!temp.isNull()) {
//       x = temp;
//       hasX = true;
//     }
//     temp = doc["y"];
//     if (!temp.isNull()) {
//       y = temp;
//       hasY = true;
//     }
//     temp = doc["speedX"];
//     if (!temp.isNull()) {
//       speedX = temp;
//     }
//     temp = doc["speedY"];
//     if (!temp.isNull()) {
//       speedY = temp;
//     }
//     if (hasX && hasY) {
//       return MOVE_XY;
//     }
//     if (hasX) {
//       return MOVE_X;
//     }
//     if (hasY) {
//       return MOVE_Y;
//     }
//     return ERROR;
//   }

//   Cmd setSpeedXY(JsonDocument& doc) {
//     JsonVariant temp;
//     bool hasX = false;
//     bool hasY = false;
//     temp = doc["speedX"];
//     if (!temp.isNull()) {
//       speedX = temp;
//       hasX = true;
//     }
//     temp = doc["speedY"];
//     if (!temp.isNull()) {
//       speedY = temp;
//       hasY = true;
//     }
//     if (hasX && hasY) {
//       return SPEED_XY;
//     }
//     if (hasX) {
//       return SPEED_X;
//     }
//     if (hasY) {
//       return SPEED_Y;
//     }
//     return ERROR;
//   }

//   Cmd setMoveToXY(JsonDocument& doc) {
//     JsonVariant temp;
//     bool hasX = false;
//     bool hasY = false;
//     temp = doc["x"];
//     if (!temp.isNull()) {
//       x = temp;
//       hasX = true;
//     }
//     temp = doc["y"];
//     if (!temp.isNull()) {
//       y = temp;
//       hasY = true;
//     }
//     temp = doc["speedX"];
//     if (!temp.isNull()) {
//       speedX = temp;
//     }
//     temp = doc["speedY"];
//     if (!temp.isNull()) {
//       speedY = temp;
//     }
//     if (hasX && hasY) {
//       return MOVE_TO_XY;
//     }
//     if (hasX) {
//       return MOVE_TO_X;
//     }
//     if (hasY) {
//       return MOVE_TO_Y;
//     }
//     return ERROR;
//   }

//   Cmd setPos(JsonDocument& doc) {
//     JsonVariant temp;
//     bool hasX = false;
//     bool hasY = false;
//     temp = doc["x"];
//     if (!temp.isNull()) {
//       x = temp;
//       hasX = true;
//     }
//     temp = doc["y"];
//     if (!temp.isNull()) {
//       y = temp;
//       hasY = true;
//     }
//     if (hasX && hasY) {
//       return SET_POS_XY;
//     }
//     if (hasX) {
//       return SET_POS_X;
//     }
//     if (hasY) {
//       return SET_POS_Y;
//     }
//     return ERROR;
//   }

//   Cmd setFocusAndTrigger(JsonDocument& doc) {
//     JsonVariant temp;
//     bool hasFocus = false;
//     bool hasTrigger = false;

//     temp = doc["focus"];
//     if (!temp.isNull()) {
//       focus = temp;
//       hasFocus = true;
//     }
//     temp = doc["trigger"];
//     if (!temp.isNull()) {
//       trigger = temp;
//       hasTrigger = true;
//     }

//     // if (hasFocus && hasTrigger) {
//     //   return FOCUS_AND_TRIGGER;
//     // }
//     // if (hasFocus) {
//     //   return FOCUS;
//     // }
//     // if (hasTrigger) {
//     //   return TRIGGER;
//     // }
//     return ERROR;
//   }

//   void set(JsonDocument& doc) {
//     valid = false;

//     JsonVariant temp;
//     temp = doc["nr"];
//     if (!temp.isNull()) {
//       nr = temp;
//     } else {
//       nr = 0;
//     }

//     temp = doc["cmd"];
//     if (!temp.isNull()) {
//       String c = temp.as<const char*>();
//       c.toLowerCase();
//       if (c == "move") {
//         cmd = setMoveXY(doc);
//         valid = cmd != Cmd::ERROR;

//       } else if (c == "speed") {
//         cmd = setSpeedXY(doc);
//         valid = cmd != Cmd::ERROR;

//       } else if (c == "moveto") {
//         cmd = setMoveToXY(doc);
//         valid = cmd != Cmd::ERROR;

//       } else if (c == "stop") {
//         cmd = Cmd::STOP_XY;
//         valid = true;

//       } else if (c == "forcestop") {
//         cmd = Cmd::FORCE_STOP;
//         valid = true;

//       // } else if (c == "shot") {
//       //   cmd = setFocusAndTrigger(doc);
//       //   valid = cmd != Cmd::ERROR;

//       } else if (c == "setpos") {
//         cmd = setPos(doc);
//         valid = cmd != Cmd::ERROR;

//       } else {
//         cmd = Cmd::UNKNOWN;
//       }
//     }
//   }

//   String toString() {
//     String temp = "jsonCommand(";
//     temp = temp + "nr: " + nr;
//     temp = temp + ", cmd: " + commandToName(cmd);
//     temp = temp + ", x: " + x;
//     temp = temp + ", y: " + y;
//     temp = temp + ", speedX: " + speedX;
//     temp = temp + ", speedY: " + speedY;
//     // temp = temp + ", focus: " + focus;
//     // temp = temp + ", trigger: " + trigger;
//     // temp = temp + ", valid: " + valid;
//     temp = temp + ")";
//     return temp;
//   }

//   int32_t nr;
//   Cmd cmd;
//   int32_t x;
//   int32_t y;
//   int32_t speedX;
//   int32_t speedY;
//   bool focus;
//   bool trigger;
//   bool valid;
// };

// class JsonCommandParser {
//  public:
//   JsonCommandParser(Stream& _console, int32_t _defaultSpeedX, int32_t _defaultSpeedY)
//       : console(_console), defaultSpeedX(_defaultSpeedX), defaultSpeedY(_defaultSpeedY) {}

//   DeserializationError parse(const char* text, JsonCommand& command) {
//     JsonDocument doc;
//     DeserializationError error = deserializeJson(doc, text);

//     if (error) {
//       console.print("[CMD] Error parsing command: ");
//       console.println(error.c_str());
//       console.print(text);
//     } else {
//       command.set(doc);

//       // speedX
//       if (command.cmd == Cmd::MOVE_X || command.cmd == Cmd::MOVE_TO_X ||
//           command.cmd == Cmd::MOVE_XY || command.cmd == Cmd::MOVE_TO_XY) {
//         if (command.speedX == 0) {
//           command.speedX = defaultSpeedX;
//         }
//       }

//       // speedY
//       if (command.cmd == Cmd::MOVE_Y || command.cmd == Cmd::MOVE_TO_Y ||
//           command.cmd == Cmd::MOVE_XY || command.cmd == Cmd::MOVE_TO_XY) {
//         if (command.speedY == 0) {
//           command.speedY = defaultSpeedY;
//         }
//       }
//     }
//     return error;
//   }

//  private:
//   Stream& console;
//   int32_t defaultSpeedX;
//   int32_t defaultSpeedY;
// };
