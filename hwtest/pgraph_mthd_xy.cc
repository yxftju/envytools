/*
 * Copyright (C) 2016 Marcin Kościelnicki <koriakin@0x04.net>
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "pgraph.h"
#include "pgraph_mthd.h"
#include "nva.h"

namespace hwtest {
namespace pgraph {

void MthdVtxXy::adjust_orig_mthd() {
	if (chipset.card_type < 3) {
		if (rnd() & 1)
			insrt(orig.access, 12, 5, 8 + rnd() % 4);
		if (rnd() & 1)
			orig.valid[0] |= 0x1ff1ff;
		if (rnd() & 1)
			orig.valid[0] |= 0x033033;
		if (rnd() & 1)
			orig.valid[0] &= ~0x11000000;
		if (rnd() & 1)
			orig.valid[0] &= ~0x00f00f;
	} else {
		if (rnd() & 1) {
			orig.valid[0] &= ~0xf0f;
			orig.valid[0] ^= 1 << (rnd() & 0xf);
			orig.valid[0] ^= 1 << (rnd() & 0xf);
		}
		if (draw) {
			if (rnd() & 3)
				insrt(orig.cliprect_ctrl, 8, 1, 0);
			if (rnd() & 3)
				insrt(orig.debug[2], 28, 1, 0);
			if (rnd() & 3) {
				insrt(orig.xy_misc_4[0], 4, 4, 0);
				insrt(orig.xy_misc_4[1], 4, 4, 0);
			}
			if (rnd() & 3) {
				orig.valid[0] = 0x0fffffff;
				if (rnd() & 1) {
					orig.valid[0] &= ~0xf0f;
				}
				orig.valid[0] ^= 1 << (rnd() & 0x1f);
				orig.valid[0] ^= 1 << (rnd() & 0x1f);
				if (chipset.card_type >= 4)
					orig.valid[0] &= 0xf07fffff;
			}
			if (chipset.card_type == 3 && rnd() & 1) {
				insrt(orig.ctx_switch[0], 24, 5, 0x17);
				insrt(orig.ctx_switch[0], 13, 2, 0);
				for (int j = 0; j < 8; j++) {
					insrt(orig.ctx_cache[j][0], 24, 5, 0x17);
					insrt(orig.ctx_cache[j][0], 13, 2, 0);
				}
			}
		}
	}
	if (rnd() & 1) {
		orig.xy_misc_4[0] &= ~0xf0;
		orig.xy_misc_4[1] &= ~0xf0;
	}
	if (rnd() & 1)
		orig.xy_misc_1[0] &= ~0x330;
	if (chipset.card_type >= 4 && draw) {
		insrt(orig.notify, 0, 1, 0);
	}
}

void MthdVtxXy::emulate_mthd() {
	if (first) {
		pgraph_clear_vtxid(&exp);
		if (chipset.card_type >= 3) {
			if (pgraph_is_class_line(&exp) || pgraph_is_class_tri(&exp)) {
				exp.valid[0] &= ~0xffff;
				insrt(exp.valid[0], 21, 1, 1);
			}
			if (cls == 0x18 && chipset.card_type == 3)
				insrt(exp.valid[0], 21, 1, 1);
		}
	}
	int vidx = pgraph_vtxid(&exp);
	if (chipset.card_type < 3) {
		if (pgraph_is_class_ifc_nv1(&exp))
			vidx = 4;
		bool eff = fract;
		if (nv01_pgraph_is_tex_class(&exp)) {
			vidx = (mthd - 0x10) >> 2 & 0xf;
			if (vidx >= 12)
				vidx -= 8;
			if (extr(exp.xy_misc_1[0], 24, 1) && extr(exp.xy_misc_1[0], 25, 1) != (uint32_t)fract) {
				exp.valid[0] &= ~0xffffff;
			}
		} else {
			eff = false;
		}
		insrt(exp.xy_misc_1[0], 0, 1, 0);
		insrt(exp.xy_misc_1[0], 24, 1, 1);
		insrt(exp.xy_misc_1[0], 25, 1, eff);
		pgraph_bump_vtxid(&exp);
		nv01_pgraph_set_vtx(&exp, 0, vidx, extrs(val, 0, 16), false);
		nv01_pgraph_set_vtx(&exp, 1, vidx, extrs(val, 16, 16), false);
		if (!poly) {
			if (vidx <= 8)
				exp.valid[0] |= 0x1001 << vidx;
			if (pgraph_is_class_line(&exp) || pgraph_is_class_tri(&exp)) {
				if (first) {
					exp.valid[0] &= ~0xffffff;
					exp.valid[0] |= 0x011111;
				} else {
					exp.valid[0] |= 0x10010 << (vidx & 3);
				}
			}
			if ((pgraph_is_class_rect(&exp) || pgraph_is_class_blit(&exp)) && first)
				exp.valid[0] |= 0x100;
		} else {
			if (pgraph_is_class_line(&exp) || pgraph_is_class_tri(&exp)) {
				exp.valid[0] |= 0x10010 << (vidx & 3);
			}
		}
	} else {
		int rvidx = ifc ? 4 : vidx;
		int svidx = vidx & 3;
		bool gdi_special = chipset.card_type == 3 && cls == 0xc && !first;
		if (gdi_special)
			vidx = rvidx = svidx = 1;
		pgraph_bump_vtxid(&exp);
		insrt(exp.xy_misc_1[0], 0, 1, 0);
		insrt(exp.xy_misc_1[1], 0, 1, 1);
		if (!gdi_special)
			insrt(exp.xy_misc_3, 8, 1, 0);
		if (poly && (exp.valid[0] & 0xf0f))
			insrt(exp.valid[0], 21, 1, 0);
		if (!poly) {
			insrt(exp.valid[0], rvidx, 1, 1);
			insrt(exp.valid[0], 8|rvidx, 1, 1);
		}
		if (pgraph_is_class_line(&exp) || pgraph_is_class_tri(&exp)) {
			insrt(exp.valid[0], 4|svidx, 1, 1);
			insrt(exp.valid[0], 0xc|svidx, 1, 1);
		}
		if (pgraph_is_class_blit(&exp) && chipset.card_type == 3) {
			insrt(exp.valid[0], vidx, 1, 1);
			insrt(exp.valid[0], vidx|8, 1, 1);
		}
		if (!gdi_special)
			insrt(exp.valid[0], 19, 1, noclip);
		if (noclip) {
			exp.vtx_xy[rvidx][0] = extrs(val, 16, 16);
			exp.vtx_xy[rvidx][1] = extrs(val, 0, 16);
		} else {
			exp.vtx_xy[rvidx][0] = extrs(val, 0, 16);
			exp.vtx_xy[rvidx][1] = extrs(val, 16, 16);
		}
		if (chipset.card_type < 4) {
			int xcstat = nv03_pgraph_clip_status(&exp, exp.vtx_xy[rvidx][0], 0, noclip);
			int ycstat = nv03_pgraph_clip_status(&exp, exp.vtx_xy[rvidx][1], 1, noclip);
			pgraph_set_xy_d(&exp, 0, vidx, vidx, false, false, false, xcstat);
			pgraph_set_xy_d(&exp, 1, vidx, vidx, false, false, false, ycstat);
		} else {
			int xcstat = nv04_pgraph_clip_status(&exp, exp.vtx_xy[rvidx][0], 0);
			int ycstat = nv04_pgraph_clip_status(&exp, exp.vtx_xy[rvidx][1], 1);
			pgraph_set_xy_d(&exp, 0, vidx, vidx, false, false, false, xcstat);
			pgraph_set_xy_d(&exp, 1, vidx, vidx, false, false, false, ycstat);
		}
	}
	if (draw) {
		if (chipset.card_type < 4) {
			pgraph_prep_draw(&exp, poly, false);
			// XXX
			if (pgraph_is_class_ifc_nv1(&exp))
				skip = true;
		} else {
			// XXX
			skip = true;
		}
	}
}

bool MthdVtxXy::other_fail() {
	int rcls = pgraph_class(&exp);
	if (real.status && chipset.card_type < 4 && (rcls == 9 || rcls == 0xa || rcls == 0x0b || rcls == 0x0c || rcls == 0x10)) {
		/* Hung PGRAPH... */
		skip = true;
	}
	return MthdTest::other_fail();
}

