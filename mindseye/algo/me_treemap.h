#pragma once


#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float x, y, w, h;
} meTreemapRect;

typedef struct {
    float value;
    void* userData;
    meTreemapRect rect;
} meTreemapItem;

typedef struct {
    meTreemapItem* items;
    int count;
} meTreemapResult;

// Computes a squarified treemap layout for the given values.
// - values: array of positive floats (area for each item)
// - count: number of items
// - userData: array of pointers, one per item (can be NULL)
// - bounds: rectangle to fill (x, y, w, h)
// - outRects: output array of meTreemapRect, must have at least count elements
void me_treemap_squarify(
    const float* values,
    int count,
    void** userData,
    meTreemapRect bounds,
    meTreemapItem* outItems
);

#ifdef __cplusplus
}
#endif


#ifdef ME_TREEMAP_IMPLEMENTATION


// Normalize sizes so that sum(sizes) == w * h
static void me_treemap_normalize_sizes(float* sizes, int count, float w, float h)
{
    float total = 0.0f;
    for (int i = 0; i < count; ++i) total += sizes[i];
    float area = w * h;
    for (int i = 0; i < count; ++i) sizes[i] = sizes[i] * area / total;
}

// Pad a rectangle (for visual separation)
static void me_treemap_pad_rect(meTreemapRect* rect)
{
    if (rect->w > 2) {
        rect->x += 1;
        rect->w -= 2;
    }
    if (rect->h > 2) {
        rect->y += 1;
        rect->h -= 2;
    }
}

// Layout a row (dx >= dy)
static void me_treemap_layout_row(const float* sizes, int n, float x, float y, float dx, float dy, meTreemapRect* outRects)
{
    float covered_area = 0.0f;
    for (int i = 0; i < n; ++i) covered_area += sizes[i];
    float width = covered_area / dy;
    float yoff = y;
    for (int i = 0; i < n; ++i) {
        float h = sizes[i] / width;
        outRects[i].x = x;
        outRects[i].y = yoff;
        outRects[i].w = width;
        outRects[i].h = h;
        yoff += h;
    }
}

// Layout a column (dx < dy)
static void me_treemap_layout_col(const float* sizes, int n, float x, float y, float dx, float dy, meTreemapRect* outRects)
{
    float covered_area = 0.0f;
    for (int i = 0; i < n; ++i) covered_area += sizes[i];
    float height = covered_area / dx;
    float xoff = x;
    for (int i = 0; i < n; ++i) {
        float w = sizes[i] / height;
        outRects[i].x = xoff;
        outRects[i].y = y;
        outRects[i].w = w;
        outRects[i].h = height;
        xoff += w;
    }
}

// Layout helper
static void me_treemap_layout(const float* sizes, int n, float x, float y, float dx, float dy, meTreemapRect* outRects)
{
    if (dx >= dy)
        me_treemap_layout_row(sizes, n, x, y, dx, dy, outRects);
    else
        me_treemap_layout_col(sizes, n, x, y, dx, dy, outRects);
}

// Compute leftover rectangle after placing a row (dx >= dy)
static void me_treemap_leftover_row(const float* sizes, int n, float x, float y, float dx, float dy, float* out_x, float* out_y, float* out_dx, float* out_dy)
{
    float covered_area = 0.0f;
    for (int i = 0; i < n; ++i) covered_area += sizes[i];
    float width = covered_area / dy;
    *out_x = x + width;
    *out_y = y;
    *out_dx = dx - width;
    *out_dy = dy;
}

// Compute leftover rectangle after placing a column (dx < dy)
static void me_treemap_leftover_col(const float* sizes, int n, float x, float y, float dx, float dy, float* out_x, float* out_y, float* out_dx, float* out_dy)
{
    float covered_area = 0.0f;
    for (int i = 0; i < n; ++i) covered_area += sizes[i];
    float height = covered_area / dx;
    *out_x = x;
    *out_y = y + height;
    *out_dx = dx;
    *out_dy = dy - height;
}

// Compute leftover rectangle after placing a row or column
static void me_treemap_leftover(const float* sizes, int n, float x, float y, float dx, float dy, float* out_x, float* out_y, float* out_dx, float* out_dy)
{
    if (dx >= dy)
        me_treemap_leftover_row(sizes, n, x, y, dx, dy, out_x, out_y, out_dx, out_dy);
    else
        me_treemap_leftover_col(sizes, n, x, y, dx, dy, out_x, out_y, out_dx, out_dy);
}

// Compute the worst aspect ratio for a group of sizes in the given rectangle
static float me_treemap_worst_ratio(const float* sizes, int n, float x, float y, float dx, float dy)
{
    meTreemapRect rects[32]; // up to 32 items per row/col
    if (n > 32) return 1e9f; // fallback for large n
    me_treemap_layout(sizes, n, x, y, dx, dy, rects);
    float worst = 0.0f;
    for (int i = 0; i < n; ++i) {
        float rw = rects[i].w, rh = rects[i].h;
        float ratio = rw > rh ? rw / rh : rh / rw;
        if (ratio > worst) worst = ratio;
    }
    return worst;
}

// Recursive squarify
static int me_treemap_squarify_impl(
    const float* sizes, int count, void** userData,
    float x, float y, float dx, float dy,
    meTreemapItem* outItems, int outOffset)
{
    if (count == 0)
        return outOffset;
    if (count == 1) {
        outItems[outOffset].rect.x = x;
        outItems[outOffset].rect.y = y;
        outItems[outOffset].rect.w = dx;
        outItems[outOffset].rect.h = dy;
        outItems[outOffset].value = sizes[0];
        outItems[outOffset].userData = userData ? userData[0] : NULL;
        return outOffset + 1;
    }

    int i = 1;
    while (i < count &&
        me_treemap_worst_ratio(sizes, i, x, y, dx, dy) >=
        me_treemap_worst_ratio(sizes, i + 1, x, y, dx, dy))
    {
        ++i;
    }

    // Layout current group
    meTreemapRect rects[32];
    me_treemap_layout(sizes, i, x, y, dx, dy, rects);
    for (int j = 0; j < i; ++j) {
        outItems[outOffset + j].rect = rects[j];
        outItems[outOffset + j].value = sizes[j];
        outItems[outOffset + j].userData = userData ? userData[j] : NULL;
    }

    // Compute leftover rectangle
    float lx, ly, ldx, ldy;
    me_treemap_leftover(sizes, i, x, y, dx, dy, &lx, &ly, &ldx, &ldy);

    // Recurse for the rest
    return me_treemap_squarify_impl(
        sizes + i, count - i, userData ? userData + i : NULL,
        lx, ly, ldx, ldy,
        outItems, outOffset + i);
}

// Public API
static void me_treemap_squarify(
    const float* sizes,
    int count,
    void** userData,
    meTreemapRect bounds,
    meTreemapItem* outItems,
    bool pad = true)
{
    me_treemap_squarify_impl(sizes, count, userData,
        bounds.x, bounds.y, bounds.w, bounds.h,
        outItems, 0);
    if (pad)
    {
        for (int i = 0; i < count; i++)
        {
            me_treemap_pad_rect(&outItems[i].rect);
        }
    }
}

#endif
