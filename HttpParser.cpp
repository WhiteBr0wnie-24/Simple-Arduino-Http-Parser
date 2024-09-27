#include "HttpParser.h"

vector<String> clientDataLines;
vector<KeyValueEntry> requestHeaders;
vector<KeyValueEntry> requestParams;
String method = "";
String path = "";
String version = "";
ErrorCode errorCode;
size_t buffLength;
unsigned int readBytes = 0;
unsigned int dataOffsetInBytes = 0;
char *buff = nullptr;
char *data = nullptr;
int parsedLines = 0;

HttpParser::HttpParser(Stream &c, size_t bufferSize) : clientStream(c) {
  readBytes = 0;
  parsedLines = 0;
  dataOffsetInBytes = 0;
  buffLength = bufferSize;
  buff = new char[bufferSize];
  data = nullptr;
  errorCode = ErrorCode::NONE;
  method = "";
  path = "";
  version = "";
}

void splitString(char* buff, unsigned int len, char delimiter, vector<String> *target) {
  String currentPiece = "";
  for (int i = 0; i < len; i++) {
    if (buff[i] != delimiter && buff[i] != '\0') {
      currentPiece += buff[i];
    } else {
      target->push_back(currentPiece);
      currentPiece = "";
    }
  }
  if (currentPiece.length() > 0) {
    target->push_back(currentPiece);
  }
}

void printDebug(String message) {
  if (DEBUG_OUTPUT) {
    Serial.println(message);
  }
}

void splitRequestIntoLines() {
  splitString(buff, readBytes, '\n', &clientDataLines);
  printDebug("Done parsing lines!");
}

bool HttpParser::parseBasicInfo(void) {
  String firstLine = clientDataLines.at(0);
  int firstWhiteSpaceIndex = firstLine.indexOf(' ');
  int secondWhiteSpaceIndex = firstLine.indexOf(' ', firstWhiteSpaceIndex + 1);
  int otherWhiteSpaceIndex = firstLine.indexOf(' ', secondWhiteSpaceIndex + 1);
  if (firstWhiteSpaceIndex == -1 || secondWhiteSpaceIndex == -1 || otherWhiteSpaceIndex != -1) {
    errorCode = ErrorCode::INVALID_PATH;
    return false;
  } else {
    method = firstLine.substring(0, firstWhiteSpaceIndex);
    path =  firstLine.substring(firstWhiteSpaceIndex, secondWhiteSpaceIndex);
    version = firstLine.substring(secondWhiteSpaceIndex);
    int paramStartIndex = path.indexOf('?');
    if (paramStartIndex > 0) {
      path = path.substring(0, paramStartIndex - 1);
    }
    method.trim();
    path.trim();
    version.trim();
  }
  parsedLines = 1;
  dataOffsetInBytes += firstLine.length() + 1;
  printDebug("Path OK");
  return true;
}

bool HttpParser::parsePathParameters(void) {
  String firstLine = clientDataLines.at(0);
  int paramStartIndex = firstLine.indexOf('?');
  if (paramStartIndex > 0) {
    vector<String> params;
    int secondWhiteSpaceIndex = firstLine.indexOf(' ', paramStartIndex);
    String paramPart = firstLine.substring(paramStartIndex + 1, secondWhiteSpaceIndex);
    size_t len = paramPart.length(); // length includes null-terminator
    char paramPartBuff[len];
    paramPart.toCharArray(paramPartBuff, len);
    splitString(paramPartBuff, len - 1, '&', &params);
    printDebug(String("Extracted ") + String(params.size()));
    printDebug(" path parameters");
    for (int i = 0; i < params.size(); i++) {
      int keyValueSeparatorIndex = params.at(i).indexOf('=');
      if (keyValueSeparatorIndex <= 0) {
        printDebug("Malformed URL parameter!");
        return false;
      } else {
        String key = params.at(i).substring(0, keyValueSeparatorIndex);
        String val = params.at(i).substring(keyValueSeparatorIndex + 1);
        requestParams.push_back(KeyValueEntry(key, val));
      }
    }
  }
  return true;
}

bool HttpParser::parseHeaders(void) {
  if (clientDataLines.size() > 1) {
    String currentLine = clientDataLines.at(parsedLines);
    while (currentLine != "\r" && parsedLines < clientDataLines.size()) {
      int separatorIndex = currentLine.indexOf(':');
      if (separatorIndex <= 0) {
        printDebug("Request headers malformed!");
        return false;
      } else {
        String key = currentLine.substring(0, separatorIndex);
        String val = currentLine.substring(separatorIndex + 1);
        key.trim();
        val.trim();
        requestHeaders.push_back(KeyValueEntry(key, val));
        currentLine = clientDataLines.at(parsedLines);
        parsedLines += 1;
        dataOffsetInBytes += currentLine.length() + 1;
      }
    }
    printDebug("Headers OK");
  }
  return true;
}