void MthdVtxX32::adjust_orig_mthd() {
	if (chipset.card_type < 3) {
		if (rnd() & 1) {
			insrt(orig.access, 12, 5, 8 + rnd() % 4);
		}
		if (rnd() & 1)
			orig.valid[0] |= 0x1ff1ff;
		if (rnd() & 1)
			orig.valid[0] |= 0x033033;
		if (rnd() & 1) {
			orig.valid[0] &= ~0x11000000;
		}
		if (rnd() & 1) {
			orig.valid[0] &= ~0x00f00f;
		}
	} else {
		if (rnd() & 1) {
			orig.valid[0] &= ~(rnd() & 0x00000f0f);
			orig.valid[0] &= ~(rnd() & 0x00000f0f);
		}
	}
	if (rnd() & 1) {
		orig.xy_misc_4[0] &= ~0xf0;
		orig.xy_misc_4[1] &= ~0xf0;
	}
	if (rnd() & 1) {
		orig.xy_misc_1[0] &= ~0x330;
	}
	if (rnd() & 1) {
		val = extrs(val, 0, 17);
	}
}

void MthdVtxX32::emulate_mthd() {
	if (first) {
		pgraph_clear_vtxid(&exp);
		if (chipset.card_type >= 3) {
			if (pgraph_is_class_line(&exp) || pgraph_is_class_tri(&exp)) {
				exp.valid[0] &= ~0xffff;
				insrt(exp.valid[0], 21, 1, 1);
			}
			if (cls == 0x18 && chipset.card_type == 3)
				insrt(exp.valid[0], 21, 1, 1);
		}
	}
	int vidx = pgraph_vtxid(&exp);
	if (nv01_pgraph_is_tex_class(&exp)) {
		if (extr(exp.xy_misc_1[0], 24, 1) && extr(exp.xy_misc_1[0], 25, 1)) {
			exp.valid[0] &= ~0xffffff;
		}
	}
	if (chipset.card_type < 3) {
		insrt(exp.xy_misc_1[0], 0, 1, 0);
		insrt(exp.xy_misc_1[0], 24, 1, 1);
		insrt(exp.xy_misc_1[0], 25, 1, 0);
		nv01_pgraph_set_vtx(&exp, 0, vidx, val, true);
		if (!poly) {
			if (vidx <= 8)
				exp.valid[0] |= 1 << vidx;
			if (pgraph_is_class_line(&exp) || pgraph_is_class_tri(&exp)) {
				if (first) {
					exp.valid[0] &= ~0xffffff;
					exp.valid[0] |= 0x000111;
				} else {
					exp.valid[0] |= 0x10 << (vidx & 3);
				}
			}
			if ((pgraph_is_class_rect(&exp) || pgraph_is_class_blit(&exp)) && first)
				exp.valid[0] |= 0x100;
		} else {
			if (pgraph_is_class_line(&exp) || pgraph_is_class_tri(&exp)) {
				if (exp.valid[0] & 0xf00f)
					exp.valid[0] &= ~0x100;
				exp.valid[0] |= 0x10 << (vidx & 3);
			}
		}
	} else {
		int svidx = vidx & 3;
		insrt(exp.xy_misc_1[0], 0, 1, 0);
		insrt(exp.xy_misc_1[1], 0, 1, 1);
		insrt(exp.xy_misc_3, 8, 1, 0);
		if (poly && (exp.valid[0] & 0xf0f))
			insrt(exp.valid[0], 21, 1, 0);
		if (!poly)
			insrt(exp.valid[0], vidx, 1, 1);
		if (pgraph_is_class_line(&exp) || pgraph_is_class_tri(&exp))
			insrt(exp.valid[0], 4|svidx, 1, 1);
		insrt(exp.valid[0], 19, 1, false);
		exp.vtx_xy[vidx][0] = val;
		if (chipset.card_type < 4) {
			int xcstat = nv03_pgraph_clip_status(&exp, exp.vtx_xy[vidx][0], 0, false);
			pgraph_set_xy_d(&exp, 0, vidx, vidx, 0, (int32_t)val != sext(val, 15), false, xcstat);
		} else {
			int xcstat = nv04_pgraph_clip_status(&exp, exp.vtx_xy[vidx][0], 0);
			pgraph_set_xy_d(&exp, 0, vidx, vidx, 0, (int32_t)val != sext(val, 15), false, xcstat);
		}
	}
}

