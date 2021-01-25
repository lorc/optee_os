/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2015-2018, Renesas Electronics Corporation
 * Copyright (c) 2021, EPAM Systems
 */

#ifndef __RCAR_H_
#define __RCAR_H_

/* Product register (PRR) */
#define RCAR_PRODUCT_REGISTER   0xFFF00044

#define PRR_PRODUCT_MASK	0x0000FF00
#define PRR_PRODUCT_API_TABLE	0x00010000
#define PRR_PRODUCT_H3		0x00004F00	/* R-Car H3 */
#define PRR_PRODUCT_M3		0x00005200	/* R-Car M3 */
#define PRR_PRODUCT_V3M		0x00005400	/* R-Car V3M */
#define PRR_PRODUCT_M3N		0x00005500	/* R-Car M3N */
#define PRR_PRODUCT_V3H		0x00005600	/* R-Car V3H */
#define PRR_PRODUCT_E3		0x00005700	/* R-Car E3 */
#define PRR_PRODUCT_D3		0x00005800	/* R-Car D3 */
#define PRR_CUT_MASK		0x000000FF
#define PRR_CUT_10		0x00000000	/* Ver 1.0 */
#define PRR_CUT_11		0x00000001	/* Ver 1.1 */
#define PRR_CUT_20		0x00000010	/* Ver 2.0 */
#define PRR_CUT_30		0x00000020	/* Ver.3.0 */
#define PRR_CUT_M3_11_OR_12	0x00000010	/* M3 Ver.1.1 or Ver.1.2 */
#define PRR_CUT_M3_13		0x00000011	/* M3 Ver.1.3 */

/* Fuse Monitor Register (holds M3 revisions) */
#define RCAR_FUSE_DUMMY5	0xE60603E8	/* Fuse dummy5 */
#define	FUSE_M3_MASK		0x1C000000	/* Dummy5[28:26] */
#define	DUMMY5_M3_100		0x00000000	/* M3 1.0  */
#define	DUMMY5_M3_105		0x04000000	/* M3 1.05 */
#define	DUMMY5_M3_106		0x08000000	/* M3 1.06 */

enum rcar_product_type {
	RCAR_SOC_UNKNOWN = 0,

	/* H3 revisions */
	RCAR_H3_ES1_0,
	RCAR_H3_ES1_1,
	RCAR_H3_ES2_0,
	RCAR_H3_ES3_0,

	/* M3 revisions */
	RCAR_M3_ES1_00,
	RCAR_M3_ES1_05,
	RCAR_M3_ES1_06,
	RCAR_M3_ES1_1,

	/* V3M revisions */
	RCAR_V3M_ES_1_0,
	RCAR_V3M_ES_2_0,

	/* Other series */
	RCAR_M3N,
	RCAR_V3H,
	RCAR_E3,
	RCAR_D3,

	RCAR_SOC_MAX,
};

/* Will either return product type or panic if product can't be determined */
enum rcar_product_type rcar_get_product_type(void);

static inline const char *rcar_product_str(enum rcar_product_type type)
{
	static const char * const names[] = {
		[RCAR_SOC_UNKNOWN] = "?",
		[RCAR_H3_ES1_0] = "H3 ES 1.0",
		[RCAR_H3_ES1_1] = "H3 ES 1.1",
		[RCAR_H3_ES2_0] = "H3 ES 2.0",
		[RCAR_H3_ES3_0] = "H3 ES 3.0",
		[RCAR_M3_ES1_00] = "M3 ES 1.0",
		[RCAR_M3_ES1_05] = "M3 ES 1.05",
		[RCAR_M3_ES1_06] = "M3 ES 1.06",
		[RCAR_M3_ES1_1] = "M3 ES 1.1",
		[RCAR_V3M_ES_1_0] = "V3M ES 1.0",
		[RCAR_V3M_ES_2_0] = "V3M ES 2.0",
		[RCAR_M3N] = "M3N",
		[RCAR_V3H] = "V3H",
		[RCAR_E3] = "E3",
		[RCAR_D3] = "D3",
	};

	COMPILE_TIME_ASSERT(ARRAY_SIZE(names) == RCAR_SOC_MAX);
	return names[type];
}

#endif
