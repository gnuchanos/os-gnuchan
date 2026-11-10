#include "../src/url_utils.h"

#include <cassert>

int main()
{
    const QUrl localhost = url_from_input(QStringLiteral("localhost:8080"));
    assert(localhost.scheme() == QStringLiteral("http"));
    assert(localhost.host() == QStringLiteral("localhost"));
    assert(localhost.port() == 8080);

    const QUrl host = url_from_input(QStringLiteral("example.com:8443/path"));
    assert(host.scheme() == QStringLiteral("http"));
    assert(host.host() == QStringLiteral("example.com"));
    assert(host.port() == 8443);
    assert(host.path() == QStringLiteral("/path"));

    const QUrl explicit_scheme =
        url_from_input(QStringLiteral("https://example.com"));
    assert(explicit_scheme.scheme() == QStringLiteral("https"));
    assert(explicit_scheme.host() == QStringLiteral("example.com"));

    const QUrl search = url_from_input(QStringLiteral("some words"));
    assert(search.host() == QStringLiteral("duckduckgo.com"));
    assert(search.query().contains(QStringLiteral("some%20words")) ||
           search.query().contains(QStringLiteral("some words")));
}
