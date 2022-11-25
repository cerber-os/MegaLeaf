// SPDX-License-Identifier: GPL-2.0-only
/*
 * mlf_effects.h - implementation of awesome effect that can be displayed on panel
 *  (C) 2022 Pawel Wieczorek
 */
#ifndef INC_MLF_EFFECTS_H_
#define INC_MLF_EFFECTS_H_

#include <stdint.h>
#include "ws2812.h"

#define MLF_EFFECT_REFRESH_REQ		1

typedef int (*MLF_effects_handlers)(struct LEDStrip*, uint32_t frame, uint32_t data);

enum MLF_EFFECTS {
	EFFECT_RAINBOW 		= 0,
	EFFECT_COLOR_CYCLE,
	EFFECT_STATIC_COLOR,
	EFFECT_BAR_CYCLE,

	EFFECT_MAX,
};

/**
 * Display selected effect on LED panel
 *
 * @param strip
 * @param effect ID of effect to display
 * @param frame increasing integer to keep track of current frame
 * @param data optional, effect specific data
 * @return 1 if panel has to be refreshed, 0 otherwise
 */
int run_effect_frame(struct LEDStrip* strip, enum MLF_EFFECTS effect, uint32_t frame, uint32_t data);

#endif /* INC_MLF_EFFECTS_H_ */
