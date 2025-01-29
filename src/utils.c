#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helper function to extract a string value
void extractString(const char *json, const char *key, char *outValue) {

  char *start = strstr(json, key);
  if (start) {
    start += strlen(key) + strlen(":\"");
    char *end = strchr(start, '\"');
    if (end) {
      strncpy(outValue, start, end - start);
      outValue[end - start] = '\0';
    }
  }
}

// Function to parse the JSON string
void parseJSON(const char *json, DeviceData *data) {
  extractString(json, "\"username\"", data->username);
  extractString(json, "\"password\"", data->password);
  extractString(json, "\"topic\"", data->topic);

  // Parse variables array
  char *variablesStart = strstr(json, "\"variables\"");
  if (variablesStart) {
    variablesStart =
        strchr(variablesStart, '[') + 1; // Move to the first array element
    char *variablesEnd = strchr(variablesStart, ']');
    char buffer[1024];
    int varIndex = 0;

    char type[100];
    while (variablesStart < variablesEnd && varIndex < MAX_VARIABLES) {
      char *variableEnd = strchr(variablesStart, '}') + 1;
      strncpy(buffer, variablesStart, variableEnd - variablesStart);
      buffer[variableEnd - variablesStart] = '\0';

      // Parse fields in the variable
      extractString(buffer, "\"variable\"", data->variables[varIndex].variable);
      extractString(buffer, "\"variableFullName\"",
                    data->variables[varIndex].variableFullName);
      extractString(buffer, "\"variableType\"", type);

      if (strcmp(type, "input") == 0) {
        data->variables[varIndex].variableType = SENSOR;
      } else {
        data->variables[varIndex].variableType = ACTUATOR;
      }

      extractString(buffer, "\"variableSendFreq\"", type);
      data->variables[varIndex].variableSendFreq = atoi(type);

      // Move to the next variable
      variablesStart = variableEnd;
      varIndex++;
    }
    data->variableCount = varIndex;
  }
}

// Function to display the parsed data
void printDeviceData(const DeviceData *data) {
  printf("Username: %s\n", data->username);
  printf("Password: %s\n", data->password);
  printf("Topic: %s\n", data->topic);

  for (int i = 0; i < data->variableCount; i++) {
    printf("  Variable %d:\n", i + 1);
    printf("    Variable: %s\n", data->variables[i].variable);
    printf("    Variable Full Name: %s\n", data->variables[i].variableFullName);

    if (data->variables[i].variableType == SENSOR)
      printf("    Variable Type: SENSOR\n");
    else
      printf("    Variable Type: ACTUATOR\n");

    printf("    Variable Send Freq: %d\n", data->variables[i].variableSendFreq);
  }
}

// ---------------------------------------------
