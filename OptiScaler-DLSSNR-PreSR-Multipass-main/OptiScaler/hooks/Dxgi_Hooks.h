#pragma once
#include "SysUtils.h"

class DxgiHooks
{
  private:
    static inline std::mutex hookMutex;

  public:
    static void Hook();
};
