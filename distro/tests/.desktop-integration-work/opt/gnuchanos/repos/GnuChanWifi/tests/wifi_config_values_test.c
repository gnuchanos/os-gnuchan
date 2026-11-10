#include "wifi_config.h"

#include <assert.h>

int main(void) {
    WifiConfig config;
    assert(wifi_config_load(&config, "tests/fixtures/wifi_numeric.conf") == 0);
    assert(config.font_size == 17);
    assert(config.width == 640);
    assert(config.rows == 12);
    return 0;
}
