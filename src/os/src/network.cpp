#include "os/network.h"
#include "debug/log.h"
#include "core/utility.h"
#include "core/vector.h"

#include <emscripten/fetch.h>
#include <emscripten/websocket.h>
#include <string.h>

struct ActiveFetch {
  bool active = false;
  emscripten_fetch_t* handle = nullptr;
  FetchCallback callback = nullptr;
  void* userData = nullptr;
};

struct ActiveSocket {
  bool active = false;
  EMSCRIPTEN_WEBSOCKET_T handle = 0;
  
  Socket::OnMessage onMessage = nullptr;
  Socket::OnState   onState   = nullptr;
  void* userData = nullptr;
  
  SocketState currentState = SocketState::Disconnected;
};

static Vec<ActiveFetch>  s_Fetches;
static Vec<ActiveSocket> s_Sockets;

template<typename T>
static u32 AllocSlot(Vec<T>& pool) {
  if (pool.empty()) {
    pool.push({});
  }
  for (u32 i = 1; i < pool.size; ++i) {
    if (!pool[i].active) return i;
  }
  pool.push({});
  return (u32)(pool.size - 1);
}

static FetchError MapHttpError(u32 status) {
  if (status == 404) return FetchError::NotFound;
  if (status == 403) return FetchError::AccessDenied;
  if (status >= 500) return FetchError::ServerFailure;
  return FetchError::NetworkError;
}

static void OnFetchSuccess(emscripten_fetch_t *fetch) {
  u32 id = (u32)(uptr)fetch->userData;
  
  if (id < s_Fetches.size && s_Fetches[id].active) {
    ActiveFetch& req = s_Fetches[id];
    
    if (req.callback) {
      Buffer buf;
      buf.data = (void*)fetch->data;
      buf.size = fetch->numBytes;
      
      if (fetch->status >= 200 && fetch->status < 300) {
        req.callback(FetchResult::Ok(buf), req.userData);
      } else {
        req.callback(FetchResult::Err(MapHttpError(fetch->status)), req.userData);
      }
    }
    req.active = false;
  }
  emscripten_fetch_close(fetch);
}

static void OnFetchError(emscripten_fetch_t *fetch) {
  u32 id = (u32)(uptr)fetch->userData;

  if (id < s_Fetches.size && s_Fetches[id].active) {
    ActiveFetch& req = s_Fetches[id];
    
    if (req.callback) {
      FetchError err = (fetch->status == 0) ? FetchError::NetworkError : MapHttpError(fetch->status);
      req.callback(FetchResult::Err(err), req.userData);
    }
    req.active = false;
  }
  emscripten_fetch_close(fetch);
}

static EM_BOOL OnSocketOpen(int, const EmscriptenWebSocketOpenEvent*, void *userData) {
  u32 id = (u32)(uptr)userData;
  if (id < s_Sockets.size && s_Sockets[id].active) {
    ActiveSocket& s = s_Sockets[id];
    s.currentState = SocketState::Connected;
    if (s.onState) s.onState({id}, SocketState::Connected, s.userData);
  }
  return EM_TRUE;
}

static EM_BOOL OnSocketError(int, const EmscriptenWebSocketErrorEvent*, void *userData) {
  u32 id = (u32)(uptr)userData;
  if (id < s_Sockets.size && s_Sockets[id].active) {
    ActiveSocket& s = s_Sockets[id];
    s.currentState = SocketState::Error;
    if (s.onState) s.onState({id}, SocketState::Error, s.userData);
  }
  return EM_TRUE;
}

static EM_BOOL OnSocketClose(int, const EmscriptenWebSocketCloseEvent*, void *userData) {
  u32 id = (u32)(uptr)userData;
  if (id < s_Sockets.size && s_Sockets[id].active) {
    ActiveSocket& s = s_Sockets[id];
    s.currentState = SocketState::Disconnected;
    if (s.onState) s.onState({id}, SocketState::Disconnected, s.userData);
  }
  return EM_TRUE;
}

static EM_BOOL OnSocketMessage(int, const EmscriptenWebSocketMessageEvent *e, void *userData) {
  u32 id = (u32)(uptr)userData;
  if (id < s_Sockets.size && s_Sockets[id].active) {
    ActiveSocket& s = s_Sockets[id];
    if (s.onMessage) {
      s.onMessage({id}, e->data, e->numBytes, s.userData);
    }
  }
  return EM_TRUE;
}

Socket Socket::Connect(const char* url, OnMessage onMsg, OnState onState, void* userData) {
  if (!emscripten_websocket_is_supported()) {
    logError("WebSockets not supported in this build/browser.");
    return {0};
  }

  u32 id = AllocSlot(s_Sockets);
  ActiveSocket& sock = s_Sockets[id];

  EmscriptenWebSocketCreateAttributes attr = { url, NULL, EM_TRUE };
  sock.handle = emscripten_websocket_new(&attr);
  
  if (sock.handle <= 0) {
    logError("Failed to create WebSocket handle for %s", url);
    sock.active = false;
    return {0};
  }

  sock.active = true;
  sock.onMessage = onMsg;
  sock.onState = onState;
  sock.userData = userData;
  sock.currentState = SocketState::Connecting;

  // emscripten_websocket_set_binary_type(sock.handle, 1);

  emscripten_websocket_set_onopen_callback(sock.handle, (void*)(uptr)id, OnSocketOpen);
  emscripten_websocket_set_onerror_callback(sock.handle, (void*)(uptr)id, OnSocketError);
  emscripten_websocket_set_onclose_callback(sock.handle, (void*)(uptr)id, OnSocketClose);
  emscripten_websocket_set_onmessage_callback(sock.handle, (void*)(uptr)id, OnSocketMessage);

  return {id};
}

void Socket::send(const void* data, u64 size) {
  if (id < s_Sockets.size) {
    ActiveSocket& s = s_Sockets[id];
    if (s.active && s.currentState == SocketState::Connected) {
      emscripten_websocket_send_binary(s.handle, (void*)data, (u32)size);
    } else {
      logError("Socket::send ignored: Socket %u is not connected.", id);
    }
  }
}

void Socket::disconnect() {
  if (id < s_Sockets.size) {
    ActiveSocket& s = s_Sockets[id];
    if (s.active) {
      emscripten_websocket_close(s.handle, 1000, "Client Disconnect");
      s.currentState = SocketState::Disconnected;
    }
  }
}

SocketState Socket::getState() const {
  if (id < s_Sockets.size && s_Sockets[id].active) {
    return s_Sockets[id].currentState;
  }
  return SocketState::Disconnected;
}

void fetchFile(const char* url, FetchCallback onComplete, void* userData) {
  u32 id = AllocSlot(s_Fetches);
  ActiveFetch& req = s_Fetches[id];
  
  req.active = true;
  req.callback = onComplete;
  req.userData = userData;

  emscripten_fetch_attr_t attr;
  emscripten_fetch_attr_init(&attr);
  
  strcpy(attr.requestMethod, "GET");
  attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
  
  const char* headers[] = {
    "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0",
    "Pragma", "no-cache",
    nullptr
  };
  attr.requestHeaders = headers;

  attr.onsuccess = OnFetchSuccess;
  attr.onerror   = OnFetchError;
  attr.userData  = (void*)(uptr)id;

  req.handle = emscripten_fetch(&attr, url);
}
