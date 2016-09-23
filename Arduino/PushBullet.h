/*
  PushBullet.h - Arduino library for sending a pushbullet push.
  Copyright 2016 Greg Cunningham, CuriousTech.net

  This library is free software; you can redistribute it and/or modify it under the terms of the GNU GPL 2.1 or later.
*/
#ifndef PUSHBULLET_H
#define PUSHBULLET_H

#include <Arduino.h>
#include <ESPAsyncTCP.h> // https://github.com/me-no-dev/ESPAsyncTCP  Set ASYNC_TCP_SSL_ENABLED 1 in async_config.h
                         // esp8266 must be >2.3.0
class PushBullet
{
public:
  PushBullet();
  bool  send(const char *pTitle, String sBody, const char *pToken);

private:
  char m_szTitle[64];
  char m_szBody[128];
  char m_szToken[40];
  AsyncClient m_ac;
  void _onConnect(AsyncClient* client);
  void _onDisconnect(AsyncClient* client);
  void _onError(AsyncClient* client, int8_t error);
  void _onTimeout(AsyncClient* client, uint32_t time);
  void _onData(AsyncClient* client, char* data, size_t len);
};

#endif // PUSHBULLET_H
