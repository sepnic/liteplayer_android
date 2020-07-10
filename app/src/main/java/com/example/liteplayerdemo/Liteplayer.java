package com.example.liteplayerdemo;

import android.app.Activity;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.Message;
import android.util.Log;
import android.widget.TextView;

import java.io.IOException;
import java.lang.ref.WeakReference;

public class Liteplayer extends Activity {
    private static final int LITEPLAYER_IDLE            = 0x00;
    private static final int LITEPLAYER_INITED          = 0x01;
    private static final int LITEPLAYER_PREPARED        = 0x02;
    private static final int LITEPLAYER_STARTED         = 0x03;
    private static final int LITEPLAYER_PAUSED          = 0x04;
    private static final int LITEPLAYER_SEEKCOMPLETED   = 0x05;
    private static final int LITEPLAYER_CACHECOMPLETED  = 0x06;
    private static final int LITEPLAYER_NEARLYCOMPLETED = 0x07;
    private static final int LITEPLAYER_COMPLETED       = 0x08;
    private static final int LITEPLAYER_STOPPED         = 0x09;
    private static final int LITEPLAYER_ERROR           = 0x0A;

    private final static String TAG = "Litelayer";
    private long mNativeContext; // accessed by native methods
    private EventHandler mEventHandler;

    public Liteplayer() {
        Looper looper;
        if ((looper = Looper.myLooper()) != null) {
            mEventHandler = new EventHandler(this, looper);
        } else if ((looper = Looper.getMainLooper()) != null) {
            mEventHandler = new EventHandler(this, looper);
        } else {
            Log.w(TAG, "no event handler");
            mEventHandler = null;
        }
        /* Native setup requires a weak reference to our object.
         * It's easier to create it here than in C++.
         */
        native_create(new WeakReference<Liteplayer>(this));
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // Example of a call to a native method
        TextView tv = findViewById(R.id.sample_text);
        tv.setText("Liteplayer demo");
    }

    /** Called when the activity is about to be destroyed. */
    @Override
    protected void onDestroy()
    {
        native_destroy();
        super.onDestroy();
    }

    private class EventHandler extends Handler
    {
        private Liteplayer mLiteplayer;

        public EventHandler(Liteplayer p, Looper looper) {
            super(looper);
            mLiteplayer = p;
        }

        @Override
        public void handleMessage(Message msg) {
            if (mLiteplayer.mNativeContext == 0) {
                Log.w(TAG, "liteplayer went away with unhandled events");
                return;
            }
            switch(msg.what) {
                case LITEPLAYER_IDLE:
                    Log.i(TAG, "-->LITEPLAYER_IDLE");
                    return;

                case LITEPLAYER_INITED:
                    Log.i(TAG, "-->LITEPLAYER_INITED");
                    return;

                case LITEPLAYER_PREPARED:
                    Log.i(TAG, "-->LITEPLAYER_PREPARED");
                    return;

                case LITEPLAYER_STARTED:
                    Log.i(TAG, "-->LITEPLAYER_STARTED");
                    return;

                case LITEPLAYER_PAUSED:
                    Log.i(TAG, "-->LITEPLAYER_PAUSED");
                    return;

                case LITEPLAYER_SEEKCOMPLETED:
                    Log.i(TAG, "-->LITEPLAYER_SEEKCOMPLETED");
                    return;

                case LITEPLAYER_CACHECOMPLETED:
                    Log.i(TAG, "-->LITEPLAYER_CACHECOMPLETED");
                    return;

                case LITEPLAYER_NEARLYCOMPLETED:
                    Log.i(TAG, "-->LITEPLAYER_NEARLYCOMPLETED");
                    return;

                case LITEPLAYER_COMPLETED:
                    Log.i(TAG, "-->LITEPLAYER_COMPLETED");
                    return;

                case LITEPLAYER_STOPPED:
                    Log.i(TAG, "-->LITEPLAYER_STOPPED");
                    return;

                case LITEPLAYER_ERROR:
                    Log.e(TAG, "-->LITEPLAYER_ERROR: (" + msg.arg1 + "," + msg.arg2 + ")");
                    return;

                default:
                    Log.e(TAG, "-->Unknown message: " + msg.what);
                    return;
            }
        }
    }

    /*
     * Called from native code when an interesting event happens.  This method
     * just uses the EventHandler system to post the event back to the main app thread.
     */
    private static void postEventFromNative(Object liteplayer_ref, int state, int errno)
    {
        Liteplayer p = (Liteplayer)((WeakReference)liteplayer_ref).get();
        if (p == null) {
            return;
        }
        if (p.mEventHandler != null) {
            Message m = p.mEventHandler.obtainMessage(state, errno, 0, null);
            p.mEventHandler.sendMessage(m);
        }
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    public native void native_create(Object liteplayer_this);
    public native void native_setDataSource(String path);
    public native void native_prepareAsync();
    public native void native_start();
    public native void native_pause();
    public native void native_resume();
    public native void native_seekTo(int msec);
    public native void native_stop();
    public native void native_reset();
    public native int native_getCurrentPosition();
    public native int native_getDuration();
    public native void native_destroy();

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("liteplayer-jni");
    }
}
