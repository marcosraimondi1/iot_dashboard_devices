#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>

#define MAX_VARIABLES 5
#define MAX_STRING_LEN 100 // Maximum length for string fields

#define RC_STR(rc) ((rc) == 0 ? "OK" : "ERROR")
#define PRINT_RESULT(func, rc) printf("%s: %d <%s>\n", (func), rc, RC_STR(rc))
#define SUCCESS_OR_EXIT(rc)                                                    \
  {                                                                            \
    if (rc != 0) {                                                             \
      return 1;                                                                \
    }                                                                          \
  }
#define SUCCESS_OR_BREAK(rc)                                                   \
  {                                                                            \
    if (rc != 0) {                                                             \
      break;                                                                   \
    }                                                                          \
  }

enum VarType { SENSOR, ACTUATOR };

typedef struct {
  char variable[MAX_STRING_LEN];
  char variableFullName[MAX_STRING_LEN];
  enum VarType variableType;
  int variableSendFreq;
  char lastValue[MAX_STRING_LEN];
  int save;
} Variable;

typedef struct {
  char username[MAX_STRING_LEN];
  char password[MAX_STRING_LEN];
  char topic[MAX_STRING_LEN];
  Variable variables[MAX_VARIABLES];
  int variableCount;
  bool isValid;
} DeviceData;

/*
 * @brief Fuction to parse JSON response from server into a DeviceData struct.
 * @param json body of the received response
 * @param data struct for the parsed data
 * */
void parseJSON(const char *json, DeviceData *data);

/*
 * @brief Print DeviceData struct
 * @param data DeviceData struct to print
 * */
void printDeviceData(const DeviceData *data);

#endif // !UTILS_H
