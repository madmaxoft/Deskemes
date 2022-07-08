package cz.xoft.deskemes;

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.util.Base64;
import android.util.Log;

import java.math.BigInteger;
import java.net.Socket;
import java.security.KeyFactory;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.NoSuchAlgorithmException;
import java.security.Principal;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.SecureRandom;
import java.security.cert.CertificateException;
import java.security.cert.X509Certificate;
import java.security.spec.EncodedKeySpec;
import java.security.spec.InvalidKeySpecException;
import java.security.spec.PKCS8EncodedKeySpec;
import java.security.spec.X509EncodedKeySpec;
import java.util.Date;

import javax.net.ssl.SSLEngine;

import org.spongycastle.asn1.x500.X500Name;
import org.spongycastle.cert.X509CertificateHolder;
import org.spongycastle.cert.X509v3CertificateBuilder;
import org.spongycastle.cert.jcajce.JcaX509CertificateConverter;
import org.spongycastle.cert.jcajce.JcaX509v3CertificateBuilder;
import org.spongycastle.jce.provider.BouncyCastleProvider;
import org.spongycastle.operator.ContentSigner;
import org.spongycastle.operator.jcajce.JcaContentSignerBuilder;


/** Manages (and persists) the list of all peers that have been approved-paired by the user.
A peer can be in one of these three states:
- Unknown (first time hearing about this ID, no record in the DB)
- With keys (the local keypair has been generated, but the peer hasn't been approved by the user yet)
- Approved (the local keypair has been generated and the user approved the peer)

To approve a peer that hasn't been encountered before:
1. generateLocalKeypair()
2. storeLocalKeyPair()
3. approvePeer()
*/
class ApprovedPeers
{
	/** The exception that is thrown when trying to approve a peer that doesn't have its local keypair stored yet. */
	class LocalKeyPairNotStoredException extends Exception
	{
	}





	/** The SQL helper class that opens the DB and upgrades it to the proper version as needed.
	See https://developer.android.com/training/data-storage/sqlite#DbHelper for details. */
	class DbHelper extends SQLiteOpenHelper
	{
		DbHelper(Context aContext)
		{
			super(aContext, "ApprovedPeers.sqlite", null, 1);
		}

		@Override
		public void onCreate(SQLiteDatabase aDB)
		{
			aDB.execSQL("CREATE TABLE ApprovedPeers (" +
				"FriendlyName TEXT, " +
				"RemotePublicIDB64 TEXT PRIMARY KEY, " +
				"RemotePublicKeyDataB64 TEXT, " +
				"LocalPublicKeyDataB64 TEXT, " +
				"LocalPrivateKeyDataB64 TEXT, " +
				"IsApproved BOOLEAN)"
			);
		}

		@Override
		public void onUpgrade(SQLiteDatabase aDB, int aFromVersion, int aToVersion)
		{
			// Nothing needed yet
		}
	}




	/** Simple storage for the data for a single approved peer. */
	class Peer
	{
		String mFriendlyName;
		byte[] mRemotePublicID;
		byte[] mRemotePublicKeyData;
		byte[] mLocalPublicKeyData;
		byte[] mLocalPrivateKeyData;
		boolean mIsApproved;

		Peer(
			String aFriendlyName,
			byte[] aRemotePublicID,
			byte[] aRemotePublicKeyData,
			byte[] aLocalPublicKeyData,
			byte[] aLocalPrivateKeyData,
			boolean aIsApproved
		)
		{
			mFriendlyName = aFriendlyName;
			mRemotePublicID = aRemotePublicID;
			mRemotePublicKeyData = aRemotePublicKeyData;
			mLocalPublicKeyData = aLocalPublicKeyData;
			mLocalPrivateKeyData = aLocalPrivateKeyData;
			mIsApproved = aIsApproved;
		}





		/** Decodes the local keypair data into an actual KeyPair instance.
		Returns null if the data cannot be decoded. */
		public KeyPair getLocalKeyPair()
		{
			EncodedKeySpec pubKeySpec = new X509EncodedKeySpec(mLocalPublicKeyData);
			EncodedKeySpec privKeySpec = new PKCS8EncodedKeySpec(mLocalPrivateKeyData);
			PrivateKey privKey;
			PublicKey pubKey;
			try
			{
				KeyFactory keyFactory = KeyFactory.getInstance("RSA");
				pubKey = keyFactory.generatePublic(pubKeySpec);
				privKey = keyFactory.generatePrivate(privKeySpec);
			}
			catch (InvalidKeySpecException | NoSuchAlgorithmException exc)
			{
				Log.d(TAG, "Failed to convert key data to keypair", exc);
				return null;
			}
			return new KeyPair(pubKey, privKey);
		}





		/** Returns an array of KeyManager instances that can be used for SSLContext initialization. */
		javax.net.ssl.KeyManager[] getKeyManagers()
		{
			return new javax.net.ssl.KeyManager[]{new KeyManager(this)};
		}




