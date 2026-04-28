#include "CanaryChecker.h"

#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QUrl>
#include <QDebug>

bool checkCanary(const QString &productionUrl, int timeoutMs)
{
    QUrl url(productionUrl.trimmed());
    if (url.scheme().compare("wss", Qt::CaseInsensitive) == 0)
        url.setScheme("https");
    else if (url.scheme().compare("ws", Qt::CaseInsensitive) == 0)
        url.setScheme("http");

    url.setPath("/canary");
    // Railway uses port 443 for TLS; drop explicit port only for https
    if (url.scheme() == "https" && (url.port() == 9001 || url.port() == -1))
        url.setPort(-1);

    qInfo().noquote() << "[canary] checking" << url.toString();

    QNetworkAccessManager nam;
    QNetworkRequest request(url);
    QNetworkReply *reply = nam.get(request);

    // Synchronous wait with timeout
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();

    if (!reply->isFinished()) {
        qWarning().noquote() << "[canary] timeout after" << timeoutMs << "ms";
        reply->abort();
        reply->deleteLater();
        return false;
    }

    if (reply->error() != QNetworkReply::NoError) {
        qWarning().noquote() << "[canary] HTTP error:" << reply->errorString();
        reply->deleteLater();
        return false;
    }

    const QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning().noquote() << "[canary] invalid JSON:" << data;
        return false;
    }

    bool isCanary = doc.object().value("canary").toBool(false);
    qInfo().noquote() << "[canary] result:" << (isCanary ? "CANARY" : "production");
    return isCanary;
}

bool checkCanaryActive(const QString &productionUrl, int timeoutMs)
{
    QUrl url(productionUrl.trimmed());
    if (url.scheme().compare("wss", Qt::CaseInsensitive) == 0)
        url.setScheme("https");
    else if (url.scheme().compare("ws", Qt::CaseInsensitive) == 0)
        url.setScheme("http");

    url.setPath("/canary/status");
    if (url.scheme() == "https" && (url.port() == 9001 || url.port() == -1))
        url.setPort(-1);

    qInfo().noquote() << "[canary] checking kill switch" << url.toString();

    QNetworkAccessManager nam;
    QNetworkReply *reply = nam.get(QNetworkRequest(url));

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();

    if (!reply->isFinished()) {
        reply->abort();
        reply->deleteLater();
        return true; // safe fallback: assume canary still active
    }
    if (reply->error() != QNetworkReply::NoError) {
        reply->deleteLater();
        return true;
    }

    const QByteArray data = reply->readAll();
    reply->deleteLater();

    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject())
        return true;

    bool active = doc.object().value("active").toBool(true);
    qInfo().noquote() << "[canary] kill switch:" << (active ? "ACTIVE" : "KILLED");
    return active;
}