void MthdVtxY32::adjust_orig_mthd() {
	if (chipset.card_type < 3) {
		if (rnd() & 1) {
			insrt(orig.access, 12, 5, 8 + rnd() % 4);
		}
		if (rnd() & 1)
			orig.valid[0] |= 0x1ff1ff;
		if (rnd() & 1)
			orig.valid[0] |= 0x033033;
		if (rnd() & 1)
			orig.valid[0] &= ~0x11000000;
		if (rnd() & 1)
			orig.valid[0] &= ~0x00f00f;
	} else {
		if (draw) {
			if (rnd() & 3)
				insrt(orig.cliprect_ctrl, 8, 1, 0);
			if (rnd() & 3)
				insrt(orig.debug[2], 28, 1, 0);
			if (rnd() & 3) {
				insrt(orig.xy_misc_4[0], 4, 4, 0);
				insrt(orig.xy_misc_4[1], 4, 4, 0);
			}
			if (rnd() & 3) {
				orig.valid[0] = 0x0fffffff;
				if (rnd() & 1) {
					orig.valid[0] &= ~0xf0f;
				}
				orig.valid[0] ^= 1 << (rnd() & 0x1f);
				orig.valid[0] ^= 1 << (rnd() & 0x1f);
			}
			if (chipset.card_type == 3 && rnd() & 1) {
				insrt(orig.ctx_switch[0], 24, 5, 0x17);
				insrt(orig.ctx_switch[0], 13, 2, 0);
				for (int j = 0; j < 8; j++) {
					insrt(orig.ctx_cache[j][0], 24, 5, 0x17);
					insrt(orig.ctx_cache[j][0], 13, 2, 0);
				}
			}
			if (chipset.card_type >= 4)
				orig.valid[0] &= 0xf07fffff;
		}
	}
	if (rnd() & 1) {
		orig.xy_misc_4[0] &= ~0xf0;
		orig.xy_misc_4[1] &= ~0xf0;
	}
	if (rnd() & 1) {
		orig.xy_misc_1[0] &= ~0x330;
	}
	if (rnd() & 1) {
		val = extrs(val, 0, 17);
	}
	if (chipset.card_type >= 4 && draw) {
		insrt(orig.notify, 0, 1, 0);
	}
}

