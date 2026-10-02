#include "GPU3D_TextureUpscaler.h"

#include <algorithm>

namespace melonDS
{

namespace
{

// MMPX's luminance, weighting opaque texels more (reference code)
u32 Luma(u32 c)
{
    u32 alpha = c >> 24;
    return ((c & 0xFF) + ((c >> 8) & 0xFF) + ((c >> 16) & 0xFF) + 1) * (256 - alpha);
}

bool AllEq2(u32 b, u32 a0, u32 a1) { return b == a0 && b == a1; }
bool AllEq3(u32 b, u32 a0, u32 a1, u32 a2) { return b == a0 && b == a1 && b == a2; }
bool AllEq4(u32 b, u32 a0, u32 a1, u32 a2, u32 a3) { return b == a0 && b == a1 && b == a2 && b == a3; }
bool AnyEq3(u32 b, u32 a0, u32 a1, u32 a2) { return b == a0 || b == a1 || b == a2; }
bool NoneEq2(u32 b, u32 a0, u32 a1) { return b != a0 && b != a1; }
bool NoneEq4(u32 b, u32 a0, u32 a1, u32 a2, u32 a3) { return b != a0 && b != a1 && b != a2 && b != a3; }

int Wrap(int v, int size, TextureUpscaler::Edge edge)
{
    switch (edge)
    {
    case TextureUpscaler::Edge::Repeat:
        v %= size;
        return v < 0 ? v + size : v;
    case TextureUpscaler::Edge::Mirror:
    {
        int period = size * 2;
        v %= period;
        if (v < 0) v += period;
        return v < size ? v : period - 1 - v;
    }
    default:
        return v < 0 ? 0 : (v >= size ? size - 1 : v);
    }
}

// MMPX reads up to 3 texels around each texel
constexpr int Pad = 3;

// Texels past a clamped edge. Repeating the edge texel (what the DS shows
// there) turns a diagonal that runs off the texture into a straight bar at
// the edge, which MMPX keeps square: the last two texels stayed blocky. The
// pattern is continued instead: locally, the inner line (one texel in from
// the edge) is the edge line shifted by d texels, so the k-th line outside is
// the edge line shifted by -k*d. d = 0 (a plain repeat) unless a shift
// explains the neighbourhood strictly better.
//   edge/inner: the two lines next to the edge, n texels each
//   out: k-th line outside the edge, k = 1..Pad
void ExtrapolateLine(const u32* edge, const u32* inner, int n, int k, u32* out)
{
    for (int i = 0; i < n; i++)
    {
        int bestShift = 0, bestMismatches = 1 << 30;
        for (int d : {0, 1, -1, 2, -2})
        {
            int mismatches = 0, pairs = 0;
            for (int j = i - 2; j <= i + 2; j++)
            {
                if (j < 0 || j >= n || j - d < 0 || j - d >= n) continue;
                pairs++;
                if (inner[j] != edge[j - d]) mismatches++;
            }
            if (pairs >= 2 && mismatches < bestMismatches)
            {
                bestShift = d;
                bestMismatches = mismatches;
            }
        }
        int src = i + k * bestShift;
        out[i] = edge[src < 0 ? 0 : (src >= n ? n - 1 : src)];
    }
}

// The texture with Pad texels of border on each side, as the DS samples it past
// its edges (repeat, mirror) or continued (clamp, see ExtrapolateLine).
std::vector<u32> PadTexture(const u32* src, int w, int h, TextureUpscaler::Edge edgeS, TextureUpscaler::Edge edgeT)
{
    const int pw = w + Pad * 2, ph = h + Pad * 2;
    std::vector<u32> padded((size_t)pw * ph);
    auto at = [&](int x, int y) -> u32& { return padded[(size_t)(y + Pad) * pw + (x + Pad)]; };

    // rows, on the texture's width
    std::vector<u32> line(std::max(w, h) + Pad * 2);
    for (int y = -Pad; y < h + Pad; y++)
    {
        if (y >= 0 && y < h)
            for (int x = 0; x < w; x++) at(x, y) = src[y * w + x];
        else if (edgeT != TextureUpscaler::Edge::Clamp || h < 2)
            for (int x = 0; x < w; x++) at(x, y) = src[Wrap(y, h, edgeT) * w + x];
        else
        {
            bool top = y < 0;
            const u32* edge = &src[(top ? 0 : h - 1) * w];
            const u32* inner = &src[(top ? 1 : h - 2) * w];
            ExtrapolateLine(edge, inner, w, top ? -y : y - (h - 1), line.data());
            for (int x = 0; x < w; x++) at(x, y) = line[x];
        }
    }

    // columns, on the padded height (corners follow the extended rows)
    std::vector<u32> edgeCol(ph), innerCol(ph);
    for (int side = 0; side < 2 && w >= 2; side++)
    {
        int ex = side ? w - 1 : 0, ix = side ? w - 2 : 1;
        for (int y = 0; y < ph; y++)
        {
            edgeCol[y] = at(ex, y - Pad);
            innerCol[y] = at(ix, y - Pad);
        }
        for (int k = 1; k <= Pad; k++)
        {
            int x = side ? w - 1 + k : -k;
            if (edgeS != TextureUpscaler::Edge::Clamp)
            {
                int sx = Wrap(x, w, edgeS);
                for (int y = -Pad; y < h + Pad; y++) at(x, y) = at(sx, y);
            }
            else
            {
                ExtrapolateLine(edgeCol.data(), innerCol.data(), ph, k, line.data());
                for (int y = -Pad; y < h + Pad; y++) at(x, y) = line[y + Pad];
            }
        }
    }
    if (w < 2 && edgeS == TextureUpscaler::Edge::Clamp)
        for (int y = -Pad; y < h + Pad; y++)
            for (int k = 1; k <= Pad; k++) at(-k, y) = at(w - 1 + k, y) = at(0, y);

    return padded;
}

}

TextureUpscaler::Edge TextureUpscaler::EdgeFromTexParam(u32 texParam, int axis)
{
    // bit 16/17: repeat S/T, bit 18/19: flip (mirror) S/T when repeating
    if (!(texParam & (1 << (16 + axis))))
        return Edge::Clamp;
    return (texParam & (1 << (18 + axis))) ? Edge::Mirror : Edge::Repeat;
}

void TextureUpscaler::MMPX2x(const u32* srcBuffer, u32 width, u32 height, Edge edgeS, Edge edgeT, u32* dst)
{
    const int w = (int)width, h = (int)height;
    const std::vector<u32> padded = PadTexture(srcBuffer, w, h, edgeS, edgeT);
    const int pw = w + Pad * 2;
    auto src = [&](int x, int y) { return padded[(size_t)(y + Pad) * pw + (x + Pad)]; };

    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            u32 A = src(x-1, y-1), B = src(x, y-1), C = src(x+1, y-1);
            u32 D = src(x-1, y),   E = src(x, y),   F = src(x+1, y);
            u32 G = src(x-1, y+1), H = src(x, y+1), I = src(x+1, y+1);

            u32 J = E, K = E, L = E, M = E;

            if (A != E || B != E || C != E || D != E || F != E || G != E || H != E || I != E)
            {
                u32 P = src(x, y-2), Q = src(x-2, y), R = src(x+2, y), S = src(x, y+2);
                u32 Bl = Luma(B), Dl = Luma(D), El = Luma(E), Fl = Luma(F), Hl = Luma(H);

                // 1:1 slope rules
                if ((D == B && D != H && D != F) && (El >= Dl || E == A) && AnyEq3(E, A, C, G) && ((El < Dl) || A != D || E != P || E != Q)) J = D;
                if ((B == F && B != D && B != H) && (El >= Bl || E == C) && AnyEq3(E, A, C, I) && ((El < Bl) || C != B || E != P || E != R)) K = B;
                if ((H == D && H != F && H != B) && (El >= Hl || E == G) && AnyEq3(E, A, G, I) && ((El < Hl) || G != H || E != S || E != Q)) L = H;
                if ((F == H && F != B && F != D) && (El >= Fl || E == I) && AnyEq3(E, C, G, I) && ((El < Fl) || I != H || E != R || E != S)) M = F;

                // intersection rules
                if ((E != F && AllEq4(E, C, I, D, Q) && AllEq2(F, B, H)) && (F != src(x+3, y))) K = M = F;
                if ((E != D && AllEq4(E, A, G, F, R) && AllEq2(D, B, H)) && (D != src(x-3, y))) J = L = D;
                if ((E != H && AllEq4(E, G, I, B, P) && AllEq2(H, D, F)) && (H != src(x, y+3))) L = M = H;
                if ((E != B && AllEq4(E, A, C, H, S) && AllEq2(B, D, F)) && (B != src(x, y-3))) J = K = B;
                if (Bl < El && AllEq4(E, G, H, I, S) && NoneEq4(E, A, D, C, F)) J = K = B;
                if (Hl < El && AllEq4(E, A, B, C, P) && NoneEq4(E, D, G, I, F)) L = M = H;
                if (Fl < El && AllEq4(E, A, D, G, Q) && NoneEq4(E, B, C, I, H)) K = M = F;
                if (Dl < El && AllEq4(E, C, F, I, R) && NoneEq4(E, B, A, G, H)) J = L = D;

                // 2:1 slope rules
                if (H != B)
                {
                    if (H != A && H != E && H != C)
                    {
                        if (AllEq3(H, G, F, R) && NoneEq2(H, D, src(x+2, y-1))) L = M;
                        if (AllEq3(H, I, D, Q) && NoneEq2(H, F, src(x-2, y-1))) M = L;
                    }
                    if (B != I && B != G && B != E)
                    {
                        if (AllEq3(B, A, F, R) && NoneEq2(B, D, src(x+2, y+1))) J = K;
                        if (AllEq3(B, C, D, Q) && NoneEq2(B, F, src(x-2, y+1))) K = J;
                    }
                }
                if (F != D)
                {
                    if (D != I && D != E && D != C)
                    {
                        if (AllEq3(D, A, H, S) && NoneEq2(D, B, src(x+1, y+2))) J = L;
                        if (AllEq3(D, G, B, P) && NoneEq2(D, H, src(x+1, y-2))) L = J;
                    }
                    if (F != E && F != A && F != G)
                    {
                        if (AllEq3(F, C, H, S) && NoneEq2(F, B, src(x-1, y+2))) K = M;
                        if (AllEq3(F, I, B, P) && NoneEq2(F, H, src(x-1, y-2))) M = K;
                    }
                }
            }

            u32* out = &dst[(y * 2) * (w * 2) + x * 2];
            out[0] = J;
            out[1] = K;
            out[w * 2] = L;
            out[w * 2 + 1] = M;
        }
    }
}

void TextureUpscaler::Upscale(const u32* decoded, u32 width, u32 height, int factor, bool binaryAlpha,
                              Edge edgeS, Edge edgeT, std::vector<u32>& out)
{
    // RGB6A5 -> RGBA8; fully transparent texels are all zero, so they compare equal
    std::vector<u32> rgba(width * height);
    for (u32 i = 0; i < width * height; i++)
    {
        u32 c = decoded[i];
        u32 r = c & 0x3F, g = (c >> 8) & 0x3F, b = (c >> 16) & 0x3F, a = (c >> 24) & 0x1F;
        u32 a8 = binaryAlpha ? (a ? 255 : 0) : (a * 255 + 15) / 31;
        rgba[i] = a8 ? (((r << 2) | (r >> 4)) | (((g << 2) | (g >> 4)) << 8) | (((b << 2) | (b >> 4)) << 16) | (a8 << 24)) : 0;
    }

    u32 w = width, h = height;
    for (int f = 1; f < factor; f *= 2)
    {
        std::vector<u32> next((size_t)w * h * 4);
        MMPX2x(rgba.data(), w, h, edgeS, edgeT, next.data());
        rgba.swap(next);
        w *= 2;
        h *= 2;
    }
    out.swap(rgba);
}

}
