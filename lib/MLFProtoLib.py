#!/usr/bin/python3
""" Python bindings for MegaLeaf controller C library
"""

from ctypes import *
from typing import Tuple

__author__ = 'Pawel Wieczorek'

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

#   int MLFProtoLib_TurnOn(MLF_handler handle)
_MLF_LIBRARY.MLFProtoLib_TurnOn.restype = c_int
_MLF_LIBRARY.MLFProtoLib_TurnOn.argtypes = [c_void_p]

#   int MLFProtoLib_TurnOff(MLF_handler handle)
_MLF_LIBRARY.MLFProtoLib_TurnOff.restype = c_int
_MLF_LIBRARY.MLFProtoLib_TurnOff.argtypes = [c_void_p]

#   int MLFProtoLib_IsTurnedOn(MLF_handler handle)
_MLF_LIBRARY.MLFProtoLib_IsTurnedOn.restype = c_int
_MLF_LIBRARY.MLFProtoLib_IsTurnedOn.argtypes = [c_void_p]

#   int MLFProtoLib_SetBrightness(MLF_handler handle, int val)
_MLF_LIBRARY.MLFProtoLib_SetBrightness.restype = c_int
_MLF_LIBRARY.MLFProtoLib_SetBrightness.argtypes = [c_void_p, c_int]

#   int MLFProtoLib_SetColors(MLF_handler handle, int* colors, int len)
_MLF_LIBRARY.MLFProtoLib_SetColors.restype = c_int
_MLF_LIBRARY.MLFProtoLib_SetColors.argtypes = [c_void_p, c_void_p, c_int]

#   int MLFProtoLib_SetEffect(MLF_handler handle, int effect, int speed, int strip, int color)
_MLF_LIBRARY.MLFProtoLib_SetEffect.restype = c_int
_MLF_LIBRARY.MLFProtoLib_SetEffect.argtypes = [c_void_p, c_int, c_int, c_int, c_int]

#   int MLFProtoLib_GetBrightness(MLF_handler handle)
_MLF_LIBRARY.MLFProtoLib_GetBrightness.restype = c_int
_MLF_LIBRARY.MLFProtoLib_GetBrightness.argtypes = [c_void_p]

#   int MLFProtoLib_GetEffect(MLF_handler handle, int* effect, int* speed, int* color);
_MLF_LIBRARY.MLFProtoLib_GetEffect.restype = c_int
_MLF_LIBRARY.MLFProtoLib_GetEffect.argtypes = [c_void_p, c_void_p, c_void_p, c_void_p]

#  const char* MLFProtoLib_GetError(MLF_handler handle)
_MLF_LIBRARY.MLFProtoLib_GetError.resType = c_char_p
_MLF_LIBRARY.MLFProtoLib_GetError.argtypes = [c_void_p]

################################
# Wrapper for Cpp class
################################
class MLFException(Exception):
    pass

class MLFProto:
    def __init__(self, path: str = ""):
        self._handle = _MLF_LIBRARY.MLFProtoLib_Init(path.encode())
        if self._handle == 0 or self._handle is None:
            raise MLFException("Failed to initialize MLFProtoLib module")

    def __del__(self):
        _MLF_LIBRARY.MLFProtoLib_Deinit(self._handle)

    def _getError(self) -> str:
        return _MLF_LIBRARY.MLFProtoLib_GetError(self._handle)

    def getFWVersion(self) -> int:
        fwVersion = c_int()
        _MLF_LIBRARY.MLFProtoLib_GetFWVersion(self._handle, byref(fwVersion))
        return fwVersion.value

    def getLedsCount(self) -> Tuple[int, int]:
        top = c_int()
        bottom = c_int()
        _MLF_LIBRARY.MLFProtoLib_GetLedsCount(self._handle, byref(top), byref(bottom))
        return (top.value, bottom.value)

    def turnOn(self) -> None:
        ret = _MLF_LIBRARY.MLFProtoLib_TurnOn(self._handle)
        if ret != 0:
            raise MLFException("Failed to turn on MLF panel" + self._getError())

    def turnOff(self) -> None:
        ret = _MLF_LIBRARY.MLFProtoLib_TurnOff(self._handle)
        if ret != 0:
            raise MLFException("Failed to turn off MLF panel" + self._getError())
    
    def isTurnedOn(self) -> int:
        ret = _MLF_LIBRARY.MLFProtoLib_IsTurnedOn(self._handle)
        if ret < 0:
            raise MLFException("Failed to get on_state of MLF panel" + self._getError())
        return ret

    def setBrightness(self, brightness: int) -> None:
        ret = _MLF_LIBRARY.MLFProtoLib_SetBrightness(self._handle, brightness)
        if ret != 0:
            raise MLFException("Failed to change brightness MLF panel" + self._getError())

    def setColors(self, colors) -> None:
        dst = c_int * len(colors)
        dst = dst(*colors)
        ret = _MLF_LIBRARY.MLFProtoLib_SetColors(self._handle, byref(dst), len(dst))
        if ret != 0:
            raise MLFException("Failed to change color of MLF panel" + self._getError())


    def setEffect(self, effect: int, speed: int = 0, strip: int = 0b11, color: int = 0):
        ret = _MLF_LIBRARY.MLFProtoLib_SetEffect(self._handle, effect, speed, strip, color)
        if ret != 0:
            raise MLFException("Failed to set effect on MLF panel" + self._getError())

    def getBrightness(self) -> int:
        ret = _MLF_LIBRARY.MLFProtoLib_GetBrightness(self._handle)
        if ret < 0:
            raise MLFException("Failed to get brightness of MLF panel" + self._getError())
        return ret
    
    def getEffect(self) -> Tuple[int, int, int]:
        effect = c_int()
        speed = c_int()
        color = c_int()

        ret = _MLF_LIBRARY.MLFProtoLib_GetEffect(self._handle, byref(effect), byref(speed), byref(color))
        if ret != 0:
            raise MLFException("Failed to get effect from MLF panel" + self._getError())
        return (effect.value, speed.value, color.value)


# Test area
controller = MLFProto()
fw_version = controller.getFWVersion()
count = controller.getLedsCount()
print(f'FW version: {fw_version}')
print(f'Leds count: {count[0]} and {count[1]}')

controller.setEffect(0, 0, 0b11, 0x0550ff)
controller.setBrightness(213)

print("Brightness: ", controller.getBrightness())
print("Effect: ", controller.getEffect())

