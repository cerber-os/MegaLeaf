from ctypes import *
from typing import Tuple


_MLF_LIBRARY = cdll.LoadLibrary("./output/libMLFProtoLib.so")

################################
# ENTRY POINTS HEADERS
################################
#   MLF_handler MLFProtoLib_Init(char* path)
_MLF_LIBRARY.MLFProtoLib_Init.restype = c_void_p
_MLF_LIBRARY.MLFProtoLib_Init.argtypes = [c_char_p]

#   void MLFProtoLib_Deinit(MLF_handler handler)
_MLF_LIBRARY.MLFProtoLib_Deinit.restype = None
_MLF_LIBRARY.MLFProtoLib_Deinit.argtypes = [c_void_p]

#   void MLFProtoLib_GetFWVersion(MLF_handler handle, int* version)
_MLF_LIBRARY.MLFProtoLib_GetFWVersion.restype = None
_MLF_LIBRARY.MLFProtoLib_GetFWVersion.argtypes = [c_void_p, c_void_p]

#   void MLFProtoLib_GetLedsCount(MLF_handler handle, int* top, int* bottom)
_MLF_LIBRARY.MLFProtoLib_GetLedsCount.restype = None
_MLF_LIBRARY.MLFProtoLib_GetLedsCount.argtypes = [c_void_p, c_void_p, c_void_p]

#   int MLFProtoLib_TurnOff(MLF_handler handle)
_MLF_LIBRARY.MLFProtoLib_TurnOff.restype = c_int
_MLF_LIBRARY.MLFProtoLib_TurnOff.argtypes = [c_void_p]

#   int MLFProtoLib_SetBrightness(MLF_handler handle, int val)
_MLF_LIBRARY.MLFProtoLib_SetBrightness.restype = c_int
_MLF_LIBRARY.MLFProtoLib_SetBrightness.argtypes = [c_void_p, c_int]

#   int MLFProtoLib_SetColors(MLF_handler handle, int* colors, int len)
_MLF_LIBRARY.MLFProtoLib_SetColors.restype = c_int
_MLF_LIBRARY.MLFProtoLib_SetColors.argtypes = [c_void_p, c_void_p, c_int]

#   int MLFProtoLib_SetEffect(MLF_handler handle, int effect, int speed, int strip, int color)
_MLF_LIBRARY.MLFProtoLib_SetEffect.restype = c_int
_MLF_LIBRARY.MLFProtoLib_SetEffect.argtypes = [c_void_p, c_int, c_int, c_int, c_int]


################################
# Wrapper for Cpp class
################################
class MLFExcetion(Exception):
    pass

class MLFProtoLib:
    def __init__(self, path: str = ""):
        self._handle = _MLF_LIBRARY.MLFProtoLib_Init(path.encode())
        if self._handle == 0 or self._handle is None:
            raise MLFExcetion("Failed to initialize MLFProtoLib module")

    def __del__(self):
        _MLF_LIBRARY.MLFProtoLib_Deinit(self._handle)

    def getFWVersion(self) -> int:
        fwVersion = c_int()
        _MLF_LIBRARY.MLFProtoLib_GetFWVersion(self._handle, byref(fwVersion))
        return fwVersion.value

    def getLedsCount(self) -> Tuple[int, int]:
        top = c_int()
        bottom = c_int()
        _MLF_LIBRARY.MLFProtoLib_GetLedsCount(self._handle, byref(top), byref(bottom))
        return (top.value, bottom.value)

    def turnOff(self) -> None:
        ret = _MLF_LIBRARY.MLFProtoLib_TurnOff(self._handle)
        if ret != 0:
            raise MLFExcetion("Failed to turn off MLF panel")

    def setBrightness(self, brightness: int) -> None:
        ret = _MLF_LIBRARY.MLFProtoLib_SetBrightness(self._handle, brightness)
        if ret != 0:
            raise MLFExcetion("Failed to change brightness MLF panel")

    def setColors(self, colors) -> None:
        dst = c_int() * len(colors)
        for i, color in enumerate(colors):
            dst[i] = color
        ret = _MLF_LIBRARY.MLFProtoLib_SetBrightness(self._handle, byref(dst), len(dst))
        if ret != 0:
            raise MLFExcetion("Failed to change color of MLF panel")
        

    def setEffect(self, effect: int, speed: int = 0, strip: int = 0b11, color: int = 0):
        ret = _MLF_LIBRARY.MLFProtoLib_SetEffect(self._handle, effect, speed, strip, color)
        if ret != 0:
            raise MLFExcetion("Failed to set effect on MLF panel")


# Test area 
controller = MLFProtoLib()
fw_version = controller.getFWVersion()
count = controller.getLedsCount()
print(f'FW version: {fw_version}')
print(f'Leds count: {count[0]} and {count[1]}')

controller.setEffect(0, 9)
controller.setBrightness(200)