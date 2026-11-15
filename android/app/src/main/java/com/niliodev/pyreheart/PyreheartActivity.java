package com.niliodev.pyreheart;

import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.ViewConfiguration;

import org.libsdl.app.SDLActivity;

/** SDL runs the game; this only passes the watch crown on, as turns of a mouse wheel. */
public class PyreheartActivity extends SDLActivity {
    private float crown = 0;

    @Override
    public boolean dispatchGenericMotionEvent(MotionEvent ev) {
        if (ev.getAction() == MotionEvent.ACTION_SCROLL && ev.isFromSource(InputDevice.SOURCE_ROTARY_ENCODER)) {
            float px = -ev.getAxisValue(MotionEvent.AXIS_SCROLL)
                    * ViewConfiguration.get(this).getScaledVerticalScrollFactor();
            crown += px;
            // one step of the ring for every bit of turn, however fast the crown spins
            final float step = 48f;
            while (Math.abs(crown) >= step) {
                float dir = crown > 0 ? -1f : 1f;
                crown -= crown > 0 ? step : -step;
                SDLActivity.onNativeMouse(0, MotionEvent.ACTION_SCROLL, 0f, dir, false);
            }
            return true;
        }
        return super.dispatchGenericMotionEvent(ev);
    }
}
