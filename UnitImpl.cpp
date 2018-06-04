#include <Arduino.h>
#include "Unit.h"
#include "Module.h"
#include "StringBuffer.cpp"
#include "Utils.cpp"
#include "UJsonListener.cpp"
#include "JsonStreamingParser.h"
#include "List.cpp"
#include <SPI.h>
#include <Ethernet.h>
#include <WebSocketClient.h>


class UnitImpl : public Unit {

  public :

    List<Module> *modules;
    WebSocketClient webSocketClient;
    EthernetClient ethernetClient;
    char *unitUuid;
    char *connectionPassword;
    char *encryptionPassword;
    bool encryption;
    const byte  mac[6] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
    bool isInit = false;

    void init() {
      if (isInit) 
        return;

      this->webSocketClient.path = concat("/unit/", unitUuid);
      this->webSocketClient.host = "192.168.0.220:8020";
      this->webSocketClient.connectionPassword = connectionPassword;
      isInit = true;
    }

    void updateValuesInModules() {
      for (int i = 0; i < modules->size(); i++) {
        if (modules->get(i)->needToApplied()) {
          modules->get(i)->updateValue();
          modules->get(i)->applied();
          modules->get(i)->inform();
        }
      }
    }

    StringBuffer* checkData() {
      String data;

      this->webSocketClient.getData(data);
      if (data.length() == 0) {
        return NULL;
      }

      StringBuffer *sb = new StringBuffer();
      sb->appendString(data.c_str());
      sb->trim();

      return sb;
    }

    bool connection() {

      Ethernet.begin(this->mac);

      init();

      if (ethernetClient.connect("192.168.0.220", 8020)) {
        Serial.println(F("conected"));
      } else {
        Serial.println(F("fail"));
        return false;
      }

      if (webSocketClient.handshake(ethernetClient)) {
        Serial.println(F("Handshake successful"));
      } else {
        Serial.println(F("Handshake failed."));
        return false;
      }

      return true;
    };

    void connectionSurvival() {
      if (ethernetClient.connected()) {
        return;
      }

      ethernetClient.stop();
      delay(1000);

      Serial.println(F("try reconnect ..."));

      if (connection()) {
        return;
      }
    };

    void executor(char *json) {
      UJsonListener listener;
      listener.setModules(modules);

      JsonStreamingParser parser;
      parser.setListener(&listener);

      for (int i = 0; i < strlen(json); i++) {
        parser.parse(json[i]);
      }
      parser.reset();
    }

    void responseHandler(StringBuffer *response) {

      char *json = NULL;
      if (encryption) {
        json = uDecryptBody(response->toString(), encryptionPassword);
      } else {
        json = response->toString();
      }

      executor(json);

      if (encryption) {
        delete [] json;
      }

      delete response;
    }

    void getRequest() {
      StringBuffer *response = checkData();
      if (!response)
        return;

      responseHandler(response);
    }

    void prepareOutgoingData() {
      if (!needToSend())
        return;

      bool first = true;
      StringBuffer *sb = new StringBuffer();
      sb->append('{');

      for (int i = 0; i < modules->size(); i++) {
        if (modules->get(i)->needToInform()) {

          if (!first) {
            sb->append(',');
          } else {
            first = false;
          }

          sb->append('\"');
          sb->appendString(modules->get(i)->getKey());
          sb->appendString("\":\"");
          sb->appendString(modules->get(i)->getValue());
          sb->append('\"');

          modules->get(i)->informed();
        }
      }

      sb->append('}');
      sb->trim();

      char *json = NULL;

      if (encryption) {
        json = uEncryptBody(sb->toString(), encryptionPassword);
      } else {
        json = sb->toString();
      }

      this->webSocketClient.sendData(json);

      if (encryption) {
        delete [] json;
      }

      delete sb;
    }

    bool needToSend() {
      bool result = false;

      for (int i = 0; i < modules->size(); i++) {
        if (modules->get(i)->needToInform()) {
          result = true;
          break;
        }
      }
      return result;
    }

  public :

    UnitImpl() {
      this->modules = new List<Module>();
    }

    ~UnitImpl() {}

    void putModule(Module *module) {
      this->modules->add(module);
    }

    void setUuid(char *unitUuid) {
      this->unitUuid = unitUuid;
    }

    void setConnectionPassword(char *connectionPassword) {
      this->connectionPassword = connectionPassword;
    }

    void setEncryption(bool encryption) {
      this->encryption = encryption;
    }

    void setEncryptionPassword(char *encryptionPassword) {
      this->encryptionPassword = encryptionPassword;
    }

    void update() {
      connectionSurvival();

      getRequest();

      updateValuesInModules();

      prepareOutgoingData();
    }

};
