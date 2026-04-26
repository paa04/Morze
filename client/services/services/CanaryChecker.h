#ifndef CANARYCHECKER_H
#define CANARYCHECKER_H

#include <QString>

/// Makes an HTTP GET to productionUrl/canary and returns the "canary" flag.
/// Converts wss:// to https:// automatically.
/// Returns false on error or timeout (safe fallback to production).
bool checkCanary(const QString &productionUrl, int timeoutMs = 3000);

#endif // CANARYCHECKER_H
