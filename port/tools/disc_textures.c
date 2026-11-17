// Every texture on the disc into the texture catalog (T6_TEXCATALOG's), without playing through the game.
//
// The game's data (USRDIR's files on the disc are only the index: ~384 MB after the file system,
// read by sector) is one block encrypted by position: its first sector holds the key (two words
// it points to through the tables at 0x08bce780/0x08bce788 of the executable, FUN_08aa2e88), each
// 1 KB after it a seed from its number (FUN_08aa2ca8: x = n + k0, x += bitreverse(x), rotated
// right by (3n mod 29) + 2, xor k1), each word the next of x = rotl(x, 3) * 5 + 1. Decrypted, the
// files sit in it as they are loaded, most of them in "GZIP" blocks ("GZIP", 1, 1, the size
// unpacked, the size packed, then a zlib stream). The textures come in "NTPP" packages of textures, each a 0x50-byte header
// (sizes, palette and texel formats, width, height; "eXt" and "GIDX" chunks), its texels, then
// its palette.
//
// Each texture is decoded as runtime/ge.c decodes it (the same conversions, unswizzling, DXT
// blocks) and written as T6_TEXCATALOG writes what the game draws: <key>.rgba and a line in
// index.txt "key w h fmt offset scene 0 where" with scene 101 (the disc): offset the GZIP block's
// (or the texture's, outside them), where "g<block>+<offset in it>" or "p<offset>". Only textures
// not in the folder yet get a line (what the game was seen drawing keeps its place). The key covers the picture
// itself (w x h, as the texture pack looks a picture up inside a bigger texture), so a texture the
// game draws at a power-of-two size bigger than its picture is found too.
//
//     cc -O2 -o build/disc_textures tools/disc_textures.c -lz
//     build/disc_textures GAME.iso EBOOT_decrypted.elf build/texcatalog
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <zlib.h>

#define DATA_LBN 0x38370u
#define DATA_SIZE 0x16e42800u
#define SCENE_DISC 101

static uint32_t bitrev(uint32_t x) {
	uint32_t r = 0;
	for (int i = 0; i < 32; i++) r |= ((x >> i) & 1u) << (31 - i);
	return r;
}
static uint32_t rotr(uint32_t x, unsigned n) {
	n &= 31;
	return n ? (x >> n) | (x << (32 - n)) : x;
}
static uint32_t seed(uint32_t n, uint32_t k0, uint32_t k1) {
	uint32_t x = n + k0;
	x += bitrev(x);
	return rotr(x, (uint32_t)((int32_t)(3 * n) % 29) + 2) ^ k1;
}

// ---- runtime/ge.c's texel conversions -------------------------------------------------------------
static uint32_t expand565(uint16_t v) {
	uint32_t r = v & 31, g = (v >> 5) & 63, b = (v >> 11) & 31;
	return 0xFF000000u | ((b << 3 | b >> 2) << 16) | ((g << 2 | g >> 4) << 8) | (r << 3 | r >> 2);
}
static uint32_t expand5551(uint16_t v) {
	uint32_t r = v & 31, g = (v >> 5) & 31, b = (v >> 10) & 31;
	return ((v >> 15) ? 0xFF000000u : 0) | ((b << 3 | b >> 2) << 16) | ((g << 3 | g >> 2) << 8) | (r << 3 | r >> 2);
}
static uint32_t expand4444(uint16_t v) {
	uint32_t r = v & 15, g = (v >> 4) & 15, b = (v >> 8) & 15, a = (v >> 12) & 15;
	return (a * 17) << 24 | (b * 17) << 16 | (g * 17) << 8 | r * 17;
}
static uint32_t conv16(uint16_t v, int fmt) {
	return fmt == 0 ? expand565(v) : fmt == 1 ? expand5551(v) : expand4444(v);
}
static uint32_t clut_at(const uint8_t *clut, int cfmt, uint32_t i) {  // CLUT mode: no shift, mask 0xff, start 0
	i &= 0xFF;
	if (cfmt == 3) {
		uint32_t v;
		memcpy(&v, clut + i * 4, 4);
		return v;
	}
	uint16_t v;
	memcpy(&v, clut + i * 2, 2);
	return conv16(v, cfmt);
}

