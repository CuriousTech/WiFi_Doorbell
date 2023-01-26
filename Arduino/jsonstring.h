#include <ESPAsyncWebServer.h> // https://github.com/me-no-dev/ESPAsyncWebServer

class jsonString
{
public:
  jsonString(const char *pLabel = NULL)
  {
    m_cnt = 0;
    if(pLabel)
      s = pLabel, s += ";";
    s += "{";
  }
        
  String Close(void)
  {
    s += "}";
    return s;
  }

  void Var(const char *key, int iVal)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":";
    s += iVal;
    m_cnt++;
  }

  void Var(const char *key, uint32_t iVal)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":";
    s += iVal;
    m_cnt++;
  }

  void Var(const char *key, long int iVal)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":";
    s += iVal;
    m_cnt++;
  }

  void Var(const char *key, float fVal)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":";
    s += fVal;
    m_cnt++;
  }
  
  void Var(const char *key, bool bVal)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":";
    s += bVal ? 1:0;
    m_cnt++;
  }
  
  void Var(const char *key, const char *sVal)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":\"";
    s += sVal;
    s += "\"";
    m_cnt++;
  }
  
  void Var(const char *key, String sVal)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":\"";
    s += sVal;
    s += "\"";
    m_cnt++;
  }

  void Array(const char *key, uint32_t iVal[], int n)
  {
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":[";
    for(int i = 0; i < n; i++)
    {
      if(i) s += ",";
      s += iVal[i];
    }
    s += "]";
    m_cnt++;
  }

  void ArrayHdrs(const char *key, AsyncWebServerRequest *request)
  {
    int nHeaders = request->headers();
    if(nHeaders == 0) return;
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":[";

    for(int i = 0; i < nHeaders; i++){
      AsyncWebHeader* h = request->getHeader(i);
      if(i) s += ",";
      s +="\"";
      s += h->name().c_str();
      s += "=";
      s += h->value().c_str();
      s += "\"";
    }
    s += "]";
    m_cnt++;
  }

  void ArrayPrms(const char *key, AsyncWebServerRequest *request)
  {
    int nParams = request->params();
    if(!nParams) return;
    if(m_cnt) s += ",";
    s += "\"";
    s += key;
    s += "\":[";

    for(int i = 0; i < nParams; i++)
    {
      AsyncWebParameter* p = request->getParam(i);
      if(i) s += ",";
      s += "\"";
      if(p->isFile()){
        s += "FILE";
      } else if(p->isPost()){
        s += "POST";
      } else {
        s += "GET";
      }
      s += ",";
      s += p->name().c_str();
      s += "=";
      s += p->value().c_str();
      s += "\"";
    }
    s += "]";
  }

protected:
  String s;
  int m_cnt;
};
