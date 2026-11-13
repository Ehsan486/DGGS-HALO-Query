#pragma once

#if defined(__has_include)
  #if __has_include(<h3api.h>)
    #include <h3api.h>
  #elif __has_include(<h3/h3api.h>)
    #include <h3/h3api.h>
  #else
    #error "H3 header not found. Expected h3api.h or h3/h3api.h."
  #endif
#else
  #include <h3api.h>
#endif
