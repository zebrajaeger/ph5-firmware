#pragma once

#include <Arduino.h>

const String websocketUrl("/ws");
uint16_t websocketPort = 80;

// present broadcast
bool sendPresentBroadcast = true;
uint16_t presentBroadcastPeriodMs = 1000;

// Mqtt status
bool sendMqttStatus = false;
uint16_t mqttStatusSlowPeriodMs = 5000;
uint16_t mqttStatusFastPeriodMs = 500;

// UDP/Websocket status
bool sendBroadcastStatus = false;
bool sendWebsocketStatus = true;
uint16_t broadcastStatePeriodMs = 100;
uint16_t broadcastPort = 1666;

// command
bool receiveMqttCommands = false;
bool receiveBroadcastCommands = true;
uint16_t cmdPort = 1667;

//  calculated
bool enableMqtt = sendMqttStatus || receiveMqttCommands;