static const int BITS[11] = {16, 16, 16, 32, 4, 8, 16, 32, 4, 8, 8};  // per texel (DXT: per texel in a block)

// Decodes a texture (fmt as the GE's, swz, stride in texels) into w x h RGBA.
static uint32_t *decode(const uint8_t *src, size_t bytes, int fmt, int swz, int w, int h, int stride, const uint8_t *clut, int cfmt) {
	uint32_t *out = calloc((size_t)w * h, 4);
	if (fmt >= 8) {  // PSP DXT blocks: indices first, then the two 565 colors (alpha after for DXT3/5)
		int bs = fmt == 8 ? 8 : 16;
		for (int by = 0; by < (h + 3) / 4; by++)
			for (int bx = 0; bx < (w + 3) / 4; bx++) {
				size_t bo = ((size_t)by * ((w + 3) / 4) + bx) * bs;
				if (bo + bs > bytes) continue;
				const uint8_t *b = src + bo;
				uint32_t lines;
				uint16_t c1, c2;
				memcpy(&lines, b, 4);
				memcpy(&c1, b + 4, 2);
				memcpy(&c2, b + 6, 2);
				uint32_t pal[4];
				pal[0] = expand565(c1);
				pal[1] = expand565(c2);
				for (int k = 0; k < 2; k++) pal[k] = (pal[k] & 0xFF00FF00u) | (pal[k] & 0xFFu) << 16 | ((pal[k] >> 16) & 0xFFu);
				for (int k = 0; k < 3; k++) {
					uint32_t a = (pal[0] >> (k * 8)) & 0xFF, bb = (pal[1] >> (k * 8)) & 0xFF, m2, m3;
					if (c1 > c2 || fmt != 8) m2 = (2 * a + bb) / 3, m3 = (a + 2 * bb) / 3;
					else m2 = (a + bb) / 2, m3 = 0;
					pal[2] = (k ? pal[2] : 0xFF000000u) | (m2 << (k * 8));
					pal[3] = (k ? pal[3] : ((c1 > c2 || fmt != 8) ? 0xFF000000u : 0)) | (m3 << (k * 8));
				}
				for (int py = 0; py < 4; py++)
					for (int px = 0; px < 4; px++) {
						int x = bx * 4 + px, y = by * 4 + py;
						if (x >= w || y >= h) continue;
						uint32_t c = pal[(lines >> ((py * 4 + px) * 2)) & 3];
						if (fmt == 9) {
							uint16_t al;
							memcpy(&al, b + 8 + py * 2, 2);
							c = (c & 0xFFFFFFu) | (((al >> (px * 4)) & 0xF) * 17u) << 24;
						}
						else if (fmt == 10) {
							uint8_t a0 = b[14], a1 = b[15];
							uint64_t bits = 0;
							memcpy(&bits, b + 8, 6);
							int idx = (int)((bits >> ((py * 4 + px) * 3)) & 7);
							uint32_t a;
							if (idx == 0) a = a0;
							else if (idx == 1) a = a1;
							else if (a0 > a1) a = ((8 - idx) * a0 + (idx - 1) * a1) / 7;
							else a = idx == 6 ? 0 : idx == 7 ? 255 : ((6 - idx) * a0 + (idx - 1) * a1) / 5;
							c = (c & 0xFFFFFFu) | a << 24;
						}
						out[y * w + x] = c;
					}
			}
		return out;
	}
	size_t row_bytes = (size_t)stride * BITS[fmt] / 8;
	uint8_t *lin = calloc(1, row_bytes * h + (size_t)w * 4 + 16);
	if (swz) {
		size_t bw = row_bytes / 16;
		for (int y = 0; y < h; y++)
			for (size_t xb = 0; xb < row_bytes; xb++) {
				size_t so = ((size_t)(y / 8) * bw + xb / 16) * 128 + (size_t)(y % 8) * 16 + xb % 16;
				if (so < bytes) lin[(size_t)y * row_bytes + xb] = src[so];
			}
	}
	else memcpy(lin, src, bytes < row_bytes * h ? bytes : row_bytes * h);
	for (int y = 0; y < h; y++) {
		const uint8_t *row = lin + (size_t)y * row_bytes;
		for (int x = 0; x < w; x++) {
			uint32_t px;
			switch (fmt) {
			case 0: case 1: case 2: { uint16_t v; memcpy(&v, row + x * 2, 2); px = conv16(v, fmt); break; }
			case 3: memcpy(&px, row + x * 4, 4); break;
			case 4: px = clut_at(clut, cfmt, (row[x / 2] >> ((x & 1) * 4)) & 0xF); break;
			case 5: px = clut_at(clut, cfmt, row[x]); break;
			default: px = 0;
			}
			out[(size_t)y * w + x] = px;
		}
	}
	free(lin);
	return out;
}

