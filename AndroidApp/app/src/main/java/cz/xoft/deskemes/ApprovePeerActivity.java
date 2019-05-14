package cz.xoft.deskemes;

import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.ComponentName;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.ServiceConnection;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.IBinder;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;

import java.security.KeyPair;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;





/** The activity that allows the user to approve a single remote peer (PublicID + PublicKey).
A thumbprint image is used to let the user visually verify the equalness of the data on both ends.*/
public class ApprovePeerActivity extends AppCompatActivity
{
	/** The tag used for logging. */
	private static final String TAG = "ApprovePeerActivity";

	/** The connectivity service, once it is bound. */
	private ConnectivityService mService;

	/** The friendly name reported by the remote, to be shown to the user. */
	private String mRemoteFriendlyName;

	/** The PublicID of the remote to be approved. */
	private byte[] mRemotePublicID;

	/** The PubKey of the remote to be approved. */
	private byte[] mRemotePublicKey;

	/** The progress dialog used while the activity is starting (keypair and thumbprint generation). */
	private ProgressDialog mProgressDlg;

	/** The generated local keypair to use for this peer. */
	private KeyPair mKeyPair;





	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_approve_peer);
	}





	@Override
	protected void onStart()
	{
		super.onStart();

		// Fill with the data from the intent:
		TextView txtRemoteName = (TextView)findViewById(R.id.txtRemoteName);
		TextView txtRemoteAddress = (TextView)findViewById(R.id.txtRemoteAddress);
		Intent intent = getIntent();
		mRemoteFriendlyName = intent.getStringExtra("RemoteFriendlyName");
		txtRemoteName.setText(mRemoteFriendlyName);
		txtRemoteAddress.setText(intent.getStringExtra("RemoteAddress"));
		mRemotePublicID = intent.getByteArrayExtra("RemotePublicID");
		mRemotePublicKey = intent.getByteArrayExtra("RemotePublicKey");
		if ((mRemotePublicID == null) || (mRemotePublicKey == null))
		{
			Log.d(TAG, "Remote identification not given, aborting activity.");
			finish();
			return;
		}
		CheckBox chbIsSame = (CheckBox)findViewById(R.id.chbIsSame);
		chbIsSame.setChecked(false);
		chbIsSame.setEnabled(false);
		Button btnAllow = (Button)findViewById(R.id.btnAllow);
		btnAllow.setEnabled(false);

		// Create the progress dialog:
		mProgressDlg = new ProgressDialog(this, ProgressDialog.STYLE_SPINNER);
		mProgressDlg.setMessage(getResources().getString(R.string.approve_peer_progress_connecting_service));
		mProgressDlg.show();
		mProgressDlg.setOnCancelListener(new DialogInterface.OnCancelListener()
			{
			  @Override
			  public void onCancel(DialogInterface aDialog)
			  {
				  aDialog.dismiss();
				  finish();
			  }
			}
		);

		// Bind to ConnectivityService:
		Intent svcIntent = new Intent(this, ConnectivityService.class);
		bindService(svcIntent, svcConn, Context.BIND_AUTO_CREATE);
	}





	@Override
	protected void onStop()
	{
		unbindService(svcConn);
		mService = null;
		super.onStop();
	}





	public void onAllowClick(View aView)
	{
		// Check if the user really checked the image (and checked the checkbox):
		CheckBox chbIsSame = (CheckBox)findViewById(R.id.chbIsSame);
		if (!chbIsSame.isChecked())
		{
			Toast.makeText(ApprovePeerActivity.this, R.string.approve_peer_need_to_check, Toast.LENGTH_SHORT).show();
			return;
		}

		// Add the approval:
		ApprovedPeers.Peer peer;
		try
		{
			peer = mService.approvedPeers().approvePeer(
				mRemotePublicID
			);
		}
		catch (Exception exc)
		{
			Log.d(TAG, "Failed to store peer approval.", exc);
			Toast.makeText(ApprovePeerActivity.this, R.string.approve_peer_err_store_try_again, Toast.LENGTH_SHORT).show();
			return;
		}

		// Notify ConnectionMgr to continue connecting:
		mService.connectionMgr().onPeerApproved(peer);

		// Finish the activity
		finish();
	}





	public void onForbidClick(View aView)
	{
		// Finish the activity without approving anything:
		finish();
	}





	/** Generates and returns a bitmap representing the thumbprint of the crypto keys and IDs. */
	private Bitmap generateThumbprint(
		byte[] aRemotePublicID,
		byte[] aLocalPublicID,
		byte[] aRemotePublicKey,
		byte[] aLocalPublicKey
	) throws NoSuchAlgorithmException
	{
		final int wid = 17;
		final int hei = 9;
		final int[] palette =
		{
			0xffffffff,
			0xff0000ff, 0xff00ff00, 0xffff0000,
			0xff7f7f7f, 0xffcfcfcf,
			0xff00ffff, 0xffffff00, 0xffff00ff,
			0xff00007f, 0xff007f00, 0xff7f0000,
			0xff00ff7f, 0xffff7f00, 0xff7f00ff,
			0xff007fff, 0xff7fff00, 0xffff007f,
			0xff007f7f, 0xff7f7f00, 0xff7f007f,
		};

		MessageDigest md = MessageDigest.getInstance("SHA-1");
		md.update(aRemotePublicID);
		md.update(aLocalPublicID);
		md.update(aRemotePublicKey);
		md.update(aLocalPublicKey);
		byte[] sha1hash = md.digest();

		// The drunken bishop algorithm: http://www.dirk-loss.de/sshvis/drunken_bishop.pdf
		// Adapted from OpenSSH C source: https://cvsweb.openbsd.org/cgi-bin/cvsweb/src/usr.bin/ssh/Attic/key.c?rev=1.70
		int[] colors = new int[wid * hei];
		int x = wid / 2;
		int y = hei / 2;
		for (byte h: sha1hash)
		{
			int val = h;
			for (byte shift = 0; shift < 4; ++shift)
			{
				x += ((val & 0x01) != 0) ? 1 : -1;
				y += ((val & 0x02) != 0) ? 1 : -1;
				x = Utils.clamp(x, 0, wid - 1);
				y = Utils.clamp(y, 0, hei - 1);
				val = val >> 2;
				colors[x + wid * y] += 1;
			}
		}
		for (int i = 0; i < wid * hei; ++i)
		{
			colors[i] = palette[Utils.clamp(colors[i], 0, palette.length - 1)];
		}
		return Bitmap.createBitmap(colors, wid, hei, Bitmap.Config.ARGB_8888);
	}





	/** The background task that needs to be done before the approval is even presented to the user. */
	private class AsyncTaskPreApproval extends AsyncTask<Object, Integer, Integer>
	{
		private Bitmap mThumbprint;

		@Override
		protected Integer doInBackground(Object... objects)
		{
			// Get the local keypair:
			if (mKeyPair == null)
			{
				try
				{
					loadOrGenerateKeyPair();
				}
				catch (Exception exc)
				{
					return R.string.approve_peer_err_local_keypair;
				}
			}

			// Generate the thumbprint image:
			publishProgress(1);
			try
			{
				mThumbprint = generateThumbprint(
					mRemotePublicID,
					mService.settings().getPublicID(),
					mRemotePublicKey,
					mKeyPair.getPublic().getEncoded()
				);
			}
			catch (NoSuchAlgorithmException e)
			{
				return R.string.approve_peer_err_thumbprint_algorithm;
			}

			return 0;
		}





		/** Loads the local keypair, if already known.
		Otherwise generates and stores a new local keypair for the peer. */
		private void loadOrGenerateKeyPair() throws Exception
		{
			ApprovedPeers ap = mService.approvedPeers();
			ApprovedPeers.Peer peer = ap.getPeer(mRemotePublicID);
			if (peer != null)
			{
				mKeyPair = peer.getLocalKeyPair();
			}
			if (mKeyPair == null)
			{
				publishProgress(0);
				mKeyPair = ApprovedPeers.generateLocalKeyPair();
				ap.storeLocalKeyPair(
					mRemoteFriendlyName,
					mRemotePublicID,
					mRemotePublicKey,
					mKeyPair
				);
			}
		}





		@Override
		protected void onPostExecute(Integer aResult)
		{
			mProgressDlg.dismiss();

			// If the task failed, show a message and terminate the whole activity:
			if (aResult != 0)
			{
				AlertDialog alertDialog = new AlertDialog.Builder(ApprovePeerActivity.this)
					.setTitle(R.string.approve_peer_failed)
					.setMessage(aResult)
					.create();
				alertDialog.setButton(AlertDialog.BUTTON_NEUTRAL, "OK",
					new DialogInterface.OnClickListener()
					{
						public void onClick(DialogInterface aDialog, int aWhich)
						{
							aDialog.dismiss();
						}
					}
				);
				alertDialog.show();
				finish();
				return;
			}

			// Show the generated image:
			ImageView imgThumbprint = (ImageView)findViewById(R.id.imgThumbprint);
			BitmapDrawable thd = new BitmapDrawable(mThumbprint);
			thd.setAntiAlias(false);
			thd.setFilterBitmap(false);
			imgThumbprint.setImageDrawable(thd);

			// Enable the UI:
			CheckBox chbIsSame = (CheckBox)findViewById(R.id.chbIsSame);
			chbIsSame.setChecked(false);
			chbIsSame.setEnabled(true);
			Button btnAllow = (Button)findViewById(R.id.btnAllow);
			btnAllow.setEnabled(true);

			// Notify the ConnectionManager that it should send the pubk
			mService.connectionMgr().onPublicKeyGenerated(mRemotePublicID, mKeyPair.getPublic().getEncoded());
		}





		@Override
		protected void onProgressUpdate(Integer... aProgress)
		{
			int txtID;
			switch (aProgress[0])
			{
				case 0: txtID = R.string.approve_peer_progress_generating_keypair; break;
				case 1: txtID = R.string.approve_peer_progress_generating_image;   break;
				default:
				{
					Log.d(TAG, "Unknown progress value: " + aProgress[0]);
					return;
				}
			}
			mProgressDlg.setMessage(getResources().getString(txtID));
		}
	}





	private ServiceConnection svcConn = new ServiceConnection()
	{
		@Override
		public void onServiceConnected(ComponentName aName, IBinder aBinder)
		{
			mService = ((ConnectivityService.Binder)aBinder).service();
			(new AsyncTaskPreApproval()).execute(mService);
		}





		@Override
		public void onServiceDisconnected(ComponentName aName)
		{
			mService = null;
		}
	};
}
