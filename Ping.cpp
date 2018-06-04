#pragma once
#include <Arduino.h>
#include "Module.h"

class Ping : public Module {

  public :

    Ping(char *key) {
      this->key = key;
    }

    char* getValue() {
      return "true";
    }

    void  setValue(char *value) {}

    void updateValue() {}
    
};