void MthdVtxY32::emulate_mthd() {
	int vidx = pgraph_vtxid(&exp);
	if (chipset.card_type < 3) {
		if (nv01_pgraph_is_tex_class(&exp)) {
			if (extr(exp.xy_misc_1[0], 24, 1) && extr(exp.xy_misc_1[0], 25, 1)) {
				exp.valid[0] &= ~0xffffff;
			}
		}
		pgraph_bump_vtxid(&exp);
		insrt(exp.xy_misc_1[0], 0, 1, 0);
		insrt(exp.xy_misc_1[0], 24, 1, 1);
		insrt(exp.xy_misc_1[0], 25, 1, 0);
		nv01_pgraph_set_vtx(&exp, 1, vidx, val, true);
		if (!poly) {
			if (vidx <= 8)
				exp.valid[0] |= 0x1000 << vidx;
		}
		if (pgraph_is_class_line(&exp) || pgraph_is_class_tri(&exp)) {
			exp.valid[0] |= 0x10000 << (vidx & 3);
		}
	} else {
		int svidx = vidx & 3;
		pgraph_bump_vtxid(&exp);
		insrt(exp.xy_misc_1[0], 0, 1, 0);
		insrt(exp.xy_misc_1[1], 0, 1, 1);
		insrt(exp.xy_misc_3, 8, 1, 0);
		if (poly && (exp.valid[0] & 0xf0f))
			insrt(exp.valid[0], 21, 1, 0);
		if (!poly)
			insrt(exp.valid[0], vidx|8, 1, 1);
		if (pgraph_is_class_line(&exp) || pgraph_is_class_tri(&exp))
			insrt(exp.valid[0], 0xc|svidx, 1, 1);
		insrt(exp.valid[0], 19, 1, false);
		exp.vtx_xy[vidx][1] = val;
		if (chipset.card_type < 4) {
			int ycstat = nv03_pgraph_clip_status(&exp, exp.vtx_xy[vidx][1], 1, false);
			pgraph_set_xy_d(&exp, 1, vidx, vidx, 0, (int32_t)val != sext(val, 15), false, ycstat);
		} else {
			int ycstat = nv04_pgraph_clip_status(&exp, exp.vtx_xy[vidx][1], 1);
			pgraph_set_xy_d(&exp, 1, vidx, vidx, 0, (int32_t)val != sext(val, 15), false, ycstat);
		}
	}
	if (draw) {
		if (chipset.card_type < 4) {
			pgraph_prep_draw(&exp, poly, false);
			// XXX
			if (pgraph_is_class_ifc_nv1(&exp))
				skip = true;
		} else {
			// XXX
			skip = true;
		}
	}
}

bool MthdVtxY32::other_fail() {
	int rcls = pgraph_class(&exp);
	if (draw && real.status && chipset.card_type < 4 && (rcls == 9 || rcls == 0xa || rcls == 0x0b || rcls == 0x0c || rcls == 0x10)) {
		/* Hung PGRAPH... */
		skip = true;
	}
	return MthdTest::other_fail();
}

