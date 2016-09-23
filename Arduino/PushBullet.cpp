/*
  PushBullet.cpp - Arduino library for sending a pushbullet push.
  Copyright 2016 Greg Cunningham, CuriousTech.net

  This library is free software; you can redistribute it and/or modify it under the terms of the GNU GPL 2.1 or later.
*/

#include "PushBullet.h"

const char host[] = "api.pushbullet.com";
const char url[] = "/v2/pushes";

// Initialize instance with a callback (event list index, name index from 0, integer value, string value)
PushBullet::PushBullet()
{
  m_ac.onConnect([](void* obj, AsyncClient* c) { (static_cast<PushBullet*>(obj))->_onConnect(c); }, this);
  m_ac.onDisconnect([](void* obj, AsyncClient* c) { (static_cast<PushBullet*>(obj))->_onDisconnect(c); }, this);
  m_ac.onError([](void* obj, AsyncClient* c, int8_t error) { (static_cast<PushBullet*>(obj))->_onError(c, error); }, this);
  m_ac.onTimeout([](void* obj, AsyncClient* c, uint32_t time) { (static_cast<PushBullet*>(obj))->_onTimeout(c, time); }, this);
  m_ac.onData([](void* obj, AsyncClient* c, void* data, size_t len) { (static_cast<PushBullet*>(obj))->_onData(c, static_cast<char*>(data), len); }, this);

  m_ac.setRxTimeout(10000);
}

bool PushBullet::send(const char *pTitle, String sBody, const char *pToken)
{
  strncpy(m_szTitle, pTitle, sizeof(m_szTitle) );
  sBody.toCharArray(m_szBody, sizeof(m_szBody) );
  strncpy(m_szToken, pToken, sizeof(m_szToken) );

  if(m_ac.connected())
    m_ac.stop();

  return m_ac.connect( host, 443, true);
}
  
void PushBullet::_onDisconnect(AsyncClient* client)
{
  (void)client;
}

void PushBullet::_onTimeout(AsyncClient* client, uint32_t time)
{
  (void)client;
}

void PushBullet::_onError(AsyncClient* client, int8_t error)
{
  (void)client;

  Serial.print("PB error ");
  Serial.println(error);
}

void PushBullet::_onConnect(AsyncClient* client)
{
  (void)client;

  String data = "{\"type\": \"note\", \"title\": \"";
  data += m_szTitle;
  data += "\", \"body\": \"";
  data += m_szBody;
  data += "\"}";

  String s = String("POST ");
  s += url;
  s += " HTTP/1.1\r\n";
  s += "Host: ";
  s += host;
  s += "\r\n"
       "Content-Type: application/json\r\n"
       "Access-Token: ";
  s += m_szToken;
  s += "\r\n"
       "User-Agent: Arduino\r\n"
       "Content-Length: ";
  s += data.length();
  s += "\r\n"
       "Connection: close\r\n\r\n";
  s += data;
  s += "\r\n\r\n";

  m_ac.add(s.c_str(), s.length());
}

void PushBullet::_onData(AsyncClient* client, char* data, size_t len)
{
  (void)client;

// Lots of data gets sent back
//  Serial.println("PB onData");
//  Serial.print(data);
}
