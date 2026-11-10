/*
 * url_utils.cpp — the address-bar rule.
 */
#include "url_utils.h"

#include <QUrlQuery>

namespace {

/* The search engine and the home page, named once. */
const char *kSearchBase = "https://duckduckgo.com/?q=";
const char *kHome = "https://duckduckgo.com/";

/* Whether the part before the first colon is a scheme rather than a port or a
   word. A scheme is a run of letters, digits, '+', '-' and '.' that starts
   with a letter; "http", "about", "file", "data" all pass, while "192" and
   "localhost" (a bare word before the colon) do not. */
bool has_scheme(const QString &text)
{
    const int colon = text.indexOf(QLatin1Char(':'));
    if (colon <= 0) {
        return false;
    }
    const QString head = text.left(colon);
    const QString suffix = text.mid(colon + 1);
    int port_end = 0;
    while (port_end < suffix.size() &&
           suffix.at(port_end).isDigit() &&
           suffix.at(port_end).unicode() <= QLatin1Char('9').unicode()) {
        port_end++;
    }
    const bool numeric_port =
        port_end > 0 &&
        (port_end == suffix.size() ||
         suffix.at(port_end) == QLatin1Char('/') ||
         suffix.at(port_end) == QLatin1Char('?') ||
         suffix.at(port_end) == QLatin1Char('#'));
    if (numeric_port &&
        (head.compare(QStringLiteral("localhost"),
                      Qt::CaseInsensitive) == 0 ||
         head.contains(QLatin1Char('.')))) {
        return false;
    }
    if (head.isEmpty() || !head.at(0).isLetter()) {
        return false;
    }
    for (const QChar character : head) {
        if (!(character.isLetterOrNumber() || character == QLatin1Char('+') ||
              character == QLatin1Char('-') || character == QLatin1Char('.'))) {
            return false;
        }
    }
    return true;
}

/* A line that names a host and no search: a single token with a dot, or a
   host with a port. A space anywhere means it is words, not a host. */
bool looks_like_host(const QString &text)
{
    if (text.contains(QLatin1Char(' '))) {
        return false;
    }
    if (text.contains(QLatin1Char('.'))) {
        return true;
    }
    /* "localhost:8080" — no dot, but a colon-port on the end. */
    const int colon = text.indexOf(QLatin1Char(':'));
    if (colon > 0) {
        const QString tail = text.mid(colon + 1);
        int port_end = 0;
        while (port_end < tail.size() &&
               tail.at(port_end).isDigit() &&
               tail.at(port_end).unicode() <= QLatin1Char('9').unicode()) {
            port_end++;
        }
        bool ok = false;
        const int port = tail.left(port_end).toInt(&ok);
        const bool has_path =
            port_end == tail.size() ||
            tail.at(port_end) == QLatin1Char('/') ||
            tail.at(port_end) == QLatin1Char('?') ||
            tail.at(port_end) == QLatin1Char('#');
        return ok && port > 0 && has_path;
    }
    return false;
}

}  // namespace

QUrl home_url()
{
    return QUrl(QString::fromUtf8(kHome));
}

QUrl url_from_input(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return QUrl();
    }
    if (has_scheme(trimmed)) {
        return QUrl(trimmed);
    }
    if (looks_like_host(trimmed)) {
        return QUrl(QStringLiteral("http://") + trimmed);
    }
    const QString encoded =
        QString::fromUtf8(QUrl::toPercentEncoding(trimmed));
    return QUrl(QString::fromUtf8(kSearchBase) + encoded);
}