void MthdIfcSize::emulate_mthd() {
	if (chipset.card_type < 3) {
		if (is_out) {
			exp.vtx_xy[5][0] = extr(val, 0, 16);
			exp.vtx_xy[5][1] = extr(val, 16, 16);
			if (pgraph_is_class_line(&exp) || pgraph_is_class_tri(&exp))
				exp.valid[0] &= ~0xffffff;
			exp.valid[0] |= 0x020020;
			if (pgraph_is_class_ifc_nv1(&exp))
				insrt(exp.xy_misc_1[0], 0, 1, 0);
			if (pgraph_is_class_line(&exp) || pgraph_is_class_tri(&exp) || pgraph_is_class_rect(&exp) || pgraph_is_class_blit(&exp))
				exp.valid[0] |= 0x100;
		}
		if (is_in) {
			int rcls = pgraph_class(&exp);
			exp.vtx_xy[1][1] = 0;
			exp.vtx_xy[3][0] = extr(val, 0, 16);
			exp.vtx_xy[3][1] = -extr(val, 16, 16);
			if (pgraph_is_class_ifc_nv1(&exp))
				insrt(exp.xy_misc_1[0], 0, 1, 0);
			if (!is_ifm) {
				if (pgraph_is_class_line(&exp) || pgraph_is_class_tri(&exp))
					exp.valid[0] &= ~0xffffff;
				if (pgraph_is_class_line(&exp) || pgraph_is_class_tri(&exp) || pgraph_is_class_rect(&exp) || pgraph_is_class_blit(&exp))
					exp.valid[0] |= 0x100;
			}
			exp.valid[0] |= 0x008008;
			if (pgraph_is_class_line(&exp) || pgraph_is_class_tri(&exp))
				exp.valid[0] |= 0x080080;
			exp.edgefill &= ~0x110;
			if (exp.vtx_xy[3][0] < 0x20 && rcls == 0x12)
				exp.edgefill |= 0x100;
			if (rcls != 0x0d && rcls != 0x1d) {
				insrt(exp.xy_misc_4[0], 28, 2, 0);
				insrt(exp.xy_misc_4[1], 28, 2, 0);
			}
			if (exp.vtx_xy[3][0])
				exp.xy_misc_4[0] |= 2 << 28;
			if (exp.vtx_xy[3][1])
				exp.xy_misc_4[1] |= 2 << 28;
			bool zero;
			if (pgraph_is_class_itm(&exp)) {
				uint32_t steps = 4 / pgraph_cpp_in(&exp);
				zero = (exp.vtx_xy[3][0] == steps || !exp.vtx_xy[3][1]);
			} else {
				zero = extr(exp.xy_misc_4[0], 28, 2) == 0 ||
					 extr(exp.xy_misc_4[1], 28, 2) == 0;
			}
			pgraph_set_image_zero(&exp, zero);
		}
		pgraph_clear_vtxid(&exp);
	} else {
		if (is_out) {
			exp.valid[0] |= 0x2020;
			exp.vtx_xy[5][0] = extr(val, 0, 16);
			exp.vtx_xy[5][1] = extr(val, 16, 16);
			if (chipset.card_type >= 4) {
				pgraph_vtx_cmp(&exp, 0, 5, false);
				pgraph_vtx_cmp(&exp, 1, 5, false);
			}
		}
		if (is_in) {
			exp.valid[0] |= 0x8008;
			exp.vtx_xy[3][0] = extr(val, 0, 16);
			exp.vtx_xy[3][1] = -extr(val, 16, 16);
			exp.vtx_xy[7][1] = extr(val, 16, 16);
			if (!is_out)
				exp.misc24[0] = extr(val, 0, 16);
			pgraph_vtx_cmp(&exp, 0, 3, false);
			pgraph_vtx_cmp(&exp, 1, (pgraph_is_class_sifc(&exp) && chipset.card_type < 4) ? 3 : 7, false);
			bool zero = false;
			if (!extr(exp.xy_misc_4[0], 28, 4))
				zero = true;
			if (!extr(exp.xy_misc_4[1], 28, 4))
				zero = true;
			pgraph_set_image_zero(&exp, zero);
			if (!pgraph_is_class_sifc(&exp)) {
				insrt(exp.xy_misc_3, 12, 1, extr(val, 0, 16) < 0x20 && is_bitmap && !extr(exp.xy_a, 24, 1));
			}
		}
			if (cls != 0x8a || !extr(exp.debug[3], 16, 1))
				insrt(exp.xy_misc_1[0], 0, 1, 0);
		insrt(exp.xy_misc_1[1], 0, 1, !pgraph_is_class_sifc(&exp));
		insrt(exp.xy_misc_3, 8, 1, 0);
	}
	pgraph_clear_vtxid(&exp);
}

void MthdRect::adjust_orig_mthd() {
	if (chipset.card_type < 3) {
		if (rnd() & 1)
			orig.valid[0] |= 0x1ff1ff;
		if (rnd() & 1)
			orig.valid[0] |= 0x033033;
		if (rnd() & 1) {
			orig.xy_misc_4[0] &= ~0xf0;
			orig.xy_misc_4[1] &= ~0xf0;
		}
		if (rnd() & 1) {
			orig.valid[0] &= ~0x11000000;
			orig.xy_misc_1[0] &= ~0x330;
		}
		if (rnd() & 1) {
			orig.valid[0] &= ~0x00f00f;
		}
	} else {
		if (rnd() & 3) {
			insrt(orig.xy_misc_4[0], 4, 4, 0);
			insrt(orig.xy_misc_4[1], 4, 4, 0);
		}
		if (rnd() & 3) {
			orig.valid[0] |= 0x007fffff;
			if (rnd() & 1) {
				orig.valid[0] &= ~0xf0f;
			}
			orig.valid[0] ^= 1 << (rnd() & 0xf);
			orig.valid[0] ^= 1 << (rnd() & 0xf);
		}
		if (chipset.card_type < 4) {
			if (rnd() & 1) {
				insrt(orig.ctx_user, 16, 5, 0x17);
			}
			if (rnd() & 3)
				insrt(orig.cliprect_ctrl, 8, 1, 0);
			if (rnd() & 3)
				insrt(orig.debug[2], 28, 1, 0);
			if (rnd() & 1) {
				insrt(orig.ctx_switch[0], 24, 5, 0x17);
				insrt(orig.ctx_switch[0], 13, 2, 0);
				int j;
				for (j = 0; j < 8; j++) {
					insrt(orig.ctx_cache[j][0], 24, 5, 0x17);
					insrt(orig.ctx_cache[j][0], 13, 2, 0);
				}
			}
		} else {
			insrt(orig.notify, 0, 1, 0);
		}
	}
	if (!(rnd() & 3)) {
		val &= ~0xffff;
	}
	if (!(rnd() & 3)) {
		val &= ~0xffff0000;
	}
	if (rnd() & 1) {
		val ^= 1 << (rnd() & 0x1f);
	}
	if (rnd() & 1) {
		orig.vtx_xy[2][0] &= 0x80000003;
	}
}

