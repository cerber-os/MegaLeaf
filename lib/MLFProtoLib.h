/**
 * @file MLFProtoLib.h
 * @author Pawel Wieczorek
 * @brief Header file providing C bindings for MLFProtoLib
 * @date 2022-06-14
 * 
 */

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief MLFProtoLib object handler
 * 
 */
struct MLF_C_Object;
typedef MLF_C_Object *MLF_handler;

/**
 * @brief Initializes MLFProtoLib object
 * 
 * @param path path to MLF Controller device or empty to auto detect
 * @return MLF_handler library handler or NULL if an error occurred
 */
MLF_handler MLFProtoLib_Init(char* path);

/**
 * @brief Deinitializes MLFProtoLib object
 * 
 * @param handle MLFProtoLib handler
 */
void MLFProtoLib_Deinit(MLF_handler handle);


/**
 * @brief Retrieve current FW version
 * 
 * @param handle MLFProtoLib handler
 * @param version pointer to memory in which FW version will be stored
 */
void MLFProtoLib_GetFWVersion(MLF_handler handle, int* version);

/**
 * @brief Retrieve number of LEDS on both strips
 * 
 * @param handle MLFProtoLib handler
 * @param top    number of leds stored on top strip
 * @param bottom number of leds stored on bottom strip
 */
void MLFProtoLib_GetLedsCount(MLF_handler handle, int* top, int* bottom);

/**
 * @brief Bring back all LEDs to the state from before calling
 *          MLFProtoLib_TurnOff
 * 
 * @param handle MLFProtoLib handler
 * @return int  0 on success, -1 otherwise
 */
int MLFProtoLib_TurnOn(MLF_handler handle);

/**
 * @brief Turn off all LEDs
 * 
 * @param handle MLFProtoLib handler
 * @return int   0 on success, -1 otherwise
 */
int MLFProtoLib_TurnOff(MLF_handler handle);

/**
 * @brief Return true if LEDs aren't in OFF mode now
 * 
 * @param handle MLFProtoLib handler
 * @return int   0/1 - LEDs off/on, -1 in case of an error
 */
int MLFProtoLib_IsTurnedOn(MLF_handler handle);

/**
 * @brief Set brightness of all LEDS
 * 
 * @param handle MLFProtoLib handler
 * @param val    target brightness to be set
 * @return int   0 on success, -1 otherwise
 */
int MLFProtoLib_SetBrightness(MLF_handler handle, int val);

/**
 * @brief Set color of all LEDS
 * 
 * @param handle MLFProtoLib handler
 * @param colors array of integers representing color of each LED
 * @param len    number of elements in `colors`
 * @return int   0 on success, -1 otherwise
 */
int MLFProtoLib_SetColors(MLF_handler handle, int* colors, int len);

/**
 * @brief Run one of the predefined effects on selected LED strip
 * 
 * @param handle MLFProtoLib handler
 * @param effect ID of predefined effect
 * @param speed  how fast the effect will be running (lowest: 0)
 * @param strip  bitfield indicating target strip (1 - bottom, 2 - top)
 * @param color  optional color parameter used by selected effect
 * @return int   0 on success, -1 otherwise
 */
int MLFProtoLib_SetEffect(MLF_handler handle, int effect, int speed, int strip, int color);

/**
 * @brief Acquire currently set brightness from MegaLeaf controller
 * 
 * @param handle MLFProtoLib handler
 * @return int   [0:255] current brightness, -1 in case of an error
 */
int MLFProtoLib_GetBrightness(MLF_handler handle);

/**
 * @brief Acquire currently set effect from MegaLeaf controller
 * 
 * @param handle MLFProtoLib handler
 * @param effect place to store ID of current effect
 * @param speed  place to store the speed of current effect
 * @param color  place to store the color associated with present effect
 */
int MLFProtoLib_GetEffect(MLF_handler handle, int* effect, int* speed, int* color);

/**
 * @brief Retrieve the last error reported by library
 * 
 * @param handle MLFProtoLib handler
 * @return const char* string containing error content
 */
const char* MLFProtoLib_GetError(MLF_handler handle);

#ifdef __cplusplus
}
#endif
