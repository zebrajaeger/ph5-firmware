#include "ph5.h"

#include "config.h"
#include "timer.h"

// Network log
#include <TelnetSpy.h>
TelnetSpy serialAndTelnet;

//  WS
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
AsyncWebServer server(websocketPort);
AsyncWebSocket ws(websocketUrl);

// DoubleresetDetector
#define DOUBLERESETDETECTOR_DEBUG false

// config
#include <Preferences.h>
Preferences preferences;
#define PREF_WIFI_1_SSID "wifi-1-ssid"
#define PREF_WIFI_1_PW "wifi-1-pw"
#define PREF_WIFI_2_SSID "wifi-2-ssid"
#define PREF_WIFI_2_PW "wifi-2-pw"

#define PREF_MQTT_1_HOST "mqtt-1-host"
#define PREF_MQTT_1_PORT "mqtt-1-port"
#define PREF_MQTT_2_HOST "mqtt-2-host"
#define PREF_MQTT_2_PORT "mqtt-2-port"

#define PREF_ROTATE_DISPLAY "rotdisp"
#define PREF_SWITCH_XY "swxy"

// WIFIManager
#define _ESP_WM_LITE_LOGLEVEL_ 0
// #define USE_DYNAMIC_PARAMETERS true
#define RESET_IF_CONFIG_TIMEOUT false
#define USING_BOARD_NAME true
#define SCAN_WIFI_NETWORKS true
#define MAX_SSID_IN_LIST 10
#define REQUIRE_ONE_SET_SSID_PW true
#include <ESPAsync_WiFiManager_Lite.h>
const int mqttPort = 1883;
uint8_t mqttServerIndex = 0;

// #include "credentials.h"
bool LOAD_DEFAULT_CONFIG_DATA = false;
ESP_WM_LITE_Configuration defaultConfig;

String deviceName = F("actor");
String AP_SSID = F("actor-AP");
ESPAsync_WiFiManager_Lite* ESPAsync_WiFiManager;

// OTA
#include <ArduinoOTA.h>

// MQTT
#include <PubSubClient.h>
#include <WiFi.h>

#include "ph5command.h"
String mqttPrefix = F("ph5");
WiFiClient espClient;
PubSubClient mqtt(espClient);
JsonCommandParser jsonCommandParser(serialAndTelnet, 1000, 1000);
unsigned long mqttNextConnectionAttempt = 0;

// Stepper
#define STEPPER_X_ENABLE_PIN 32
#define STEPPER_X_STEP_PIN 19
#define STEPPER_X_DIR_PIN 18
#define STEPPER_Y_ENABLE_PIN 33
#define STEPPER_Y_STEP_PIN 23
#define STEPPER_Y_DIR_PIN 22
#include <FastAccelStepper.h>
FastAccelStepperEngine engine = FastAccelStepperEngine();
FastAccelStepper* stepperX = NULL;
FastAccelStepper* stepperY = NULL;
bool isRunningX = false;
bool isRunningY = false;

// I2C
#define I2C_SDA_PIN 14
#define I2C_SCL_PIN 27

// Display
// #define DISPLAY_DEBUG true
#define DISPLAY_ADDRESS 0x3C
#include <Adafruit_SSD1306.h>
// select u8g2 font from here: https://github.com/olikraus/u8g2/wiki/fntlistall
#include <U8g2_for_Adafruit_GFX.h>
#include <Wire.h>
U8G2_FOR_ADAFRUIT_GFX u8g2;
Adafruit_SSD1306 display(128, 64, &Wire, -1);
bool displayUpdateRequired = true;
unsigned long displayLastUpdate = 0;

// Status (MQTT)
#include "jsonStatus.h"
class StateTimer : public IntervalTimer {
  virtual void onTimer() {
    String tmp;
    JsonStatus::convert(stepperX, stepperY, tmp);
    serialAndTelnet.print("[Status] Actor:");
    serialAndTelnet.println(tmp);
    mqtt.publish((mqttPrefix + "/actor").c_str(), tmp.c_str());
  }
};

StateTimer stateTimer;

// UDP
#include "AsyncUDP.h"
AsyncUDP udp;

// Broadcast (UDP)
class PresentBroadcastTimer : public IntervalTimer {
  virtual void onTimer() { udp.broadcastTo((uint8_t*)"ph5\0p\0\0\0", 8, broadcastPort); }
};
PresentBroadcastTimer presentBroadcastTimer;

// Status (UDP)
struct WSStatusMsg {
  int32_t x;
  int32_t y;
  uint32_t active;
};

struct UdpStatusMsg {
  const char magicHeader[8] = {'p', 'h', '5', 0, 's', 0, 0, 0};
  WSStatusMsg data;
};

void notifyClients(const WSStatusMsg& msg);

class BroadcastStatusTimer : public IntervalTimer {
  virtual void onTimer() {
    if (!stepperX || !stepperY) {
      return;
    }

    UdpStatusMsg msg;
    msg.data.x = stepperX->getCurrentPosition();
    msg.data.y = stepperY->getCurrentPosition();
    msg.data.active = stepperX->isRunning() | stepperY->isRunning() << 1;

    if (sendBroadcastStatus) {
      udp.broadcastTo((uint8_t*)&msg, sizeof(UdpStatusMsg), broadcastPort);
    }
    if (sendWebsocketStatus) {
      notifyClients(msg.data);
    }
  }
};