void MthdRect::emulate_mthd() {
	if (pgraph_is_class_itm(&exp)) {
		pgraph_bump_vtxid(&exp);
		exp.vtx_xy[3][0] = extr(val, 0, 16);
		exp.vtx_xy[3][1] = extr(val, 16, 16);
		if (chipset.card_type < 3) {
			nv01_pgraph_vtx_fixup(&exp, 0, 2, exp.vtx_xy[3][0], 1, 0, 2);
			nv01_pgraph_vtx_fixup(&exp, 1, 2, exp.vtx_xy[3][1], 1, 0, 2);
			exp.valid[0] |= 0x4004;
		} else {
			pgraph_vtx_cmp(&exp, 0, 3, false);
			pgraph_vtx_cmp(&exp, 1, 3, false);
			nv03_pgraph_vtx_add(&exp, 0, 2, exp.vtx_xy[0][0], exp.vtx_xy[3][0], 0, false, false);
			nv03_pgraph_vtx_add(&exp, 1, 2, exp.vtx_xy[0][1], exp.vtx_xy[3][1], 0, false, false);
			exp.misc24[0] = extr(val, 0, 16);
			exp.valid[0] |= 0x404;
		}
		pgraph_set_image_zero(&exp, !exp.vtx_xy[3][0] || !exp.vtx_xy[3][1]);
	} else if (pgraph_is_class_blit(&exp)) {
		pgraph_bump_vtxid(&exp);
		pgraph_bump_vtxid(&exp);
		if (chipset.card_type < 3) {
			nv01_pgraph_vtx_fixup(&exp, 0, 2, extr(val, 0, 16), 1, 0, 2);
			nv01_pgraph_vtx_fixup(&exp, 1, 2, extr(val, 16, 16), 1, 0, 2);
			nv01_pgraph_vtx_fixup(&exp, 0, 3, extr(val, 0, 16), 1, 1, 3);
			nv01_pgraph_vtx_fixup(&exp, 1, 3, extr(val, 16, 16), 1, 1, 3);
			exp.valid[0] |= 0x00c00c;
		} else {
			insrt(exp.xy_misc_1[1], 0, 1, 1);
			nv03_pgraph_vtx_add(&exp, 0, 2, exp.vtx_xy[0][0], extr(val, 0, 16), 0, false, false);
			nv03_pgraph_vtx_add(&exp, 1, 2, exp.vtx_xy[0][1], extr(val, 16, 16), 0, false, false);
			nv03_pgraph_vtx_add(&exp, 0, 3, exp.vtx_xy[1][0], extr(val, 0, 16), 0, false, false);
			nv03_pgraph_vtx_add(&exp, 1, 3, exp.vtx_xy[1][1], extr(val, 16, 16), 0, false, false);
			pgraph_vtx_cmp(&exp, 0, 8, true);
			pgraph_vtx_cmp(&exp, 1, 8, true);
			exp.valid[0] |= 0xc0c;
		}
	} else if (pgraph_is_class_rect(&exp) || pgraph_is_class_ifc_nv1(&exp)) {
		if (chipset.card_type < 3) {
			int vidx = pgraph_vtxid(&exp);
			nv01_pgraph_vtx_fixup(&exp, 0, vidx, extr(val, 0, 16), 1, 0, vidx & 3);
			nv01_pgraph_vtx_fixup(&exp, 1, vidx, extr(val, 16, 16), 1, 0, vidx & 3);
			if (vidx <= 8)
				exp.valid[0] |= 0x1001 << vidx;
		} else {
			if (noclip) {
				nv03_pgraph_vtx_add(&exp, 0, 1, exp.vtx_xy[0][0], extr(val, 16, 16), 0, true, false);
				nv03_pgraph_vtx_add(&exp, 1, 1, exp.vtx_xy[0][1], extr(val, 0, 16), 0, true, false);
			} else {
				nv03_pgraph_vtx_add(&exp, 0, 1, exp.vtx_xy[0][0], extr(val, 0, 16), 0, false, false);
				nv03_pgraph_vtx_add(&exp, 1, 1, exp.vtx_xy[0][1], extr(val, 16, 16), 0, false, false);
			}
			if (noclip) {
				int steps = 0x20;
				if (extr(exp.xy_misc_3, 12, 1)) {
					steps = 4 / pgraph_cpp_in(&orig);
				}
				exp.vtx_xy[2][0] -= steps;
				pgraph_vtx_cmp(&exp, 0, 2, false);
				exp.vtx_xy[2][0] += steps;
				pgraph_vtx_cmp(&exp, 1, 2, false);
			} else {
				pgraph_vtx_cmp(&exp, 0, 8, true);
				pgraph_vtx_cmp(&exp, 1, 8, true);
			}
			exp.valid[0] |= 0x202;
			insrt(exp.xy_misc_1[1], 0, 1, 1);
		}
		pgraph_bump_vtxid(&exp);
	} else {
		nv01_pgraph_vtx_fixup(&exp, 0, 15, extr(val, 0, 16), 1, 15, 1);
		nv01_pgraph_vtx_fixup(&exp, 1, 15, extr(val, 16, 16), 1, 15, 1);
		pgraph_bump_vtxid(&exp);
		if (pgraph_is_class_line(&exp) || pgraph_is_class_tri(&exp)) {
			exp.valid[0] |= 0x080080;
		}
	}
	if (draw) {
		if (chipset.card_type < 4) {
			pgraph_prep_draw(&exp, false, noclip);
			// XXX
			if (pgraph_is_class_ifc_nv1(&exp))
				skip = true;
		} else {
			// XXX
			skip = true;
		}
	}
}

