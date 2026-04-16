#pragma once

inline const char* TOPIC_VITALS = "patient/vitals";
inline const char* TOPIC_ALERTS = "patient/alerts"; 
inline const char* TOPIC_STATUS = "patient/battery/status"; // battery status

struct AlertTypes {
    const char* highTemperature = "HIGH_TEMP";
    const char* lowTemperature  = "LOW_TEMP";
    const char* highPressure    = "HYPERTENSION";
    const char* lowPressure     = "HYPOTENSION";
    const char* fallDetected    = "FALL";
    const char* emergencyButton = "SOS";
    const char* normal = "NORMAL";
};

inline AlertTypes alertType;
