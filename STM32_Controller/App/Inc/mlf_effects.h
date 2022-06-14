// SPDX-License-Identifier: GPL-2.0-only
/*
 * mlf_effects.h -
 *  (C) 2021 Pawel Wieczorek
 */
#ifndef INC_MLF_EFFECTS_H_
#define INC_MLF_EFFECTS_H_

#include <stdint.h>
#include "ws2812.h"

typedef int (*MLF_effects_handlers)(struct LEDStrip*, uint32_t frame, uint32_t data);

enum MLF_EFFECTS {
	EFFECT_RAINBOW 		= 0,
	EFFECT_COLOR_CYCLE,
	EFFECT_STATIC_COLOR,

	EFFECT_MAX,
};

/**
 * Display selected effect on LED panel
 * @param effect - one of the defined MLF_EFFECTS
 * @param frame - number increased after each function invocation
 */
int run_effect_frame(struct LEDStrip* strip, enum MLF_EFFECTS effect, uint32_t frame, uint32_t data);

#endif /* INC_MLF_EFFECTS_H_ */
