package cz.xoft.deskemes;

import android.support.test.InstrumentationRegistry;
import android.support.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.security.KeyPair;

import static org.junit.Assert.*;





@RunWith(AndroidJUnit4.class)
public class ApprovedPeerTest
{
	@Test
	public void storeAndQueryApproval()
	{
		// Initialize data:
		// byte[] remotePublicID  = new byte[] {0x41, 0x42, 0x43, 0x01, 0x02, 0x03, 0x0a, 0x0d, 0x0d, 0x0a, 0x41};  // "abc"
		byte[] remotePublicID  = new byte[] {0x41, 0x42, 0x43};
		byte[] remotePublicKey = new byte[] {0x44, 0x45, 0x46, 0x01, 0x02, 0x03, 0x0a, 0x0d, 0x0d, 0x0a, 0x42};  // "def"
		KeyPair localKeyPair;
		try
		{
			localKeyPair = ApprovedPeers.generateLocalKeyPair();
		}
		catch (Exception exc)
		{
			throw new AssertionError("Failed to generate local keypair", exc);
		}

		// Store the approval:
		ApprovedPeers ap = new ApprovedPeers(InstrumentationRegistry.getTargetContext());
		ApprovedPeers.Peer peer1;
		try
		{
			peer1 = ap.approvePeer("ApprovedPeerTest", remotePublicID, remotePublicKey, localKeyPair);
		}
		catch (Exception exc)
		{
			throw new AssertionError("Failed to store approval", exc);
		}

		// Read the approval:
		ApprovedPeers.Peer peer2 = ap.getApprovedPeer(remotePublicID);
		if (peer2 == null)
		{
			throw new AssertionError("Failed to read back the stored approval");
		}
		assertEquals     (peer1.mFriendlyName,        peer2.mFriendlyName);
		assertEquals     (peer1.mRemotePublicID,      peer2.mRemotePublicID);
		assertArrayEquals(peer1.mLocalPrivateKeyData, peer2.mLocalPrivateKeyData);
		assertArrayEquals(peer1.mLocalPublicKeyData,  peer2.mLocalPublicKeyData);
		assertArrayEquals(peer1.mRemotePublicKeyData, peer2.mRemotePublicKeyData);
	}
}