// ---- runtime/texpack.c's key: CRC-32 and Adler-32 of the RGBA ----------------------------------
static uint32_t crc_table[256];
static uint32_t crc_update(uint32_t crc, const uint8_t *p, size_t n) {
	if (!crc_table[1])
		for (uint32_t i = 0; i < 256; i++) {
			uint32_t c = i;
			for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
			crc_table[i] = c;
		}
	crc = ~crc;
	while (n--) crc = crc_table[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
	return ~crc;
}
static uint32_t adler_update(uint32_t adler, const uint8_t *p, size_t n) {
	uint32_t a = adler & 0xffff, b = adler >> 16;
	while (n) {
		size_t k = n < 5552 ? n : 5552;
		n -= k;
		while (k--) a += *p++, b += a;
		a %= 65521, b %= 65521;
	}
	return b << 16 | a;
}

static FILE *idx;
static const char *dir;
static int packages, found, written, odd, palettes, recolored;

// One texture of a package decoded and written (palette clut, cfmt; tag: which palette).
static void emit(const uint8_t *texels, uint32_t dat_size, int fmt, int swz, int w, int h, int stride, const uint8_t *clut, int cfmt,
                 size_t block, bool in_block, size_t q, int pal) {
	char path[1024];
	uint32_t *px = decode(texels, dat_size, fmt, swz, w, h, stride, clut, cfmt);
	uint32_t crc = crc_update(0, (const uint8_t *)px, (size_t)w * h * 4), ad = adler_update(1, (const uint8_t *)px, (size_t)w * h * 4);
	char key[17];
	snprintf(key, sizeof key, "%08x%08x", crc, ad);
	snprintf(path, sizeof path, "%s/%s.rgba", dir, key);
	struct stat st;
	if (stat(path, &st) != 0) {
		FILE *o = fopen(path, "wb");
		uint16_t wh[2] = {(uint16_t)w, (uint16_t)h};
		fwrite(wh, 2, 2, o);
		fwrite(px, 4, (size_t)w * h, o);
		fclose(o);
		written++;
		if (pal) recolored++;
		char where[64];
		if (in_block) snprintf(where, sizeof where, "g%zx+%zx", block, q);
		else snprintf(where, sizeof where, "p%zx", q);
		if (pal) snprintf(where + strlen(where), sizeof where - strlen(where), "/pal%d", pal);
		fprintf(idx, "%s %d %d %d %08zx %d 0 %s\n", key, w, h, fmt, in_block ? block : q, SCENE_DISC, where);
	}
	free(px);
}

typedef struct {
	const uint8_t *texels, *clut;
	uint32_t dat_size, pal_size;
	int fmt, swz, w, h, stride, cfmt;
	size_t q;
} Tex;

// The NTPP packages in buf: block is the GZIP block's offset (in_block) or 0. A package's palettes
// alone (no texels) are other colors for its textures (the characters' costumes): every texture
// with a palette of that size is decoded with each of them too.
static void scan(const uint8_t *buf, size_t size, size_t block, bool in_block) {
	static Tex tex[4096];
	static struct { const uint8_t *p; uint32_t size; int cfmt; } pals[4096];
	for (size_t p = 0; p + 16 <= size; p += 16) {
		if (memcmp(buf + p, "NTPP", 4)) continue;
		uint16_t count;
		memcpy(&count, buf + p + 6, 2);
		packages++;
		int nt = 0, np = 0;
		size_t q = p + 16;
		for (int t = 0; t < count; t++) {
			if (q + 0x50 > size) break;
			const uint8_t *hd = buf + q;
			uint32_t total, pal_size, dat_size;
			uint16_t hsize, w, h;
			memcpy(&total, hd, 4);
			memcpy(&pal_size, hd + 4, 4);
			memcpy(&dat_size, hd + 8, 4);
			memcpy(&hsize, hd + 12, 2);
			uint8_t mips = hd[17], cfmt = hd[18], tf = hd[19];
			memcpy(&w, hd + 20, 2);
			memcpy(&h, hd + 22, 2);
			int fmt = tf & 0x7f, swz = (tf & 0x80) != 0;
			if (hsize >= 0x30 && total >= hsize && q + total <= size && !w && !h && !dat_size) {  // a palette alone
				palettes++;
				if (np < 4096 && cfmt <= 3 && pal_size && (size_t)hsize + pal_size <= total)
					pals[np].p = hd + hsize, pals[np].size = pal_size, pals[np].cfmt = cfmt, np++;
				q += total;
				continue;
			}
			if (hsize < 0x30 || total < hsize || q + total > size || fmt > 10 || fmt == 6 || fmt == 7 || !w || !h || w > 1024 || h > 1024 ||
			    (size_t)hsize + dat_size > total) {
				if (getenv("T6_DISC_ODD") && odd < 12) {
					fprintf(stderr, "odd at %zx+%zx:", block, q);
					for (int i = 0; i < 0x30; i++) fprintf(stderr, "%s%02x", i % 4 ? "" : " ", hd[i]);
					fprintf(stderr, "\n");
				}
				odd++;
				break;
			}
			const uint8_t *texels = hd + hsize, *clut = texels + dat_size;
			if ((fmt == 4 || fmt == 5) && (cfmt > 3 || !pal_size || (size_t)hsize + dat_size + pal_size > total)) {
				if (getenv("T6_DISC_ODD")) fprintf(stderr, "odd palette at %zx+%zx: cfmt %d pal %u\n", block, q, cfmt, pal_size);
				odd++;
				q += total;
				continue;
			}
			found++;
			// the rows' length (texels): what the size says, the first level's (mipmaps follow it)
			int stride = w;
			if (fmt < 8 && mips <= 1) {
				size_t row_bytes = dat_size / h;
				int s = (int)(row_bytes * 8 / BITS[fmt]);
				if (s >= w) stride = s;
			}
			if (fmt < 8 && swz && (size_t)stride * BITS[fmt] / 8 % 16) stride = (int)((((size_t)stride * BITS[fmt] / 8 + 15) / 16 * 16) * 8 / BITS[fmt]);
			if (nt < 4096) tex[nt++] = (Tex){texels, clut, dat_size, pal_size, fmt, swz, w, h, stride, cfmt, q};
			q += total;
		}
		for (int i = 0; i < nt; i++) {
			Tex *t = &tex[i];
			emit(t->texels, t->dat_size, t->fmt, t->swz, t->w, t->h, t->stride, t->clut, t->cfmt, block, in_block, t->q, 0);
			if (t->fmt != 4 && t->fmt != 5) continue;
			for (int k = 0; k < np; k++)
				if (pals[k].size == t->pal_size && pals[k].cfmt == t->cfmt)
					emit(t->texels, t->dat_size, t->fmt, t->swz, t->w, t->h, t->stride, pals[k].p, pals[k].cfmt, block, in_block, t->q, k + 1);
		}
	}
}

int main(int argc, char **argv) {
	if (argc != 4) {
		fprintf(stderr, "usage: %s GAME.iso EBOOT_decrypted.elf CATALOG_DIR\n", argv[0]);
		return 2;
	}
	// the key's tables in the executable (its one LOAD segment)
	FILE *ef = fopen(argv[2], "rb");
	if (!ef) return perror(argv[2]), 1;
	uint8_t eh[0x34], ph[0x20];
	if (fread(eh, 1, sizeof eh, ef) != sizeof eh) return 1;
	uint32_t phoff, p_off, p_vaddr, t780[2], t788[2];
	memcpy(&phoff, eh + 0x1c, 4);
	fseek(ef, phoff, SEEK_SET);
	if (fread(ph, 1, sizeof ph, ef) != sizeof ph) return 1;
	memcpy(&p_off, ph + 4, 4);
	memcpy(&p_vaddr, ph + 8, 4);
	fseek(ef, p_off + 0x08bce780u - p_vaddr, SEEK_SET);
	if (fread(t780, 4, 2, ef) != 2 || fread(t788, 4, 2, ef) != 2) return 1;
	fclose(ef);

	FILE *iso = fopen(argv[1], "rb");
	if (!iso) return perror(argv[1]), 1;
	uint8_t *blob = malloc(DATA_SIZE);
	fseeko(iso, (off_t)DATA_LBN * 0x800, SEEK_SET);
	if (!blob || fread(blob, 1, DATA_SIZE, iso) != DATA_SIZE) return fprintf(stderr, "can't read the data\n"), 1;
	fclose(iso);
	uint32_t *w32 = (uint32_t *)blob, k[2];
	for (int i = 0; i < 2; i++) {
		uint32_t at = w32[t788[i]] ^ t780[i];
		if (at >= 0x200) return fprintf(stderr, "no key in the first sector\n"), 1;
		k[i] = w32[at];
	}
	for (uint32_t n = 0; n < DATA_SIZE / 1024; n++) {  // in place, the first sector too (not used)
		uint32_t s = seed(n, k[0], k[1]);
		for (int j = 0; j < 256; j++) w32[n * 256 + j] ^= s, s = rotr(s, 29) * 5 + 1;
	}

	mkdir(argv[3], 0755);
	char path[1024];
	snprintf(path, sizeof path, "%s/index.txt", argv[3]);
	idx = fopen(path, "a");
	if (!idx) return perror(path), 1;
	dir = argv[3];
	scan(blob, DATA_SIZE, 0, false);
	int blocks = 0, broken = 0;
	for (size_t p = 0; p + 20 <= DATA_SIZE; p += 16) {
		if (memcmp(blob + p, "GZIP", 4)) continue;
		uint32_t hdr[4];
		memcpy(hdr, blob + p + 4, 16);
		uLongf usize = hdr[2];
		if (hdr[0] != 1 || hdr[1] != 1 || p + 20 + hdr[3] > DATA_SIZE || usize > 64u << 20) {
			broken++;
			continue;
		}
		uint8_t *u = malloc(usize ? usize : 1);
		if (uncompress(u, &usize, blob + p + 20, hdr[3]) != Z_OK) broken++;
		else blocks++, scan(u, usize, p, true);
		free(u);
	}
	fclose(idx);
	printf("%d GZIP blocks (%d not unpacked), %d packages, %d textures, %d new (%d in other colors), %d palettes alone, %d left (unknown layout)\n",
	       blocks, broken, packages, found, written, recolored, palettes, odd);
	return 0;
}
