#ifndef CANARYCHECKER_H
#define CANARYCHECKER_H

#include <QString>

/// Makes HTTP GET to productionUrl/canary, returns the "canary" flag.
/// Returns false on error/timeout (safe fallback).
bool checkCanary(const QString &productionUrl, int timeoutMs = 3000);

/// Makes HTTP GET to productionUrl/canary/status, returns the "active" flag.
/// Returns true on error/timeout (safe fallback — assume canary is still active).
bool checkCanaryActive(const QString &productionUrl, int timeoutMs = 3000);

#endif // CANARYCHECKER_H