BroadcastStatusTimer broadcastStatusTimer;

// CLI
#include <SimpleCLI.h>
SimpleCLI cli;
Command cmdHelp;
Command cmdWiFi;
Command cmdMqtt;
Command cmdMoveX;
Command cmdMoveY;
Command cmdMove;
Command cmdRestart;
Command cmdRotateDisplay;
Command cmdSwitchXY;
Command cmdPrint;
#define CLI_BUFFER_SIZE 128
u8_t cliBufferIndex = 0;
char cliBuffer[CLI_BUFFER_SIZE];

// PH5 Firmware

void telnetConnected() { serialAndTelnet.println(F("[Telnet] connection established.")); }

void telnetDisconnected() { serialAndTelnet.println(F("[Telnet] connection closed.")); }

void disconnectClientWrapper() { serialAndTelnet.disconnectClient(); }

void processCommand(Ph5Command* cmd) {
  serialAndTelnet.print("[CMD] ");
  serialAndTelnet.println(cmd->toString());

  displayUpdateRequired = true;
  switch (cmd->getType()) {
    case Ph5CommandType::MOVE_X: {
      if (stepperX) {
        if (stepperX->setSpeedInHz(cmd->getSpeedX()) == 0) {
          isRunningX = stepperX->move(cmd->getPosX()) == MOVE_OK;
          serialAndTelnet.print("[Stepper] MoveX (1): ");
          serialAndTelnet.println(isRunningX);
          stateTimer.forceTrigger();
        }
      }
      break;
    }
    case Ph5CommandType::MOVE_Y: {
      if (stepperY) {
        if (stepperY->setSpeedInHz(cmd->getSpeedY()) == 0) {
          isRunningY = stepperY->move(cmd->getPosX()) == MOVE_OK;
          serialAndTelnet.print("[Stepper] MoveY (1): ");
          serialAndTelnet.println(isRunningY);
          stateTimer.forceTrigger();
        }
      }
      break;
    }
    case Ph5CommandType::MOVE_XY: {
      if (stepperX) {
        if (stepperX->setSpeedInHz(cmd->getSpeedX()) == 0) {
          isRunningX = stepperX->move(cmd->getPosX()) == MOVE_OK;
          serialAndTelnet.print("[Stepper] MoveX (2): ");
          serialAndTelnet.println(isRunningX);
          stateTimer.forceTrigger();
        }
      }
      if (stepperY) {
        if (stepperY->setSpeedInHz(cmd->getSpeedY()) == 0) {
          isRunningY = stepperY->move(cmd->getPosY()) == MOVE_OK;
          serialAndTelnet.print("[Stepper] MoveY (2): ");
          serialAndTelnet.println(isRunningY);
          stateTimer.forceTrigger();
        }
      }
      break;
    }

    case Ph5CommandType::SPEED_X: {
      if (stepperX) {
        if (cmd->getSpeedX() > 0) {
          if (stepperX->setSpeedInHz(cmd->getSpeedX()) == 0) {
            isRunningX = stepperX->runForward() == MOVE_OK;
          }
        } else {
          if (stepperX->setSpeedInHz(-cmd->getSpeedX()) == 0) {
            isRunningX = stepperX->runBackward() == MOVE_OK;
          }
        }
        serialAndTelnet.println("[Stepper] SpeedX");
        stateTimer.forceTrigger();
      }
      break;
    }
    case Ph5CommandType::SPEED_Y: {
      if (stepperY) {
        if (cmd->getSpeedY() > 0) {
          if (stepperY->setSpeedInHz(cmd->getSpeedY()) == 0) {
            isRunningY = stepperY->runForward() == MOVE_OK;
          }
        } else {
          if (stepperY->setSpeedInHz(-cmd->getSpeedY()) == 0) {
            isRunningY = stepperY->runBackward() == MOVE_OK;
          }
        }
        serialAndTelnet.println("[Stepper] SpeedY");
        stateTimer.forceTrigger();
      }
      break;
    }
    case Ph5CommandType::SPEED_XY: {
      if (stepperX) {
        if (cmd->getSpeedX() > 0) {
          if (stepperX->setSpeedInHz(cmd->getSpeedX()) == 0) {
            isRunningX = stepperX->runForward() == MOVE_OK;
          }
        } else {
          if (stepperX->setSpeedInHz(-cmd->getSpeedX()) == 0) {
            isRunningX = stepperX->runBackward() == MOVE_OK;
          }
        }
        serialAndTelnet.println("[Stepper] SpeedX");
        stateTimer.forceTrigger();
      }
      if (stepperY) {
        if (cmd->getSpeedY() > 0) {
          if (stepperY->setSpeedInHz(cmd->getSpeedY()) == 0) {
            isRunningY = stepperY->runForward() == MOVE_OK;
          }
        } else {
          if (stepperY->setSpeedInHz(-cmd->getSpeedY()) == 0) {
            isRunningY = stepperY->runBackward() == MOVE_OK;
          }
        }
        serialAndTelnet.println("[Stepper] SpeedY");
        stateTimer.forceTrigger();
      }
      break;
    }

    case Ph5CommandType::MOVE_TO_X: {
      if (stepperX) {
        if (stepperX->setSpeedInHz(cmd->getSpeedX()) == 0) {
          isRunningX = stepperX->moveTo(cmd->getPosX()) == MOVE_OK;
          serialAndTelnet.println("[Stepper] MoveToX");
          stateTimer.forceTrigger();
        }
      }
      break;
    }
    case Ph5CommandType::MOVE_TO_Y: {
      if (stepperY) {
        if (stepperY->setSpeedInHz(cmd->getSpeedY()) == 0) {
          isRunningY = stepperY->moveTo(cmd->getPosY()) == MOVE_OK;
          serialAndTelnet.println("[Stepper] MoveToY");
          stateTimer.forceTrigger();
        }
      }
      break;
    }
    case Ph5CommandType::MOVE_TO_XY: {
      if (stepperX) {
        if (stepperX->setSpeedInHz(cmd->getSpeedX()) == 0) {
          isRunningX = stepperX->moveTo(cmd->getPosX()) == MOVE_OK;
          serialAndTelnet.println("[Stepper] MoveToX");
          stateTimer.forceTrigger();
        }
      }
      if (stepperY) {
        if (stepperY->setSpeedInHz(cmd->getSpeedY()) == 0) {
          isRunningY = stepperY->moveTo(cmd->getPosX()) == MOVE_OK;
          serialAndTelnet.println("[Stepper] MoveToY");
          stateTimer.forceTrigger();
        }
      }
      break;
    }

    case Ph5CommandType::STOP_X: {
      if (stepperX) {
        stepperX->stopMove();
      }
      stateTimer.forceTrigger();
      break;
    }
    case Ph5CommandType::STOP_Y: {
      if (stepperY) {
        stepperY->stopMove();
      }
      stateTimer.forceTrigger();
      break;
    }
    case Ph5CommandType::STOP_XY: {
      if (stepperX) {
        stepperX->stopMove();
      }
      if (stepperY) {
        stepperY->stopMove();
      }
      stateTimer.forceTrigger();
      break;
    }

    case Ph5CommandType::FORCE_STOP: {
      if (stepperX) {
        stepperX->forceStop();
      }
      if (stepperY) {
        stepperY->forceStop();
      }
      stateTimer.forceTrigger();
      break;
    }

    case Ph5CommandType::SET_POS_X: {
      if (stepperX) {
        if (!stepperX->isRunning()) {
          stepperX->setCurrentPosition(cmd->getPosX());
          stateTimer.forceTrigger();
        }
      }
      break;
    }
    case Ph5CommandType::SET_POS_Y: {
      if (stepperY) {
        if (!stepperY->isRunning()) {
          stepperY->setCurrentPosition(cmd->getPosY());
          displayUpdateRequired = true;
          stateTimer.forceTrigger();
        }
      }
      break;
    }
    case Ph5CommandType::SET_POS_XY: {
      if (stepperX) {
        if (!stepperX->isRunning()) {
          stepperX->setCurrentPosition(cmd->getPosX());
          displayUpdateRequired = true;
          stateTimer.forceTrigger();
        }
      }
      if (stepperY) {
        if (!stepperY->isRunning()) {
          stepperY->setCurrentPosition(cmd->getPosY());
          displayUpdateRequired = true;
          stateTimer.forceTrigger();
        }
      }
      break;
    }

    case Ph5CommandType::PING:
      cmd->setType(Ph5CommandType::PONG);
      break;

    case Ph5CommandType::UNKNOWN:
      // we ignore it?
      break;

    default:
      // TODO do something. Logging?
      break;
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char msg[length + 1];
  strncpy(msg, (char*)payload, length);
  msg[length] = 0;
  serialAndTelnet.print("[MQTT] Received Message: '");
  serialAndTelnet.print(topic);
  serialAndTelnet.print("', l:");
  serialAndTelnet.print(length);
  serialAndTelnet.print(", msg:'");
  serialAndTelnet.print(msg);
  serialAndTelnet.println("'");

  Ph5Command cmd;
  if (!jsonCommandParser.parse((char*)payload, cmd)) {
    processCommand(&cmd);
  } else {
    serialAndTelnet.println("[JSON] Error: Command is invalid");
  }
}

namespace UdpPacket {
bool isMagicHeaderPh5(const uint8_t* data) {
  return data[0] == 'p' && data[1] == 'h' && data[2] == '5' && data[3] == 0;
}
bool isMagicHeaderCommand(const uint8_t* data) { return data[4] == 'c' && data[5] == 0; }
bool isMagicHeaderConfig(const uint8_t* data) { return data[4] == 'o' && data[5] == 0; }
const uint8_t* getDataPointer(const uint8_t* data) { return &(data[6]); }
}  // namespace UdpPacket

void commandReceiverSetup() {
  if (udp.listen(cmdPort)) {
    udp.onPacket([](AsyncUDPPacket packet) {
      // we accept only unicast packets
      if (!packet.isBroadcast() && !packet.isMulticast()) {
        // with the correct length
        if (packet.length() == sizeof(Command)) {
          // Only with the magic header
          if (UdpPacket::isMagicHeaderPh5(packet.data())) {
            // And only commands so far
            if (UdpPacket::isMagicHeaderCommand(packet.data())) {
              Ph5Command* cmd = (Ph5Command*)packet.data();
              processCommand(cmd);
              // send back as 'ack'
              udp.writeTo(packet.data(), packet.length(), packet.remoteIP(), cmdPort);
            } else if (UdpPacket::isMagicHeaderConfig(packet.data())) {
              udp.writeTo(packet.data(), packet.length(), packet.remoteIP(), cmdPort);
            }
          }
        }
      };
    });
  }
}

void wiFiEvent(WiFiEvent_t event) {
  displayUpdateRequired = true;
  serialAndTelnet.printf("[-WiFi-] event: %d\n", event);
  switch (event) {
    case SYSTEM_EVENT_WIFI_READY:
      serialAndTelnet.println("[-WiFi-] WiFi interface ready");
      break;
    case SYSTEM_EVENT_SCAN_DONE:
      serialAndTelnet.println("[-WiFi-] Completed scan for access points");
      break;
    case SYSTEM_EVENT_STA_START:
      serialAndTelnet.println("[-WiFi-] WiFi client started");
      break;
    case SYSTEM_EVENT_STA_STOP:
      serialAndTelnet.println("[-WiFi-] WiFi clients stopped");
      break;
    case SYSTEM_EVENT_STA_CONNECTED:
      serialAndTelnet.println("[-WiFi-] Connected to access point");
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      serialAndTelnet.println("[-WiFi-] Disconnected from WiFi access point");
      break;
    case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:
      serialAndTelnet.println("[-WiFi-] Authentication mode of access point has changed");
      break;
    case SYSTEM_EVENT_STA_GOT_IP:
      serialAndTelnet.printf("[-WiFi-] Obtained IP address: %s\n", WiFi.localIP().toString());
      break;
    case SYSTEM_EVENT_STA_LOST_IP:
      serialAndTelnet.println("[-WiFi-] Lost IP address and IP address is reset to 0");
      //      vTaskDelay( 5000 );
      //      ESP.restart();
      break;
    case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:
      serialAndTelnet.println("[-WiFi-] WiFi Protected Setup (WPS): succeeded in enrollee mode");
      break;
    case SYSTEM_EVENT_STA_WPS_ER_FAILED:
      serialAndTelnet.println("[-WiFi-] WiFi Protected Setup (WPS): failed in enrollee mode");
      //      ESP.restart();
      break;
    case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:
      serialAndTelnet.println("[-WiFi-] WiFi Protected Setup (WPS): timeout in enrollee mode");
      break;
    case SYSTEM_EVENT_STA_WPS_ER_PIN:
      serialAndTelnet.println("[-WiFi-] WiFi Protected Setup (WPS): pin code in enrollee mode");
      break;
    case SYSTEM_EVENT_AP_START:
      serialAndTelnet.println("[-WiFi-] WiFi access point started");
      break;
    case SYSTEM_EVENT_AP_STOP:
      serialAndTelnet.println("[-WiFi-] WiFi access point  stopped");
      break;
    case SYSTEM_EVENT_AP_STACONNECTED:
      serialAndTelnet.println("[-WiFi-] Client connected");
      break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
      serialAndTelnet.println("[-WiFi-] WiFi client disconnected");
      break;
    case SYSTEM_EVENT_AP_STAIPASSIGNED:
      serialAndTelnet.println("[-WiFi-] Assigned IP address to client");
      break;
    case SYSTEM_EVENT_AP_PROBEREQRECVED:
      serialAndTelnet.printf("[-WiFi-] Received probe request");
      break;
    case SYSTEM_EVENT_GOT_IP6:
      serialAndTelnet.println("[-WiFi-] IPv6 is preferred");
      break;
    case SYSTEM_EVENT_ETH_GOT_IP:
      serialAndTelnet.println("[-WiFi-] ETH_GOT_IP");
      break;
    default:
      break;
  }
}

void scanI2C() {
  int nDevices;

  serialAndTelnet.println("[I2C] Scanning...");

  nDevices = 0;
  for (uint8_t address = 1; address < 127; address++) {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();

    if (error == 0) {
      serialAndTelnet.print("[I2C] I2C device found at address 0x");
      if (address < 16) Serial.print("0");
      serialAndTelnet.print(address, HEX);
      serialAndTelnet.println("  !");

      nDevices++;
    } else if (error == 4) {
      serialAndTelnet.print("[I2C] Unknown error at address 0x");
      if (address < 16) serialAndTelnet.print("0");
      serialAndTelnet.println(address, HEX);
    }
  }
  if (nDevices == 0)
    serialAndTelnet.println("[I2C] No I2C devices found");
  else
    serialAndTelnet.println("[I2C] done");
}

void handleMqtt() {
  if (!mqtt.connected() && WiFi.isConnected() &&
      (mqttNextConnectionAttempt == 0 || mqttNextConnectionAttempt < millis())) {
    // server 1 or server 2
    String host = preferences.getString(mqttServerIndex == 0 ? PREF_MQTT_1_HOST : PREF_MQTT_2_HOST);
    mqtt.setServer(host.c_str(), 1883);  // TODO make port configurable
    serialAndTelnet.print("[MQTT] Try connect to MQTT Server ");
    serialAndTelnet.println(host);

    mqttNextConnectionAttempt = millis() + 5000;  // every 5s
    if (mqtt.connect(deviceName.c_str())) {
      serialAndTelnet.println("[MQTT] connected");

      // Subscribe to command topic
      serialAndTelnet.print("[MQTT] subscribe to '");
      serialAndTelnet.print(mqttPrefix + "/cmd");
      serialAndTelnet.println("'");
      mqtt.subscribe((mqttPrefix + "/cmd").c_str());

    } else {
      serialAndTelnet.print("[MQTT] Connection error, rc=");
      serialAndTelnet.print(mqtt.state());

      if (mqttServerIndex == 0) {
        mqttServerIndex = 1;
      } else {
        mqttServerIndex = 0;
      }
      serialAndTelnet.print(" retry with next server");
    }
    displayUpdateRequired = true;
  }

  mqtt.loop();
}

void handleDisplay() {
  if (!displayUpdateRequired) {
    return;
  }
  if (displayLastUpdate > millis()) {
    return;
  }
#ifdef DISPLAY_DEBUG
  serialAndTelnet.println("[Display] Update");
#endif
  displayUpdateRequired = false;
  displayLastUpdate = millis() + 100;  // max 10Hz

  display.clearDisplay();
  u8g2.setForegroundColor(WHITE);
  u8g2.setBackgroundColor(BLACK);
  u8g2.setFont(u8g2_font_helvR10_tf);

  // WiFi Mode
  u8g2.setCursor(0, 15);
  if (WiFi.getMode() == wifi_mode_t::WIFI_MODE_AP) {
    u8g2.print("AP  ");
  } else if (WiFi.getMode() == wifi_mode_t::WIFI_MODE_STA) {
    u8g2.print("STA ");
  } else if (WiFi.getMode() == wifi_mode_t::WIFI_MODE_APSTA) {
    u8g2.print("AS  ");
  } else if (WiFi.getMode() == wifi_mode_t::WIFI_MODE_MAX) {
    u8g2.print("MAX ");
  } else if (WiFi.getMode() == wifi_mode_t::WIFI_MODE_NULL) {
    u8g2.print("NUL ");
  } else {
    u8g2.print("??? ");
  }

  // WiFi IP
  u8g2.setFont(u8g2_font_helvR08_tf);
  u8g2.print(WiFi.localIP().toString());

  // MQTT
  if (enableMqtt) {
    u8g2.setCursor(0, 30);
    u8g2.print("M ");
    if (WiFi.isConnected()) {
      if (mqtt.connected()) {
        u8g2.print(preferences.getString(mqttServerIndex == 0 ? PREF_MQTT_1_HOST : PREF_MQTT_2_HOST));
      } else {
        u8g2.print("[");
        u8g2.print(preferences.getString(mqttServerIndex == 0 ? PREF_MQTT_1_HOST : PREF_MQTT_2_HOST));
        u8g2.print("]");
      }
    } else {
      u8g2.print("-");
    }
  }

  // X
  if (stepperX) {
    if (stepperX->isRunning()) {
      u8g2.setForegroundColor(BLACK);
      u8g2.setBackgroundColor(WHITE);
    } else {
      u8g2.setForegroundColor(WHITE);
      u8g2.setBackgroundColor(BLACK);
    }
    u8g2.setCursor(0, 50);
    u8g2.setFont(u8g2_font_helvR10_tf);
    u8g2.print("x");
    u8g2.setCursor(20, 50);
    u8g2.setFont(u8g2_font_helvR08_tf);
    u8g2.setForegroundColor(WHITE);
    u8g2.setBackgroundColor(BLACK);
    u8g2.print(stepperX->getCurrentPosition());
  }

  // Y
  if (stepperY) {
    if (stepperY->isRunning()) {
      u8g2.setForegroundColor(BLACK);
      u8g2.setBackgroundColor(WHITE);
    } else {
      u8g2.setForegroundColor(WHITE);
      u8g2.setBackgroundColor(BLACK);
    }
    u8g2.setCursor(0, 63);
    u8g2.setFont(u8g2_font_helvR10_tf);
    u8g2.print("y");
    u8g2.setCursor(20, 63);
    u8g2.setForegroundColor(WHITE);
    u8g2.setBackgroundColor(BLACK);
    u8g2.setFont(u8g2_font_helvR08_tf);
    u8g2.print(stepperY->getCurrentPosition());
  }

  display.display();
}

void cliHelpCallback(cmd* c) {
  serialAndTelnet.println("wifi  (-nr [1|2]) -ssid <ssid> -pw <password>   set WiFi credentials");
  serialAndTelnet.println("mqtt  (-nr [1|2]) -host <host> -port <port>     set MQTT Server config");
  serialAndTelnet.println("movex <x>                                       move steps in x direction");
  serialAndTelnet.println("movey <y>                                       move steps in y direction");
  serialAndTelnet.println("move <x> <y>                                    move steps in x,y or xy direction");
  serialAndTelnet.println("restart                                         restart device");
  serialAndTelnet.println("rotdisp                                         invert display rotation");
  serialAndTelnet.println("switchxy                                        switch x and y axis");
  serialAndTelnet.println("print                                           print configuration");
  serialAndTelnet.println("help                                            show help");
}

void cliMqttCallback(cmd* c) {
  Command cmd(c);

  long nr = cmd.getArgument("nr").getValue().toInt();
  String host = cmd.getArgument("host").getValue();

  if (nr == 1) {
    serialAndTelnet.print("Set mqttServer1 to ");
    serialAndTelnet.println(host);
    preferences.putString(PREF_MQTT_1_HOST, host);
  } else if (nr == 2) {
    serialAndTelnet.print("Set mqttServer2 to ");
    serialAndTelnet.println(host);
    preferences.putString(PREF_MQTT_2_HOST, host);
  } else {
    serialAndTelnet.println("nr out of range [1..2]");
  }
}

void cliWifiCallback(cmd* c) {
  Command cmd(c);

  Argument nr = cmd.getArgument("nr");
  Argument ssid = cmd.getArgument("ssid");
  Argument pw = cmd.getArgument("pw");
  int nrVal = nr.getValue().toInt();
  String ssidVal = ssid.getValue();
  String pwVal = pw.getValue();

  if (nrVal == 1) {
    ESPAsync_WiFiManager->setWiFiCredentials(0, ssidVal, pwVal);
    preferences.putString(PREF_WIFI_1_SSID, ssidVal);
    preferences.putString(PREF_WIFI_1_PW, pwVal);
  } else if (nrVal == 2) {
    ESPAsync_WiFiManager->setWiFiCredentials(1, ssidVal, pwVal);
    preferences.putString(PREF_WIFI_2_SSID, ssidVal);
    preferences.putString(PREF_WIFI_2_PW, pwVal);
  }
}

void cliMoveCallback(cmd* c) {
  Command cmd(c);
  Argument x = cmd.getArgument("x");
  Argument y = cmd.getArgument("y");

  Ph5Command jc;
  if (x.isSet() && y.isSet()) {
    jc.setType(Ph5CommandType::MOVE_XY);
    jc.setPosX(x.getValue().toInt());
    jc.setSpeedX(1000);
    jc.setPosY(y.getValue().toInt());
    jc.setSpeedY(1000);
    processCommand(&jc);

  } else if (x.isSet()) {
    jc.setType(Ph5CommandType::MOVE_X);
    jc.setPosX(x.getValue().toInt());
    jc.setSpeedX(1000);
    processCommand(&jc);

  } else if (y.isSet()) {
    jc.setType(Ph5CommandType::MOVE_Y);
    jc.setPosY(y.getValue().toInt());
    jc.setSpeedY(1000);
    processCommand(&jc);
  }
}

void cliPrintConfig(cmd* c) {
  serialAndTelnet.println("+--------------------------------+");
  serialAndTelnet.println("| Config                         |");
  serialAndTelnet.println("+--------------------------------+");
  serialAndTelnet.print("mqttServer1: ");
  serialAndTelnet.println(preferences.getString(PREF_MQTT_1_HOST));
  serialAndTelnet.print("mqttServer2: ");
  serialAndTelnet.println(preferences.getString(PREF_MQTT_2_HOST));
  serialAndTelnet.print("rotateDisplay: ");
  serialAndTelnet.println(preferences.getBool(PREF_ROTATE_DISPLAY) ? "true" : "false");
  serialAndTelnet.print("switchXY: ");
  serialAndTelnet.println(preferences.getBool(PREF_SWITCH_XY) ? "true" : "false");
  serialAndTelnet.print("SSID1: ");
  serialAndTelnet.println(ESPAsync_WiFiManager->getWiFiSSID(0));
  serialAndTelnet.print("PW1: ");
  serialAndTelnet.println(ESPAsync_WiFiManager->getWiFiPW(0));
  serialAndTelnet.print("SSID2: ");
  serialAndTelnet.println(ESPAsync_WiFiManager->getWiFiSSID(1));
  serialAndTelnet.print("PW2: ");
  serialAndTelnet.println(ESPAsync_WiFiManager->getWiFiPW(1));
  serialAndTelnet.print("localIP:");
  serialAndTelnet.println(ESPAsync_WiFiManager->localIP());
}

void cliRestart(cmd* c) { ESP.restart(); }

void cliRotateDisplay(cmd* c) {
  if (preferences.getBool(PREF_ROTATE_DISPLAY)) {
    display.setRotation(0);
    serialAndTelnet.println("The display is not rotated");
    serialAndTelnet.println(preferences.getBool(PREF_ROTATE_DISPLAY));
    preferences.putBool(PREF_ROTATE_DISPLAY, false);
    serialAndTelnet.println(preferences.getBool(PREF_ROTATE_DISPLAY));
  } else {
    serialAndTelnet.println("The display is now rotated by 180 degrees");
    display.setRotation(2);
    serialAndTelnet.println(preferences.getBool(PREF_ROTATE_DISPLAY));
    preferences.putBool(PREF_ROTATE_DISPLAY, true);
    serialAndTelnet.println(preferences.getBool(PREF_ROTATE_DISPLAY));
  }
  displayUpdateRequired = true;
}

void cliSwitchXY(cmd* c) {
  if (preferences.getBool(PREF_SWITCH_XY)) {
    preferences.putBool(PREF_SWITCH_XY, false);
    serialAndTelnet.println("X and Y axis normal (requires restart)");
  } else {
    preferences.putBool(PREF_SWITCH_XY, true);
    serialAndTelnet.println("X and Y axis switched (requires restart)");
  }
}

void processCmdChar(char c) {
  if (cliBufferIndex < CLI_BUFFER_SIZE - 1) {
    // within buffer
    if ('\r' == c) {
      // ignore
    } else if ('\n' == c) {
      // end of line. terminate and parse
      cliBuffer[cliBufferIndex] = 0;
      serialAndTelnet.print("# ");
      serialAndTelnet.println(cliBuffer);
      cli.parse(cliBuffer);
      cliBufferIndex = 0;

      if (cli.errored()) {
        CommandError cmdError = cli.getError();

        serialAndTelnet.print("ERROR: ");
        serialAndTelnet.println(cmdError.toString());

        if (cmdError.hasCommand()) {
          serialAndTelnet.print("Did you mean \"");
          serialAndTelnet.print(cmdError.getCommand().toString());
          serialAndTelnet.println("\"?");
        }
      }

    } else {
      // write char
      cliBuffer[cliBufferIndex] = c;
      cliBufferIndex++;
    }
  } else if (cliBufferIndex == CLI_BUFFER_SIZE - 1) {
    // end of buffer, close with zero
    cliBuffer[cliBufferIndex] = 0;
    cliBufferIndex++;

  } else {
    // buffer left -> ignore
  }
}

void notifyClients(const WSStatusMsg& msg) { ws.binaryAll((const char*)&msg, sizeof(WSStatusMsg)); }

void handleWebSocketMessage(void* arg, uint8_t* data, size_t len) {
  AwsFrameInfo* info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_BINARY) {
    if (len == sizeof(Ph5Command)) {
      Ph5Command* command = (Ph5Command*)data;
      // Serial.println(command->toString());
      processCommand(command);
    }
  }
}

void onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data,
                      size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void setup() {
  // Serial and Telnet
  serialAndTelnet.setWelcomeMsg(F("Welcome to the TelnetSpy example\r\n\n"));
  serialAndTelnet.setCallbackOnConnect(telnetConnected);
  serialAndTelnet.setCallbackOnDisconnect(telnetDisconnected);
  serialAndTelnet.setFilter(char(1), F("\r\nNVT command: AO\r\n"), disconnectClientWrapper);
  serialAndTelnet.begin(115200);
  delay(100);

  serialAndTelnet.println("----------------------------------------");
  serialAndTelnet.print(appName);
  serialAndTelnet.print(" v");
  serialAndTelnet.print(appVersion);
  serialAndTelnet.print(" build ");
  serialAndTelnet.println(buildTime);
  serialAndTelnet.println("----------------------------------------");

  // I2C
  serialAndTelnet.println("[APP] Initialize I²C");
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  scanI2C();

  // Config
  preferences.begin("ph5");

  // Display
  serialAndTelnet.println("[APP] Initialize Screen");
  if (!display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_ADDRESS)) {
    serialAndTelnet.println(F("[SCREEN] Error: SSD1306 allocation failed"));
  } else {
    display.clearDisplay();
    if (preferences.getBool(PREF_ROTATE_DISPLAY)) {
      display.setRotation(2);
    }
    u8g2.begin(display);
    u8g2.setFont(u8g2_font_helvR10_tf);
    u8g2.setForegroundColor(WHITE);
    u8g2.setCursor(0, 15);
    u8g2.print(appName);
    u8g2.setCursor(0, 30);
    u8g2.print("v");
    u8g2.print(appVersion);
    u8g2.setCursor(0, 45);
    u8g2.print(buildTime);
    display.display();
  }

  // WIFI
  serialAndTelnet.println("[APP] Initialize WiFi");
  WiFi.onEvent(wiFiEvent);

  serialAndTelnet.print(F("[App] Starting ESPAsync_WiFi using "));
  serialAndTelnet.print(FS_Name);
  serialAndTelnet.print(F(" on "));
  serialAndTelnet.println(ARDUINO_BOARD);

  ESPAsync_WiFiManager = new ESPAsync_WiFiManager_Lite();
  ESPAsync_WiFiManager->setConfigPortal(AP_SSID, "");
  ESPAsync_WiFiManager->setWiFiCredentials(0, preferences.getString(PREF_WIFI_1_SSID),
                                           preferences.getString(PREF_WIFI_1_PW));
  ESPAsync_WiFiManager->setWiFiCredentials(1, preferences.getString(PREF_WIFI_2_SSID),
                                           preferences.getString(PREF_WIFI_2_PW));
  ESPAsync_WiFiManager->begin(deviceName.c_str());

  // OTA
  serialAndTelnet.println("[APP] Initialize OTA");
  ArduinoOTA.onStart([]() { serialAndTelnet.println("[OTA] Start"); });
  ArduinoOTA.onEnd([]() { serialAndTelnet.println("\n[OTA] End"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    serialAndTelnet.printf("[OTA] Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    serialAndTelnet.printf("[OTA] Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)
      serialAndTelnet.println("[OTA] Auth Failed");
    else if (error == OTA_BEGIN_ERROR)
      serialAndTelnet.println("[OTA] Begin Failed");
    else if (error == OTA_CONNECT_ERROR)
      serialAndTelnet.println("[OTA] Connect Failed");
    else if (error == OTA_RECEIVE_ERROR)
      serialAndTelnet.println("[OTA] Receive Failed");
    else if (error == OTA_END_ERROR)
      serialAndTelnet.println("[OTA] End Failed");
  });
  ArduinoOTA.begin();

  // WIFI
  serialAndTelnet.print("[App] IP address: ");
  serialAndTelnet.println(WiFi.localIP());

  // MQTT
  if (enableMqtt) {
    serialAndTelnet.println("[APP] Initialize MQTT");
    serialAndTelnet.print("[MQTT] server1: ");
    serialAndTelnet.println(preferences.getString(PREF_MQTT_1_HOST));
    serialAndTelnet.print("[MQTT] server2: ");
    serialAndTelnet.println(preferences.getString(PREF_MQTT_2_HOST));
    mqtt.setSocketTimeout(5);  // 5 seconds
    mqtt.setCallback(mqttCallback);
  }

  // Steppers
  serialAndTelnet.println("[APP] Initialize Steppers");

  engine.init();
  bool switchAxis = preferences.getBool(PREF_SWITCH_XY);
  if (switchAxis) {
    serialAndTelnet.println("[APP] Switch X and Y axis pins");
  }
  stepperX = engine.stepperConnectToPin(switchAxis ? STEPPER_Y_STEP_PIN : STEPPER_X_STEP_PIN);
  if (stepperX) {
    stepperX->setDirectionPin(switchAxis ? STEPPER_Y_DIR_PIN : STEPPER_X_DIR_PIN);
    stepperX->setEnablePin(switchAxis ? STEPPER_Y_ENABLE_PIN : STEPPER_X_ENABLE_PIN);
    stepperX->setSpeedInHz(3200);
    // stepperX->setLinearAcceleration(100);
    stepperX->setAcceleration(5000);
    stepperX->setAutoEnable(false);
    stepperX->enableOutputs();
    serialAndTelnet.println("[Stepper] Stepper X initialized");
  } else {
    serialAndTelnet.println("[Stepper] Error: Stepper X not initialized");
  }
  stepperY = engine.stepperConnectToPin(switchAxis ? STEPPER_X_STEP_PIN : STEPPER_Y_STEP_PIN);
  if (stepperY) {
    stepperY->setDirectionPin(switchAxis ? STEPPER_X_DIR_PIN : STEPPER_Y_DIR_PIN);
    stepperY->setEnablePin(switchAxis ? STEPPER_X_ENABLE_PIN : STEPPER_Y_ENABLE_PIN);
    stepperY->setSpeedInHz(3200);
    // stepperY->setLinearAcceleration(100);
    stepperY->setAcceleration(5000);
    stepperY->setAutoEnable(false);
    stepperY->enableOutputs();
    serialAndTelnet.println("[Stepper] Stepper Y initialized");
  } else {
    serialAndTelnet.println("[Stepper] Error: Stepper Y not initialized");
  }

  // State publishing
  if (sendMqttStatus) {
    serialAndTelnet.println("[APP] State timer");
    stateTimer.start(10000);
    stateTimer.forceTrigger();
  }

  // CLI
  cmdMqtt = cli.addCommand("mqtt", cliMqttCallback);
  cmdMqtt.addArgument("nr", "1");
  cmdMqtt.addArgument("host");
  cmdMqtt.addArgument("port", "1883");

  cmdWiFi = cli.addCommand("wifi", cliWifiCallback);
  cmdWiFi.addArgument("nr", "1");
  cmdWiFi.addArgument("ssid");
  cmdWiFi.addArgument("pw");

  cmdMoveX = cli.addCommand("movex", cliMoveCallback);
  cmdMoveX.addPositionalArgument("x", "0");
  cmdMoveY = cli.addCommand("movey", cliMoveCallback);
  cmdMoveY.addPositionalArgument("y", "0");
  cmdMove = cli.addCommand("move", cliMoveCallback);
  cmdMove.addPositionalArgument("x", "0");
  cmdMove.addPositionalArgument("y", "0");

  cmdPrint = cli.addCommand("print", cliPrintConfig);
  cmdRestart = cli.addCommand("restart", cliRestart);
  cmdRestart = cli.addCommand("reboot", cliRestart);

  cmdRotateDisplay = cli.addCommand("rotdisp", cliRotateDisplay);
  cmdSwitchXY = cli.addCommand("switchxy", cliSwitchXY);

  cmdHelp = cli.addCommand("help", cliHelpCallback);

  // UDP Present broadcast
  if (sendPresentBroadcast) {
    presentBroadcastTimer.start(presentBroadcastPeriodMs);
  }

  // UDP Status broadcast
  if (sendBroadcastStatus || sendWebsocketStatus) {
    broadcastStatusTimer.start(broadcastStatePeriodMs);
  }

  // UDP command receiver
  commandReceiverSetup();

  // WS
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) { request->send(200, "text/html", "Hello!"); });
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);
  server.begin();
}

