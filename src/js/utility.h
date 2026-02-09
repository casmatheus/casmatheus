#pragma once
#include <emscripten.h>

namespace JS {

inline void Close() {
  emscripten_run_script(
    "try { window.close(); } catch(e) { console.log('Close blocked, redirecting...'); }"
    "setTimeout(function() { window.location.href = '../../index.html'; }, 100);"
  );
}  

}  
