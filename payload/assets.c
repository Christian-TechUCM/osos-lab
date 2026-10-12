/* SPDX-License-Identifier: GPL-3.0-only */
#include "assets.h"
#include "dark_mode.h"
#include "osos.h"
#include "patch.h"
#include "themes.h"
#include "theme_pixels.h"

void cfw_bitmap_view_load_original(void *view);
/* Stock lookups bypass the hook for pixel matching and light-mode restoration. */
void *cfw_bitmap_resource_get_original(void *service, uint32_t type, uint32_t id);
void cfw_bitmap_cache_load_original(void *cache);

static int active(void) { return cfw_dark_mode_enabled() || cfw_theme_custom_light(); }
static int attached(const struct cfw_bitmap_asset *asset, const unsigned char *pixels)
{
    return pixels == asset->data + OSOS_BMAP_HEADER_SIZE ||
           (asset->themed && pixels == asset->themed + OSOS_BMAP_HEADER_SIZE);
}
static const unsigned char *asset_data(struct cfw_bitmap_asset *asset)
{
    unsigned key = cfw_theme_key();
    if (key == 129) return asset->data; /* exact original navy */
    if (!asset->themed && asset->theme_key != key) {
        asset->theme_key = key; /* avoid allocation retry storms after OOM */
        asset->themed = ((void *(*)(uint32_t))OSOS_IMPORT_ALLOC)(asset->size);
        if (asset->themed) asset->theme_key = 0;
    }
    if (!asset->themed) return cfw_dark_mode_enabled() ? asset->data : 0;
    if (asset->theme_key != key) {
        cfw_theme_bitmap(asset->themed, asset->data, asset->size, asset->role, cfw_theme_palette());
        asset->theme_key = key;
    }
    /* Stable lifetime: native views retain pixel pointers. Never free this
     * bounded one-copy-per-listed-asset cache or recolor it from itself. */
    return asset->themed;
}

PATCH_ARM const void *cfw_bitmap_resource_get(void *service, uint32_t type, uint32_t id)
{
    if (type == OSOS_RESOURCE_BMAP && active()) {
        for (uint32_t i = 0; i < cfw_bitmap_asset_count; i++) {
            if (cfw_bitmap_assets[i].id == id) {
                const void *data = asset_data(&cfw_bitmap_assets[i]);
                if (data) return data;
                break;
            }
        }
    }
    const void *resource = cfw_bitmap_resource_get_original(service, type, id);
    if (type == OSOS_RESOURCE_COLR)
        resource = cfw_dark_mode_color(id, resource);
    return resource;
}

static struct cfw_bitmap_asset *find_asset(void *service, uint32_t id,
                                                const unsigned char *pixels, int dark)
{
    for (uint32_t i = 0; i < cfw_bitmap_asset_count; i++) {
        struct cfw_bitmap_asset *asset = &cfw_bitmap_assets[i];
        if (attached(asset, pixels) || (dark && asset->id == id))
            return asset;
    }
    /* Light mode only needs to restore views still showing replacement pixels. */
    if (!dark)
        return 0;
    /* Dynamic properties identify the source by its resolved pixel storage. */
    for (uint32_t i = 0; i < cfw_bitmap_asset_count; i++) {
        struct cfw_bitmap_asset *asset = &cfw_bitmap_assets[i];
        if (!asset->original)
            asset->original = cfw_bitmap_resource_get_original(service, OSOS_RESOURCE_BMAP,
                                                        asset->id);
        if (asset->original && pixels == asset->original + OSOS_BMAP_HEADER_SIZE)
            return asset;
    }
    return 0;
}

PATCH_ARM void cfw_bitmap_view_load(void *view)
{
    cfw_bitmap_view_load_original(view);
    uint32_t type = osos_bitmap_view_type(view);
    if (!osos_bitmap_view_ready(view) ||
        (type != OSOS_RESOURCE_BMAP && type != OSOS_RESOURCE_DRAW))
        return;
    uint32_t id = osos_bitmap_view_id(view);
    void *context = osos_bitmap_view_context(view);
    void *bitmap = osos_bitmap_view_bitmap(view);
    void *service = osos_bitmap_view_service(view);
    const unsigned char *pixels = osos_bitmap_pixels(osos_graphics_bitmap(context));
    int dark = active();
    struct cfw_bitmap_asset *asset = find_asset(service, id, pixels, dark);
    if (!asset)
        return;
    const unsigned char *replacement = dark ? asset_data(asset) : 0;
    if ((replacement && pixels == replacement + OSOS_BMAP_HEADER_SIZE) ||
        (!replacement && !attached(asset, pixels)))
        return;
    if (!replacement && type == OSOS_RESOURCE_DRAW) {
        void *original = cfw_bitmap_resource_get_original(service, type, id);
        if (original && !attached(asset, osos_bitmap_pixels(original))) {
            osos_graphics_attach_bitmap(context, original);
            return;
        }
    }
    const void *data = replacement ? replacement :
        cfw_bitmap_resource_get_original(service, OSOS_RESOURCE_BMAP, asset->id);
    if (data) {
        /* Draw may use a shared bitmap; keep replacement pixels in this view. */
        osos_bitmap_load_resource(bitmap, data);
        osos_graphics_attach_bitmap(context, bitmap);
    }
}

PATCH_ARM void cfw_bitmap_cache_load(void *cache)
{
    cfw_bitmap_cache_load_original(cache);
    if (!osos_bitmap_cache_ready(cache))
        return;
    void *service = osos_bitmap_cache_service(cache);
    void *bitmap = osos_bitmap_cache_bitmap(cache);
    const unsigned char *pixels = osos_bitmap_pixels(bitmap);
    int dark = active();
    struct cfw_bitmap_asset *asset = find_asset(
        service, osos_bitmap_cache_id(cache), pixels, dark);
    if (!asset) return;
    const unsigned char *replacement = dark ? asset_data(asset) : 0;
    if ((replacement && pixels == replacement + OSOS_BMAP_HEADER_SIZE) ||
        (!replacement && !attached(asset, pixels))) return;
    const void *data = replacement ? replacement :
        cfw_bitmap_resource_get_original(service, OSOS_RESOURCE_BMAP, asset->id);
    if (data)
        osos_bitmap_load_resource(bitmap, data);
}