unsigned long t = millis();

void loop() {
  ESPAsync_WiFiManager->run();

  ArduinoOTA.handle();
  serialAndTelnet.handle();

  // CLI
  if (serialAndTelnet.available()) {
    processCmdChar(serialAndTelnet.read());
  }

  if (enableMqtt) {
    handleMqtt();
  }
  // camera.handle();

  bool fastTimer = false;
  if (stepperX) {
    bool r = stepperX->isRunning();
    fastTimer |= r;
    if (r) {
      displayUpdateRequired = true;
    }
    if (r != isRunningX) {
      isRunningX = r;
      stateTimer.forceTrigger();
      broadcastStatusTimer.forceTrigger();
    }
  }

  if (stepperY) {
    bool r = stepperY->isRunning();
    fastTimer |= r;
    if (r) {
      displayUpdateRequired = true;
    }
    if (r != isRunningY) {
      isRunningY = r;
      stateTimer.forceTrigger();
      broadcastStatusTimer.forceTrigger();
      displayUpdateRequired = true;
    }
  }

  if (sendMqttStatus) {
    stateTimer.setPeriodMs(fastTimer ? mqttStatusSlowPeriodMs : mqttStatusFastPeriodMs);
    stateTimer.handle();
  }

  if (sendBroadcastStatus || sendWebsocketStatus) {
    broadcastStatusTimer.handle();
  }

  if (sendPresentBroadcast) {
    presentBroadcastTimer.handle();
  }

  handleDisplay();

  ws.cleanupClients();
}