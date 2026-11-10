#define main software_center_application_main
#include "../src/GnuChanSoftwareCenter.c"
#undef main

#include <assert.h>

static void test_catalog_metadata(void) {
    static const char catalog[] =
        "Package: sample-browser\n"
        "Version: 1.0\n"
        "Section: web\n"
        "Description-en: A web browser\n"
        " that follows Debian package metadata.\n"
        "Description-md5: 1234567890abcdef\n"
        "Maintainer: Example Maintainer <maintainer@example.invalid>\n"
        "Homepage: https://example.invalid/browser\n"
        "Installed-Size: 2048\n"
        "Size: 409600\n"
        "Tag: role::program, interface::graphical,\n"
        " web::browser, use::browsing\n"
        "\n"
        "Package: web-library\n"
        "Version: 1.0\n"
        "Section: web\n"
        "Description-en: Shared support library for web applications.\n"
        "Tag: role::shared-lib\n"
        "\n"
        "Package: learning-tool\n"
        "Version: 1.0\n"
        "Section: education\n"
        "Description: Learning tools\n"
        "\n"
        "Package: science-tool\n"
        "Version: 1.0\n"
        "Section: science\n"
        "Description: Scientific tools\n"
        "\n"
        "Package: sample-fonts\n"
        "Version: 2.0\n"
        "Section: fonts\n"
        "Description: A sample font family\n"
        "\n"
        "Package: font-tool\n"
        "Version: 1.0\n"
        "Section: misc\n"
        "Description: Font support tool\n"
        "Tag: role::font\n"
        "\n";
    FILE *fp = fmemopen((void *)catalog, sizeof(catalog) - 1, "r");
    assert(fp != NULL);
    parse_available_catalog(fp);
    fclose(fp);

    Package *browser = pkg_find("sample-browser");
    assert(browser != NULL);
    assert(browser->category == CATEGORY_BROWSER);
    assert(strstr(browser->desc, "Debian package metadata") != NULL);
    assert(view_matches(VIEW_BROWSER, browser));
    assert(strcmp(browser->maintainer,
                   "Example Maintainer <maintainer@example.invalid>") == 0);
    assert(strcmp(browser->homepage,
                   "https://example.invalid/browser") == 0);
    assert(browser->installed_size_kb == 2048);
    assert(browser->download_size_bytes == 409600);
    assert(pkg_matches_search(browser, "Example Maintainer"));
    assert(pkg_matches_search(browser, "1.0"));

    Package *library = pkg_find("web-library");
    assert(library != NULL);
    assert(library->category == CATEGORY_INTERNET);
    assert(!view_matches(VIEW_BROWSER, library));

    Package *education = pkg_find("learning-tool");
    Package *science = pkg_find("science-tool");
    assert(education && education->category == CATEGORY_EDUCATION);
    assert(science && science->category == CATEGORY_SCIENCE);

    Package *fonts = pkg_find("sample-fonts");
    Package *font_tool = pkg_find("font-tool");
    assert(fonts && fonts->category == CATEGORY_FONTS);
    assert(view_matches(VIEW_FONTS, fonts));
    assert(pkg_matches_search(fonts, "sample font"));
    assert(font_tool && font_tool->category == CATEGORY_FONTS);
    assert(view_matches(VIEW_FONTS, font_tool));
    assert(valid_package_name("libc6:amd64"));
    assert(!valid_package_name("libc6:amd64:invalid"));
    assert(!valid_package_name("libc6:"));
}

static void test_apt_updates(void) {
    pkg_add("libc6", "1.0", "", PKG_STATE_INSTALLED, CATEGORY_SYSTEM,
            0, 0, 0);
    pkg_add("sample-browser", "1.0", "", PKG_STATE_INSTALLED,
            CATEGORY_BROWSER, 0, 0, 0);

    static const char simulation[] =
        "Inst libc6:amd64 [1.0] (2.0 Debian:stable)\n"
        "Inst sample-browser [1.0] (2.0 Debian:stable)\n"
        "Inst libc6:amd64 [1.0] (2.0 Debian:stable)\n";
    FILE *fp = fmemopen((void *)simulation, sizeof(simulation) - 1, "r");
    assert(fp != NULL);
    mark_apt_updates(fp);
    fclose(fp);

    assert(update_available == 2);
    assert(pkg_find("libc6")->has_update);
    assert(pkg_find("sample-browser")->has_update);
}

static void test_sidebar_and_repo_configs(void) {
    assert(run_queue_button_x(0) == LIST_LEFT);
    assert(run_queue_button_x(1) == LIST_LEFT + 108);
    assert(bulk_update_button_x(0) == LIST_LEFT + 108);
    assert(bulk_update_button_x(1) == LIST_LEFT + 216);
    assert(gnuchan_update_button_x(0) == LIST_LEFT + 256);
    assert(gnuchan_update_button_x(1) == LIST_LEFT + 364);

    int browser_visible = 0;
    int education_visible = 0;
    int science_visible = 0;
    int fonts_visible = 0;
    for (int i = 0; i < sidebar_count; i++) {
        assert(strcmp(sidebar_items[i].label_en, "Other") != 0);
        browser_visible |= sidebar_items[i].view == VIEW_BROWSER;
        education_visible |= sidebar_items[i].view == VIEW_EDUCATION;
        science_visible |= sidebar_items[i].view == VIEW_SCIENCE;
        fonts_visible |= sidebar_items[i].view == VIEW_FONTS;
    }
    assert(browser_visible && education_visible && science_visible &&
           fonts_visible);
    assert(strcmp(GNUCHANSOFTWARECENTER_VERSION, "0.3.0") == 0);

    int wm_config_fixed = 0;
    int browser_config_fixed = 0;
    int runner_config_fixed = 0;
    for (size_t i = 0; i < GNUCHAN_REPO_COUNT; i++) {
        if (strcmp(gnuchan_repos[i].program, "GnuChanWM") == 0) {
            wm_config_fixed =
                strcmp(gnuchan_repos[i].config_file, "GnuChanWM.conf") == 0;
        } else if (strcmp(gnuchan_repos[i].program, "GnuChanBrowser") == 0) {
            browser_config_fixed =
                strcmp(gnuchan_repos[i].config_file,
                       "GnuChanBrowser.conf") == 0;
        } else if (strcmp(gnuchan_repos[i].program, "GnuChanRunner") == 0) {
            runner_config_fixed =
                strcmp(gnuchan_repos[i].config_file, "config.conf") == 0;
        }
    }
    assert(wm_config_fixed && browser_config_fixed && runner_config_fixed);
}

static void test_system_catalog(void) {
    parse_available_catalog(stdin);
    assert(pkg_count > 1000);

    int browser_count = 0;
    int font_count = 0;
    for (Package *p = pkg_head; p; p = p->next) {
        browser_count += p->category == CATEGORY_BROWSER;
        font_count += p->category == CATEGORY_FONTS;
    }
    assert(browser_count > 0 && font_count > 0);
    printf("APT catalog parsed: %d packages, %d browser(s), %d font package(s)\n",
           pkg_count, browser_count, font_count);
}

int main(int argc, char **argv) {
    assert(pthread_mutex_init(&state_mutex, NULL) == 0);
    if (argc == 2 && strcmp(argv[1], "--system-catalog") == 0) {
        test_system_catalog();
    } else {
        test_catalog_metadata();
        test_apt_updates();
        test_sidebar_and_repo_configs();
        puts("Software Center catalog tests passed");
    }
    pkg_free_all();
    pthread_mutex_destroy(&state_mutex);
    return 0;
}
