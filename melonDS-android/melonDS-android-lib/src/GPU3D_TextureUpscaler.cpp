#include "GPU3D_TextureUpscaler.h"

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
    auto src = [&](int x, int y) { return srcBuffer[Wrap(y, h, edgeT) * w + Wrap(x, w, edgeS)]; };

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
