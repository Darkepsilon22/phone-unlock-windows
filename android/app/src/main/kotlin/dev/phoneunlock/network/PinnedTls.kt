package dev.phoneunlock.network

import android.annotation.SuppressLint
import dev.phoneunlock.pairing.PairingProtocol
import java.net.InetAddress
import java.net.Socket
import java.security.MessageDigest
import java.security.Principal
import java.security.PrivateKey
import java.security.cert.CertificateException
import java.security.cert.X509Certificate
import javax.net.ssl.HostnameVerifier
import javax.net.ssl.SSLContext
import javax.net.ssl.SSLSession
import javax.net.ssl.SSLSocket
import javax.net.ssl.SSLSocketFactory
import javax.net.ssl.X509KeyManager
import javax.net.ssl.X509TrustManager

@SuppressLint("CustomX509TrustManager")
class PinnedServerTrust(private val pinnedFingerprint: ByteArray) : X509TrustManager {

    override fun checkServerTrusted(chain: Array<out X509Certificate>?, authType: String?) {
        val leaf = chain?.firstOrNull() ?: throw CertificateException("Aucun certificat présenté par le PC")
        val fingerprint = PairingProtocol.fingerprint(leaf.publicKey.encoded)
        if (!MessageDigest.isEqual(fingerprint, pinnedFingerprint)) {
            throw CertificateException("Ce n'est pas le PC attendu")
        }
    }

    override fun checkClientTrusted(chain: Array<out X509Certificate>?, authType: String?) {
        throw CertificateException("Le téléphone n'accepte pas de connexion entrante")
    }

    override fun getAcceptedIssuers(): Array<X509Certificate> = emptyArray()
}

object PinnedKeyReplacesHostnameCheck : HostnameVerifier {
    @SuppressLint("BadHostnameVerifier")
    override fun verify(hostname: String?, session: SSLSession?): Boolean = true
}

class ClientIdentity(private val key: PrivateKey, private val certificate: X509Certificate) : X509KeyManager {

    override fun chooseClientAlias(keyTypes: Array<out String>?, issuers: Array<out Principal>?, socket: Socket?): String = ALIAS

    override fun getCertificateChain(alias: String?): Array<X509Certificate>? = if (alias == ALIAS) arrayOf(certificate) else null

    override fun getPrivateKey(alias: String?): PrivateKey? = if (alias == ALIAS) key else null

    override fun getClientAliases(keyType: String?, issuers: Array<out Principal>?): Array<String> = arrayOf(ALIAS)

    override fun chooseServerAlias(keyType: String?, issuers: Array<out Principal>?, socket: Socket?): String? = null

    override fun getServerAliases(keyType: String?, issuers: Array<out Principal>?): Array<String>? = null

    private companion object {
        const val ALIAS = "transport"
    }
}

class Tls13SocketFactory(private val delegate: SSLSocketFactory) : SSLSocketFactory() {

    override fun getDefaultCipherSuites(): Array<String> = delegate.defaultCipherSuites

    override fun getSupportedCipherSuites(): Array<String> = delegate.supportedCipherSuites

    override fun createSocket(socket: Socket?, host: String?, port: Int, autoClose: Boolean): Socket =
        restrict(delegate.createSocket(socket, host, port, autoClose))

    override fun createSocket(host: String?, port: Int): Socket = restrict(delegate.createSocket(host, port))

    override fun createSocket(host: String?, port: Int, localHost: InetAddress?, localPort: Int): Socket =
        restrict(delegate.createSocket(host, port, localHost, localPort))

    override fun createSocket(host: InetAddress?, port: Int): Socket = restrict(delegate.createSocket(host, port))

    override fun createSocket(address: InetAddress?, port: Int, localAddress: InetAddress?, localPort: Int): Socket =
        restrict(delegate.createSocket(address, port, localAddress, localPort))

    private fun restrict(socket: Socket): Socket = socket.also { (it as SSLSocket).enabledProtocols = arrayOf(TLS_13) }

    companion object {
        const val TLS_13 = "TLSv1.3"

        fun create(identity: ClientIdentity, trust: PinnedServerTrust): Tls13SocketFactory {
            val context = SSLContext.getInstance(TLS_13).apply { init(arrayOf(identity), arrayOf(trust), null) }
            return Tls13SocketFactory(context.socketFactory)
        }
    }
}