bool HttpParser::parseRequestData(void) {
  int numDataBytes = getDataLength();
  data = new char[numDataBytes];
  memcpy(data, buff + dataOffsetInBytes, numDataBytes);
  printDebug(String("Copied ") + String(numDataBytes) + String(" data bytes!"));
  return true;
}

bool HttpParser::parseRequest(void) {
  if (clientDataLines.empty()) {
    printDebug("Request empty!");
    errorCode = ErrorCode::REQUEST_MALFORMED;
    return false;
  } else {
    parseBasicInfo();
    parsePathParameters();
    parseHeaders();
    // The subsequent line must be empty
    if (!clientDataLines.at((--parsedLines)++).startsWith("\r")) {
      return false;
    }
    parseRequestData();
    return true;
  }
}

bool HttpParser::receive(void) {
  while (clientStream.available()) {
    buff[readBytes++] = (char) clientStream.read();
    if (readBytes >= buffLength) {
      printDebug("Buffer overflow while reading from client!");
      errorCode = ErrorCode::BUFFER_OVERFLOW;
      return false;
    }
  }
  if (readBytes <= 0) {
    printDebug("No data read from client!");
    return false;
  } else {
    printDebug("Done reading request!");
    splitRequestIntoLines();
    if (!parseRequest()) {
      printDebug("Request malformed: ");
      return false;
    } else {
      printDebug("Receive success!");
    }
  }
  return true;
}

bool HttpParser::transmit(int code, String message) {
  clientStream.println("HTTP/1.1 " + String(code) + " " + message);
  clientStream.println("Connection: close");
  clientStream.println();
  clientStream.flush();
  delay(25);
  return true;
}

bool HttpParser::transmit(String body) {
  size_t len = body.length();
  char bodyBuff[len];
  body.toCharArray(bodyBuff, len);
  return transmit("text/plain", (byte*) bodyBuff, len - 1);
}

bool HttpParser::transmit(String contentType, String body) {
  size_t len = body.length();
  char bodyBuff[len];
  body.toCharArray(bodyBuff, len);
  return transmit(contentType, (byte*) bodyBuff, len - 1);
}

bool HttpParser::transmit(String contentType, byte *data, size_t length) {
  clientStream.println("HTTP/1.1 200 OK");
  clientStream.println("Connection: close");
  clientStream.println("Content-Type: " + contentType);
  clientStream.println("Content-Length: " + String(length));
  clientStream.println();
  clientStream.write(data, length);
  clientStream.println();
  clientStream.flush();
  delay(25);
  return true;
}

String HttpParser::getVersion(void) {
  return version;
}

String HttpParser::getMethod(void) {
  return method;
}

String HttpParser::getPath(void) {
  return path;
}

String HttpParser::getHeader(String key) {
  for (int i = 0; i < requestHeaders.size(); i++) {
    KeyValueEntry current = requestHeaders.at(i);
    if (current.key == key) {
      return current.value;
    }
  }
  return "";
}

String HttpParser::getPathParameter(String key) {
  for (int i = 0; i < requestParams.size(); i++) {
    KeyValueEntry current = requestParams.at(i);
    if (current.key == key) {
      return current.value;
    }
  }
  return "";
}

vector<KeyValueEntry> HttpParser::getHeaders(void) {
  return requestHeaders;
}

vector<KeyValueEntry> HttpParser::getPathParameters(void) {
  return requestParams;
}

int HttpParser::getDataOffsetInBytes(void) {
  return dataOffsetInBytes;
}

int HttpParser::getDataLength(void) {
  unsigned int dataBytes = readBytes - dataOffsetInBytes;
  if (dataBytes >= 0) {
    return dataBytes;
  } else {
    return 0;
  }
}

String HttpParser::getDataAsString(void) {
  size_t numDataBytes = getDataLength();
  char tmp[numDataBytes + 1];
  memcpy(tmp, data, numDataBytes);
  tmp[numDataBytes] = '\0';
  return String(tmp);
}

char* HttpParser::getData(void) {
  return data;
}

ErrorCode HttpParser::getError(void) {
  return errorCode;
}

void HttpParser::end(void) {
  clientDataLines.clear();
  requestHeaders.clear();
  requestParams.clear();
  if (buff != nullptr) {
    delete[] buff;
  }
  if (data != nullptr) {
    delete[] data;
  }
  clientStream.flush();
}
