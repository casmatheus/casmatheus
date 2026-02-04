#pragma once

#include "core/result.h"
#include "core/primitive.h"

enum class FetchError : u8 {
  NetworkError,
  NotFound,
  AccessDenied,
  ServerFailure
};

enum class SocketState : u8 {
  Disconnected = 0,
  Connecting,
  Connected,
  Error
};

struct Buffer {
  void* data;
  u64   size;       
};

using FetchResult = Result<Buffer, FetchError>;

struct Socket {
  u32 id;

	using OnMessage = void(*)(Socket socket, const void* data, u64 size, void* userData);
  using OnState   = void(*)(Socket socket, SocketState newState, void* userData);

  static Socket Connect(const char* url, 
                        OnMessage onMessage, 
                        OnState onStateChange = nullptr,
                        void* userData = nullptr);

  void send(const void* data, u64 size);
  void disconnect();
  SocketState getState() const;
  
  bool isValid() const { return id != 0; }
  
  bool operator==(const Socket& other) const { return id == other.id; }
  bool operator!=(const Socket& other) const { return id != other.id; }
};


using FetchCallback = void(*)(const FetchResult& fetch, void* userData);
void fetchFile(const char* url, FetchCallback onComplete, void* userData = nullptr);