bool MthdRect::other_fail() {
	if (chipset.card_type < 4) {
		int rcls = pgraph_class(&exp);
		if (draw && real.status && (rcls == 7 || rcls == 0x0b || rcls == 0x0c || rcls == 0x10)) {
			/* Hung PGRAPH... */
			skip = true;
		}
	}
	return MthdTest::other_fail();
}

namespace {

class MthdIfcDataTest : public MthdTest {
	bool is_bitmap;
	int repeats() override { return 100000; };
	bool supported() override { return chipset.card_type < 3; } // XXX
	void adjust_orig_mthd() override {
		if (rnd() & 3)
			orig.valid[0] = 0x1ff1ff;
		if (rnd() & 3) {
			orig.xy_misc_4[0] &= ~0xf0;
			orig.xy_misc_4[1] &= ~0xf0;
		}
		if (rnd() & 3) {
			orig.valid[0] &= ~0x11000000;
			orig.xy_misc_1[0] &= ~0x330;
		}
		if (rnd() & 3)
			insrt(orig.access, 12, 5, 0x11 + (rnd() & 1));
	}
	void choose_mthd() override {
		switch (rnd() % 3) {
			case 0:
				cls = 0x11;
				mthd = 0x400 | (rnd() & 0x7c);
				is_bitmap = false;
				break;
			case 1:
				cls = 0x12;
				mthd = 0x400 | (rnd() & 0x7c);
				is_bitmap = true;
				break;
			case 2:
				cls = 0x13;
				mthd = 0x040 | (rnd() & 0x3c);
				is_bitmap = false;
				break;
			default:
				abort();
		}
	}
	void emulate_mthd() override {
		int rcls = pgraph_class(&exp);
		exp.misc32[0] = is_bitmap ? pgraph_expand_mono(&exp, val) : val;
		insrt(exp.xy_misc_1[0], 24, 1, 0);
		int steps = 4 / pgraph_cpp_in(&exp);
		if (rcls == 0x12)
			steps = 0x20;
		if (rcls != 0x11 && rcls != 0x12)
			goto done;
		if (exp.valid[0] & 0x11000000 && exp.ctx_switch[0] & 0x80)
			exp.intr |= 1 << 16;
		if (extr(exp.canvas_config, 24, 1))
			exp.intr |= 1 << 20;
		if (extr(exp.cliprect_ctrl, 8, 1))
			exp.intr |= 1 << 24;
		int iter;
		iter = 0;
		if (pgraph_image_zero(&exp)) {
			if ((exp.valid[0] & 0x38038) != 0x38038)
				exp.intr |= 1 << 16;
			if ((exp.xy_misc_4[0] & 0xf0) || (exp.xy_misc_4[1] & 0xf0))
				exp.intr |= 1 << 12;
			goto done;
		}
		int vidx;
		if (!(exp.xy_misc_1[0] & 1)) {
			for (int j = 0; j < 2; j++)
				exp.vtx_xy[6][j] = exp.vtx_xy[4][j] + exp.vtx_xy[5][j];
			insrt(exp.xy_misc_1[0], 14, 1, 0);
			insrt(exp.xy_misc_1[0], 18, 1, 0);
			insrt(exp.xy_misc_1[0], 20, 1, 0);
			if ((exp.valid[0] & 0x38038) != 0x38038) {
				exp.intr |= 1 << 16;
				if ((exp.xy_misc_4[0] & 0xf0) || (exp.xy_misc_4[1] & 0xf0))
					exp.intr |= 1 << 12;
				goto done;
			}
			nv01_pgraph_iclip_fixup(&exp, 0, exp.vtx_xy[6][0], 0);
			nv01_pgraph_iclip_fixup(&exp, 1, exp.vtx_xy[6][1], 0);
			insrt(exp.xy_misc_1[0], 0, 1, 1);
			if (extr(exp.edgefill, 8, 1)) {
				/* XXX */
				skip = true;
				return;
			}
			pgraph_clear_vtxid(&exp);
			vidx = 1;
			exp.vtx_xy[2][1] = exp.vtx_xy[3][1] + 1;
			pgraph_vtx_cmp(&exp, 1, 2, false);
			nv01_pgraph_vtx_fixup(&exp, 1, 0, 0, 1, 4, 0);
			nv01_pgraph_vtx_fixup(&exp, 0, 0, 0, 1, 4, 0);
			exp.vtx_xy[2][0] = exp.vtx_xy[3][0];
			exp.vtx_xy[2][0] -= steps;
			pgraph_vtx_cmp(&exp, 0, 2, false);
			nv01_pgraph_vtx_add(&exp, 0, vidx, vidx, exp.vtx_xy[vidx ^ 1][0], steps, 0);
			if (extr(exp.xy_misc_4[0], 28, 1)) {
				nv01_pgraph_vtx_add(&exp, 0, vidx, vidx, exp.vtx_xy[2][0], exp.vtx_xy[vidx][0], 0);
			}
			if ((exp.xy_misc_4[0] & 0xc0) || (exp.xy_misc_4[1] & 0xf0))
				exp.intr |= 1 << 12;
			if ((exp.xy_misc_4[0] & 0x30) == 0x30)
				exp.intr |= 1 << 12;
		} else {
			if ((exp.valid[0] & 0x38038) != 0x38038)
				exp.intr |= 1 << 16;
			if ((exp.xy_misc_4[0] & 0xf0) || (exp.xy_misc_4[1] & 0xf0))
				exp.intr |= 1 << 12;
		}
restart:;
		vidx = pgraph_vtxid(&exp) & 1;
		if (extr(exp.edgefill, 8, 1)) {
			/* XXX */
			skip = true;
			return;
		}
		if (!exp.intr) {
			if (extr(exp.xy_misc_4[0], 29, 1)) {
				pgraph_bump_vtxid(&exp);
			} else {
				pgraph_clear_vtxid(&exp);
				vidx = 1;
				bool check_y = false;
				if (extr(exp.xy_misc_4[1], 28, 1)) {
					exp.vtx_xy[2][1]++;
					nv01_pgraph_vtx_add(&exp, 1, 0, 0, exp.vtx_xy[0][1], exp.vtx_xy[1][1], 1);
					check_y = true;
				} else {
					exp.vtx_xy[4][0] += exp.vtx_xy[3][0];
					exp.vtx_xy[2][1] = exp.vtx_xy[3][1] + 1;
					nv01_pgraph_vtx_fixup(&exp, 1, 0, 0, 1, 4, 0);
				}
				pgraph_vtx_cmp(&exp, 1, 2, false);
				nv01_pgraph_vtx_fixup(&exp, 0, 0, 0, 1, 4, 0);
				if (extr(exp.xy_misc_4[0], 28, 1)) {
					nv01_pgraph_vtx_add(&exp, 0, vidx, vidx, exp.vtx_xy[vidx ^ 1][0], ~exp.vtx_xy[2][0], 1);
					exp.vtx_xy[2][0] += exp.vtx_xy[3][0];
					pgraph_vtx_cmp(&exp, 0, 2, false);
					if (extr(exp.xy_misc_4[0], 28, 1)) {
						nv01_pgraph_vtx_add(&exp, 0, vidx, vidx, exp.vtx_xy[2][0], exp.vtx_xy[vidx][0], 0);
						if (exp.xy_misc_4[0] & 0x10)
							exp.intr |= 1 << 12;
						check_y = true;
					} else {
						if ((exp.xy_misc_4[0] & 0x20))
							exp.intr |= 1 << 12;
					}
					if (exp.xy_misc_4[1] & 0x10 && check_y)
						exp.intr |= 1 << 12;
					iter++;
					if (iter > 10000) {
						/* This is a hang - skip this test run.  */
						skip = true;
						return;
					}
					goto restart;
				}
				exp.vtx_xy[2][0] = exp.vtx_xy[3][0];
			}
			exp.vtx_xy[2][0] -= steps;
			pgraph_vtx_cmp(&exp, 0, 2, false);
			nv01_pgraph_vtx_add(&exp, 0, vidx, vidx, exp.vtx_xy[vidx ^ 1][0], steps, 0);
			if (extr(exp.xy_misc_4[0], 28, 1)) {
				nv01_pgraph_vtx_add(&exp, 0, vidx, vidx, exp.vtx_xy[2][0], exp.vtx_xy[vidx][0], 0);
			}
		} else {
			pgraph_bump_vtxid(&exp);
			if (extr(exp.xy_misc_4[0], 29, 1)) {
				exp.vtx_xy[2][0] -= steps;
				pgraph_vtx_cmp(&exp, 0, 2, false);
			} else if (extr(exp.xy_misc_4[1], 28, 1)) {
				exp.vtx_xy[2][1]++;
			}
		}
done:
		if (exp.intr)
			exp.access &= ~0x101;
	}
public:
	MthdIfcDataTest(hwtest::TestOptions &opt, uint32_t seed) : MthdTest(opt, seed) {}
};

}

bool PGraphMthdXyTests::supported() {
	return chipset.card_type < 4;
}

Test::Subtests PGraphMthdXyTests::subtests() {
	return {
		{"ifc_data", new MthdIfcDataTest(opt, rnd())},
	};
}

}
}