		/** Returns an array of TrustManager instances that can be used for SSLContext initialization. */
		javax.net.ssl.TrustManager[] getTrustManagers()
		{
			return new javax.net.ssl.TrustManager[]{new TrustManager(this)};
		}
	}





	/** A KeyManager implementation that provides the Peer's assigned private key. */
	final class KeyManager implements javax.net.ssl.X509KeyManager
	{
		/** The Peer for which the Keymanager instance has been created. */
		private Peer mPeer;

		/** The kees to be used for the local end of the communication channel. */
		private KeyPair mLocalKeyPair;

		public KeyManager(Peer aPeer)
		{
			mPeer = aPeer;
			mLocalKeyPair = aPeer.getLocalKeyPair();
		}

		@Override
		public String[] getClientAliases(String s, Principal[] principals)
		{
			return new String[0];
		}

		@Override
		public String chooseClientAlias(String[] strings, Principal[] principals, Socket socket)
		{
			return null;
		}

		@Override
		public String[] getServerAliases(String s, Principal[] principals)
		{
			// We only support connections to a server (we're a client)
			return null;
		}

		@Override
		public String chooseServerAlias(String s, Principal[] principals, Socket socket)
		{
			// We only support connections to a server (we're a client)
			return null;
		}

		@Override
		public X509Certificate[] getCertificateChain(String s)
		{
			X500Name issuer = new X500Name("CN=Deskemes");
			BigInteger serial = BigInteger.probablePrime(16, new SecureRandom());
			Date notBefore = new Date(System.currentTimeMillis() - 24 * 60 * 60 * 1000);
			Date notAfter = new Date(notBefore.getTime() + 365 * 24 * 60 * 60 * 1000);

			final X509Certificate cert;
			try
			{
				final X509v3CertificateBuilder
				new BigInteger(64, new SecureRandom()),
						notBefore,
						notAfter,
						issuer,
						mLocalKeyPair.getPublic()
				);
				final ContentSigner signer = new JcaContentSignerBuilder("SHA256WithRSAEncryption").build(mLocalKeyPair.getPrivate());
				final X509CertificateHolder certHolder = builder.build(signer);
				cert = new JcaX509CertificateConverter(). builder = new JcaX509v3CertificateBuilder(
					issuer,setProvider(new BouncyCastleProvider()).getCertificate(certHolder);
				cert.verify(mLocalKeyPair.getPublic());
			}
			catch (Throwable t)
			{
				return null;
			}
			return new X509Certificate[]{cert};
		}

		@Override
		public PrivateKey getPrivateKey(String s)
		{
			return mLocalKeyPair.getPrivate();
		}
	}  // class KeyManager





	final class TrustManager extends javax.net.ssl.X509ExtendedTrustManager
	{
		/** The Peer for which this TrustManager instance has been created. */
		private Peer mPeer;


		public TrustManager(Peer aPeer)
		{
			mPeer = aPeer;
		}

		@Override
		public void checkClientTrusted(X509Certificate[] aCerts, String aAuthType) throws CertificateException
		{
			// We only support server connections
			throw new CertificateException();
		}

		@Override
		public void checkServerTrusted(X509Certificate[] aCerts, String aAuthType) throws CertificateException
		{
			// TODO: Check the server cert against mPeer's public key
		}

		@Override
		public X509Certificate[] getAcceptedIssuers()
		{
			return null;
		}

		@Override
		public void checkClientTrusted(X509Certificate[] x509Certificates, String s, Socket socket) throws CertificateException
		{
			// We only support server connections
			throw new CertificateException();
		}

		@Override
		public void checkServerTrusted(X509Certificate[] x509Certificates, String s, Socket socket) throws CertificateException
		{
			// We only support connections through a SSLEngine
			throw new CertificateException();
		}

		@Override
		public void checkClientTrusted(X509Certificate[] x509Certificates, String s, SSLEngine sslEngine) throws CertificateException
		{
			// We only support server connections
			throw new CertificateException();
		}

		@Override
		public void checkServerTrusted(X509Certificate[] x509Certificates, String s, SSLEngine sslEngine) throws CertificateException
		{
			// TODO
		}
	}
	/** The tag used for logging. */
	private static final String TAG = "ApprovedPeers";

	/** The DB used for storage of the Peer information. */
	private SQLiteDatabase mDB;





	ApprovedPeers(Context aContext)
	{
		mDB = (new DbHelper(aContext)).getWritableDatabase();
	}





