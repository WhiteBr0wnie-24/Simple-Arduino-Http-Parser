#ifndef DEBUG_OUTPUT
#define DEBUG_OUTPUT false
#endif

#ifndef HttpParser_h
#define HttpParser_h

#include <Arduino.h>
#include <vector>

using namespace std;

struct KeyValueEntry {
  String key;
  String value;
  KeyValueEntry(String k, String v) : key(k), value(v) {}
};

enum class ErrorCode {
    NONE,
    BUFFER_OVERFLOW,
    REQUEST_MALFORMED,
    INVALID_OR_UNSUPPORTED_METHOD,
    INVALID_REQUEST,
    INVALID_PATH,
    STREAM_UNAVAILABLE
};

class HttpParser {
  public:
    HttpParser(Stream &clientStream, size_t bufferSize);
    bool receive(void);
    bool transmit(int code, String message);
    bool transmit(String message);
    bool transmit(String contentType, byte *data, size_t length);
    String getVersion(void);
    String getMethod(void);
    String getPath(void);
    vector<KeyValueEntry> getHeaders(void);
    vector<KeyValueEntry> getPathParameters(void);
    int getDataOffsetInBytes(void);
    int getDataLength(void);
    char* getData(void);
    ErrorCode getError(void);
    void end(void);
  private:
    Stream &clientStream;
    bool parseRequest(void);
    bool parseRequestData(void);
    bool parseHeaders(void);
    bool parsePathParameters(void);
    bool parseBasicInfo(void);
};

#endif
