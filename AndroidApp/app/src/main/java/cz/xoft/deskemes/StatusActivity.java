package cz.xoft.deskemes;

import android.app.Activity;
import android.os.Bundle;
import android.view.WindowManager;





public class StatusActivity extends Activity
{

	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_status);

		// DEBUG: Wake the screen up, so that UDP broadcasts work while this activity is on-screen
		getWindow().addFlags(WindowManager.LayoutParams.FLAG_DISMISS_KEYGUARD);
		getWindow().addFlags(WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED);
		getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
		getWindow().addFlags(WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON);

		ConnectivityService.startIfNotRunning(this);
	}
}
