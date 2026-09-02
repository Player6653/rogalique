#include "pch.h"
#include "MouseWheelBuffer.h"

namespace
{
    float g_deltaThisFrame = 0.f;
}

namespace MouseWheelBuffer
{
    float deltaThisFrame()
    {
        return g_deltaThisFrame;
    }

    void pushDelta(float delta)
    {
        g_deltaThisFrame += delta;
    }

    void clear()
    {
        g_deltaThisFrame = 0.f;
    }
} // namespace MouseWheelBuffer
