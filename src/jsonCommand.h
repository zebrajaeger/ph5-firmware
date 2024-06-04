#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

class JsonCommand {
 public:
  JsonCommand() : nr(0), cmd(), x(0), y(0), focus(0), trigger(0), valid(false) {}
  // command: move(x,y,xy), moveto(x,y,xy), stop(x,y,xy), focus(f), trigger(t), focusAndTrigger(ft)
  enum Cmd {
    MOVE_X,
    MOVE_Y,
    MOVE_XY,
    MOVE_TO_X,
    MOVE_TO_Y,
    MOVE_TO_XY,
    STOP,
    FOCUS,
    TRIGGER,
    FOCUS_AND_TRIGGER,
    UNKNOWN,
    ERROR
  };

  static void commandToName(Cmd c, String& result) {
    switch (c) {
      case MOVE_X:
        result = "MOVE_X";
        break;
      case MOVE_Y:
        result = "MOVE_Y";
        break;
      case MOVE_XY:
        result = "MOVE_XY";
        break;
      case MOVE_TO_X:
        result = "MOVE_TO_X";
        break;
      case MOVE_TO_Y:
        result = "MOVE_TO_Y";
        break;
      case MOVE_TO_XY:
        result = "MOVE_TO_XY";
        break;
      case STOP:
        result = "STOP";
        break;
      case FOCUS:
        result = "FOCUS";
        break;
      case TRIGGER:
        result = "TRIGGER";
        break;
      case FOCUS_AND_TRIGGER:
        result = "FOCUS_AND_TRIGGER";
        break;
      case UNKNOWN:
        result = "UNKNOWN";
        break;
      case ERROR:
        result = "ERROR";
        break;
    }
  }

  static String commandToName(Cmd c) {
    String result;
    commandToName(c, result);
    return result;
  }

  Cmd setMoveXY(JsonDocument& doc) {
    JsonVariant temp;
    bool hasX = false;
    bool hasY = false;
    temp = doc["x"];
    if (!temp.isNull()) {
      x = temp;
      hasX = true;
    }
    temp = doc["y"];
    if (!temp.isNull()) {
      y = temp;
      hasY = true;
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
  Cmd setMoveToXY(JsonDocument& doc) {
    JsonVariant temp;
    bool hasX = false;
    bool hasY = false;
    temp = doc["x"];
    if (!temp.isNull()) {
      x = temp;
      hasX = true;
    }
    temp = doc["y"];
    if (!temp.isNull()) {
      y = temp;
      hasY = true;
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

  Cmd setFocusAndTrigger(JsonDocument& doc) {
    JsonVariant temp;
    bool hasFocus = false;
    bool hasTrigger = false;

    temp = doc["focus"];
    if (!temp.isNull()) {
      focus = temp;
      hasFocus = true;
    }
    temp = doc["trigger"];
    if (!temp.isNull()) {
      trigger = temp;
      hasTrigger = true;
    }

    if (hasFocus && hasTrigger) {
      return FOCUS_AND_TRIGGER;
    }
    if (hasFocus) {
      return FOCUS;
    }
    if (hasTrigger) {
      return TRIGGER;
    }
    return ERROR;
  }

  void set(JsonDocument& doc) {
    valid = false;

    JsonVariant temp;
    temp = doc["nr"];
    if (!temp.isNull()) {
      nr = temp;
    } else {
      nr = 0;
    }

    temp = doc["cmd"];
    if (!temp.isNull()) {
      String c = temp.as<const char*>();
      c.toLowerCase();
      if (c == "move") {
        cmd = setMoveXY(doc);
        valid = cmd != Cmd::ERROR;

      } else if (c == "moveto") {
        cmd = setMoveToXY(doc);
        valid = cmd != Cmd::ERROR;

      } else if (c == "stop") {
        cmd = Cmd::STOP;
        valid = true;

      } else if (c == "shot") {
        cmd = setFocusAndTrigger(doc);
        valid = cmd != Cmd::ERROR;

      } else {
        cmd = Cmd::UNKNOWN;
      }
    }
  }

  String toString() {
    String temp = "jsonCommand(";
    temp = temp + "nr: " + nr;
    temp = temp + ", cmd: " + commandToName(cmd);
    temp = temp + ", x: " + x;
    temp = temp + ", y: " + y;
    temp = temp + ", focus: " + focus;
    temp = temp + ", trigger: " + trigger;
    temp = temp + ", valid: " + valid;
    temp = temp + ")";
    return temp;
  }

  int32_t nr;
  Cmd cmd;
  int32_t x;
  int32_t y;
  bool focus;
  bool trigger;
  bool valid;
};

class JsonCommandParser {
 public:
  JsonCommandParser(Stream& _console) : console(_console) {}
  DeserializationError parse(const char* text, JsonCommand& command) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, text);

    if (error) {
      console.print("[CMD] Error parsing command: ");
      console.println(error.c_str());
      console.print(text);
    } else {
      command.set(doc);
      // console.print("[CMD] ");
      // console.println(command.toString());
    }
    return error;
  }

 private:
  Stream& console;
};