	/** Returns the pairing data for the specified peer, if it is known.
	Returns null if peer not known. The peer may still be unapproved, check its mIsApproved member. */
	Peer getPeer(byte[] aRemotePublicID)
	{
		String[] wherePlaceholders = {Base64.encodeToString(aRemotePublicID, Base64.NO_WRAP) };
		Cursor cursor = mDB.query(
			"ApprovedPeers",  // Table name
			null,  // Columns to return: all
			"RemotePublicIDB64 = ?",  // WHERE clause filter
			wherePlaceholders,  // WHERE clause placeholder values
			null, null,  // No grouping
			null  // No sort order
		);
		if (!cursor.moveToFirst())
		{
			// Not found
			return null;
		}
		int ciFriendlyName        = cursor.getColumnIndexOrThrow("FriendlyName");
		int ciRemotePublicKeyData = cursor.getColumnIndexOrThrow("RemotePublicKeyDataB64");
		int ciLocalPublicKeyData  = cursor.getColumnIndexOrThrow("LocalPublicKeyDataB64");
		int ciLocalPrivateKeyData = cursor.getColumnIndexOrThrow("LocalPrivateKeyDataB64");
		int ciIsApproved          = cursor.getColumnIndexOrThrow("IsApproved");
		byte[] remotePublicKeyData = cursor.getBlob(ciRemotePublicKeyData);
		if (remotePublicKeyData == null)
		{
			return null;
		}
		byte[] localPublicKeyData  = cursor.isNull(ciLocalPublicKeyData) ? null : cursor.getBlob(ciLocalPublicKeyData);
		byte[] localPrivateKeyData = cursor.isNull(ciLocalPrivateKeyData) ? null : cursor.getBlob(ciLocalPrivateKeyData);
		Peer res;
		if ((localPublicKeyData != null) && (localPrivateKeyData != null))
		{
			res = new Peer(
				cursor.getString(ciFriendlyName),
				aRemotePublicID,
				Base64.decode(remotePublicKeyData, Base64.NO_WRAP),
				Base64.decode(localPublicKeyData,  Base64.NO_WRAP),
				Base64.decode(localPrivateKeyData, Base64.NO_WRAP),
				(cursor.getInt(ciIsApproved) != 0)
			);
		}
		else
		{
			res = null;
		}
		cursor.close();
		return res;
	}





	/** Adds a new peer into the DB that is not yet approved, but has its local keys generated.
	If the remoteID already exists in the DB, it is overwritten. */
	void storeLocalKeyPair(
		String aFriendlyName,
		byte[] aRemotePublicID,
		byte[] aRemotePublicKeyData,
		KeyPair aLocalKeyPair
	)
	{
		byte[] localPrivateKeyData = aLocalKeyPair.getPrivate().getEncoded();
		byte[] localPublicKeyData  = aLocalKeyPair.getPublic().getEncoded();
		String remotePublicIDB64 = Base64.encodeToString(aRemotePublicID, Base64.NO_WRAP);

		// Remove any previous approval / keypair from the DB:
		mDB.delete(
			"ApprovedPeers",
			"RemotePublicIDB64 = ?",
			new String[]{remotePublicIDB64}
		);

		// Store in the DB:
		ContentValues values = new ContentValues();
		values.put("FriendlyName",           aFriendlyName);
		values.put("RemotePublicIDB64",      remotePublicIDB64);
		values.put("RemotePublicKeyDataB64", Base64.encode(aRemotePublicKeyData, Base64.NO_WRAP));
		values.put("LocalPublicKeyDataB64",  Base64.encode(localPublicKeyData,   Base64.NO_WRAP));
		values.put("LocalPrivateKeyDataB64", Base64.encode(localPrivateKeyData,  Base64.NO_WRAP));
		values.put("IsApproved",             false);
		try
		{
			mDB.insert("ApprovedPeers",  null, values);
		}
		catch (Exception exc)
		{
			Log.d(TAG, "Failed to insert approval to DB", exc);
			throw exc;
		}

	}





	/** Adds a new approved pairing for the specified peer.
	Returns a filled-in Peer object containing the local keypair as well as the remote ident. */
	Peer approvePeer(
		byte[] aRemotePublicID
	) throws LocalKeyPairNotStoredException
	{
		// Update the approval in the DB:
		String[] whereArgs = { Base64.encodeToString(aRemotePublicID, Base64.NO_WRAP) };
		ContentValues values = new ContentValues();
		values.put("IsApproved", true);
		int numModified = mDB.update("ApprovedPeers", values, "RemotePublicIDB64 = ?", whereArgs);
		if (numModified == 0)
		{
			throw new LocalKeyPairNotStoredException();
		}

		// Read the full Peer from the DB:
		return getPeer(aRemotePublicID);
	}





	/** Generates a new keypair to be used for the local end of communication. */
	static KeyPair generateLocalKeyPair() throws Exception
	{
		Log.d(TAG, "Generating a keypair...");
		KeyPairGenerator kpg;
		try
		{
			kpg = KeyPairGenerator.getInstance("RSA");
		}
		catch (NoSuchAlgorithmException exc)
		{
			throw new Exception("Failed to get RSA keypair generator", exc);
		}
		kpg.initialize(1024);  // TODO: Use larger keys after finishing the pairing process implementation
		KeyPair res = kpg.generateKeyPair();
		Log.d(TAG, "Keypair generated");
		return res;
	}
}
