#pragma once
#include <string>

struct OriginSession
{
    bool isValid;
    std::string token;
    std::string decartKey;
    std::string userId; // ✅ ADD THIS
    std::string name;
    int seconds;
};

OriginSession AuthenticateOrigin(const std::string &email, const std::string &password);
bool UpdateUserSeconds(const std::string &userId, int seconds, const std::string &token);
void DeductTimeFromDB(OriginSession &session, int seconds);